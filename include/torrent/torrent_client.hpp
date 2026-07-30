#include "types.hpp"
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <libtorrent/libtorrent.hpp>
#include <libtorrent/peer_list.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_status.hpp>
#include <mutex>
#include <optional>
#include <thread>

class TorrentClient {
  private:
    lt::session session_;
    lt::torrent_handle handle_;
    std::filesystem::path download_dir_;
    std::filesystem::path target_file_path_;

    std::thread alert_thread_;

    std::atomic<bool> running_{false};
    std::atomic<bool> abort_wait_{false};
    ProgressCallback progress_cb_;
    mutable std::mutex mutex_;
    mutable std::mutex progress_cb_mutex_;

    mutable std::condition_variable piece_downloaded_cv_;
    ActiveWindow active_window_;
    bool source_added_{false};

    static lt::settings_pack make_default_settings();
    void apply_priority(uint64_t offset, uint64_t length);

  public:
    TorrentClient();
    ~TorrentClient();
    bool add_source(const std::string &magnet, const std::filesystem::path &download_dir);
    float window_progress(uint64_t offset, uint64_t length) const;
    void set_progress_callback(ProgressCallback cb);
    void set_abort_wait(bool status);
    lt::torrent_status status() const;
    bool has_metadata() const;
    std::optional<VideoFileInfo> video_file_info() const;

    bool is_range_available(uint64_t offset, uint64_t length) const;
    void prioritize_range(uint64_t offset, uint64_t length);
    bool wait_for_range(uint64_t offset, uint64_t length, uint32_t timeout_ms) const;
    void alert_loop();
    float overall_progress() const;
};
