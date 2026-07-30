#pragma once

#include "torrent/types.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
extern "C" {
#include <libavformat/avio.h>
}
#include <atomic>

class TorrentClient;

class TorrentIOContext {
  public:
    bool open(TorrentClient *client, const std::filesystem::path &file_path,
              int64_t file_offset_in_torrent, int64_t total_size);
    void set_progress_callback(ProgressCallback cb);
    void set_reporting(bool on = true);

    AVIOContext *avio_context() const;
    TorrentIOContext() = default;
    ~TorrentIOContext();

    TorrentIOContext(const TorrentIOContext &) = delete;
    TorrentIOContext &operator=(const TorrentIOContext &) = delete;

  private:
    static int read_packet_callback(void *opaque, uint8_t *buf, int buf_size);
    static int64_t seek_callback(void *opaque, int64_t offset, int whence);

    int read_packet(uint8_t *buf, int buf_size);
    int64_t seek(int64_t offset, int whence);

    TorrentClient *client_{nullptr};
    std::filesystem::path path_;
    std::ifstream file_stream_;

    int64_t file_offset_in_torrent_{0};
    int64_t current_position_{0};
    int64_t total_size_{0};

    uint8_t *avio_buffer_{nullptr};
    AVIOContext *avio_context_{nullptr};

    int64_t last_prioritized_pos_{-1};

    ProgressCallback progress_cb_;
    std::atomic<bool> reporting_{true};
    float max_reported_progress{0.0f};
};
