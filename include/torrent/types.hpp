#pragma once
#include <filesystem>
#include <functional>
#include <mutex>
enum class TorrentStage { FetchingMetadata, DownloadingHeadTail, OpeningStream, Ready };

struct TorrentProgress {
    TorrentStage stage;
    float percent;
};

using ProgressCallback = std::function<void(TorrentProgress)>;

struct VideoFileInfo {
    std::filesystem::path path;
    int64_t offset_in_torrent;
    int64_t size;
};

struct ActiveWindow {
    mutable std::mutex mutex_;
    uint64_t offset_;
    uint64_t length_;
    bool set_{false};
};
