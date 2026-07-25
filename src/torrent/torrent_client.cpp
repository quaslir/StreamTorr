#include "torrent/torrent_client.hpp"
#include <arm_neon.h>
#include <charconv>
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
#include <thread>

TorrentClient::TorrentClient() : session_(make_default_settings()) {}

bool TorrentClient::add_source(const std::string& magnet, const std::filesystem::path& download_dir) {
    if(source_added_) return false;
        lt::add_torrent_params params;

    try {
        params = lt::parse_magnet_uri(magnet);
    } catch(...) {return false;}

    params.save_path = download_dir.string();

    handle_ = session_.add_torrent(params);

    if(!handle_.is_valid()) return false;

    source_added_ = true;

    download_dir_ = download_dir;
    alert_thread_= std::thread(&TorrentClient::alert_loop, this);

    return true;
}

lt::torrent_status TorrentClient::status() const {
    return handle_.status();
}

lt::settings_pack TorrentClient::make_default_settings() const {
    lt::settings_pack settings;
    settings.set_bool(lt::settings_pack::enable_dht, true);
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
void TorrentClient::prioritize_range(uint64_t offset, uint64_t length) {
    if(!handle_.is_valid()) return;

    auto torrent_info = handle_.torrent_file();
    if(!torrent_info) return;

    int64_t piece_length = torrent_info->piece_length();
    int first_piece{static_cast<int>(offset / static_cast<uint64_t>(piece_length))};
    int last_piece{static_cast<int>((offset + length - 1) / static_cast<uint64_t>(piece_length))};

    for(int i = first_piece; i <= last_piece; i++) {
        handle_.piece_priority(lt::piece_index_t(i), lt::top_priority);
    }

    int deadline_count =    std::min(3, (last_piece - first_piece + 1));

    for(int i = 0; i < deadline_count; i++) {
        handle_.set_piece_deadline(lt::piece_index_t(first_piece + i), 1000);
    }


}
bool TorrentClient::wait_for_range(uint64_t offset, uint64_t length, uint32_t timeout_ms) const {
    std::unique_lock<std::mutex> lock(mutex_);
    return piece_downloaded_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this, offset, length] {
            return is_range_available(offset, length);
        });
}

void TorrentClient::alert_loop() {
    while(running) {
        std::vector<lt::alert*> alerts;
        session_.pop_alerts(&alerts);
        for(auto * alert : alerts) {
            if(lt::alert_cast<lt::piece_finished_alert>(alert)) {
                piece_downloaded_cv_.notify_all();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TorrentClient::~TorrentClient() {
    running = false;
    if(alert_thread_.joinable()) alert_thread_.join();
}
