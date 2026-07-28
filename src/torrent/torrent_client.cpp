#include "torrent/torrent_client.hpp"
#include <chrono>
#include <cstdint>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/download_priority.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/units.hpp>
#include <mutex>
#include <thread>

TorrentClient::TorrentClient() : session_(make_default_settings()) {}

bool TorrentClient::add_source(const std::string& magnet, const std::filesystem::path& download_dir) {
    if(source_added_) return false;
        lt::add_torrent_params params;

    try {
        params = lt::parse_magnet_uri(magnet);
    } catch(...) {

        return false;}

    params.save_path = download_dir.string();

    handle_ = session_.add_torrent(params);

    if(!handle_.is_valid()) {
            return false;
    }

    source_added_ = true;

    download_dir_ = download_dir;
    running_ = true;
    alert_thread_= std::thread(&TorrentClient::alert_loop, this);

    return true;
}



float TorrentClient::window_progress(uint64_t offset, uint64_t length) const {
    if(!handle_.is_valid()) return 0.0f;
    auto torrent_info = handle_.torrent_file();
    if(!torrent_info) return 0.0f;
    int64_t piece_length = torrent_info->piece_length();
    int first = static_cast<int>(offset / static_cast<uint64_t>(piece_length));
    int last = static_cast<int>((offset + length - 1) / static_cast<uint64_t>(piece_length));
    auto status = handle_.status(lt::torrent_handle::query_pieces);
    int total = last - first + 1;
    int have = 0;

    for(int i = first; i <= last && i < static_cast<int>(status.pieces.size()); i++) {
        if(status.pieces[lt::piece_index_t(i)]) have++;
    }

    return total > 0 ? static_cast<float>(have) / static_cast<float>(total) : 1.0f;
}

void TorrentClient::set_progress_callback(ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(progress_cb_mutex_);
    progress_cb_ = cb;
}

lt::torrent_status TorrentClient::status() const {
    return handle_.status();
}

lt::settings_pack TorrentClient::make_default_settings()  {
    lt::settings_pack settings;
    settings.set_bool(lt::settings_pack::enable_dht, true);
    settings.set_int(lt::settings_pack::connections_limit, 200);
    settings.set_int(lt::settings_pack::download_rate_limit, 0);
    settings.set_int(lt::settings_pack::request_timeout, 5);
    settings.set_int(lt::settings_pack::active_downloads, 1);
    settings.set_int(lt::settings_pack::unchoke_slots_limit, 20);
    return settings;
}

bool TorrentClient::has_metadata() const {
    return handle_.is_valid() && handle_.status().has_metadata;
}
std::optional<VideoFileInfo> TorrentClient::video_file_info()const {
    if(!has_metadata()) return std::nullopt;

    auto torrent_info = handle_.torrent_file();
    const lt::file_storage& files = torrent_info->files();

    int video_index = -1;
    int64_t max_size = 0;

    for(int i = 0; i < files.num_files();i++) {
        if(files.file_size(static_cast<libtorrent::file_index_t>(i)) > max_size) {
            max_size = files.file_size(static_cast<libtorrent::file_index_t>(i));
            video_index = i;
        }
    }

    if(video_index < 0) return std::nullopt;

    return VideoFileInfo{download_dir_ / files.file_path(static_cast<libtorrent::file_index_t>(video_index)),
        files.file_offset(static_cast<libtorrent::file_index_t>(video_index)),files.file_size(static_cast<libtorrent::file_index_t>(video_index))};
}

bool TorrentClient::is_range_available(uint64_t offset, uint64_t length) const {
    if(!handle_.is_valid()) return false;

    auto torrent_info = handle_.torrent_file();
    if(!torrent_info) return false;
    int64_t piece_length = torrent_info->piece_length();

    lt::piece_index_t first_piece{static_cast<int>(offset / static_cast<uint64_t>(piece_length))};
    lt::piece_index_t last_piece{static_cast<int>((offset + length - 1) / static_cast<uint64_t>(piece_length))};
    auto status = handle_.status(lt::torrent_handle::query_pieces);

    for(int i = static_cast<int>(first_piece); i <= static_cast<int>(last_piece); i++) {
        if(i >= static_cast<int>(status.pieces.size()) || !status.pieces[lt::piece_index_t(i)]) {
            return false;
        }
    }

    return true;

}

void TorrentClient::apply_priority(uint64_t offset, uint64_t length) {
    if(!handle_.is_valid()) return;

    auto torrent_info = handle_.torrent_file();
    if(!torrent_info) return;
    int num_pieces = torrent_info->num_pieces();
    int64_t piece_length = torrent_info->piece_length();
    int first_piece{static_cast<int>(offset / static_cast<uint64_t>(piece_length))};
    int last_piece{static_cast<int>((offset + length - 1) / static_cast<uint64_t>(piece_length))};
    last_piece = std::min(last_piece, num_pieces - 1);
    if(first_piece > last_piece) return;

    for(int i = first_piece; i <= last_piece; i++) {
        handle_.piece_priority(lt::piece_index_t(i), lt::top_priority);

        handle_.set_piece_deadline(lt::piece_index_t(i), 1000);
    }
}

void TorrentClient::prioritize_range(uint64_t offset, uint64_t length) {
{
    std::lock_guard<std::mutex> lock(active_window_.mutex_);
active_window_.offset_ = offset;
active_window_.length_ = length;
active_window_.set_ = true;
}

apply_priority(offset, length);
}
bool TorrentClient::wait_for_range(uint64_t offset, uint64_t length, uint32_t timeout_ms) const {
    std::unique_lock<std::mutex> lock(mutex_);
    return piece_downloaded_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this, offset, length] {
            return is_range_available(offset, length);
        });
}

void TorrentClient::alert_loop() {
    int tick = 0;
    while(running_) {
        std::vector<lt::alert*> alerts;
        session_.pop_alerts(&alerts);
        for(auto * alert : alerts) {
            if(lt::alert_cast<lt::piece_finished_alert>(alert)) {
                piece_downloaded_cv_.notify_all();
            }

            if(lt::alert_cast<lt::metadata_received_alert>(alert)) {
                std::lock_guard<std::mutex> lock(progress_cb_mutex_);
                if(progress_cb_) progress_cb_({TorrentStage::FetchingMetadata, 1.0f});
            }
        }
        if(++tick % 10 == 0) {
            auto s = handle_.status();
            std::fprintf(stderr, "[RATE] down=%d KB/s peers=%d\n", s.download_rate / 1024, s.num_peers);

            uint64_t offset, length;
            bool have_window;
        {
            std::lock_guard<std::mutex> lock(active_window_.mutex_);
            have_window = active_window_.set_;
            offset = active_window_.offset_ ;
            length = active_window_.length_;
        }

        if(have_window) {
            apply_priority(offset, length);
        }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TorrentClient::~TorrentClient() {
    running_ = false;
    if(alert_thread_.joinable()) alert_thread_.join();
}
