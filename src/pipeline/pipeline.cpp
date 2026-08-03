
#include "pipeline/pipeline.hpp"
#include "configuration/config.hpp"
#include "decoder/demuxer.hpp"
#include "decoder/smart_items.hpp"
#include "decoder/types.hpp"
#include "decoder/video_decoder.hpp"
#include "media/video_resampler.hpp"
#include "torrent/types.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>
Pipeline::Pipeline() : video_queue_(300), audio_queue_(600) {}

bool Pipeline::open() {

    if (demuxer_.has_video()) {
        auto video_info = demuxer_.video_stream_info();
        if (!video_info.has_value())
            return false;

        bool video_decoder_open = video_decoder_.init(video_info.value());

        if (!video_decoder_open)
            return false;
    }

    if (demuxer_.has_audio()) {
        auto audio_info = demuxer_.audio_stream_info();
        if (!audio_info.has_value())
            return false;

        bool audio_decoder_open = audio_decoder_.init(audio_info.value());

        if (!audio_decoder_open)
            return false;
        if (!audio_resampler_.open(audio_decoder_.get_codec_context()))
            return false;
    }

    return true;
}

bool Pipeline::open_local(const std::string &filename) {
    bool demuxer_open = demuxer_.open(filename);
    if (!demuxer_open)
        return false;
    return open();
}

bool Pipeline::open_torrent(const std::string &magnet, const std::filesystem::path &download_dir) {

    if (!torrent_client_.add_source(magnet, download_dir))
        return false;

    int waited = 0;

    while (!torrent_client_.has_metadata()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (progress_cb_) {
            progress_cb_({TorrentStage::FetchingMetadata,
                          std::min(1.0f, static_cast<float>(waited) / 180.0f)});
        }
        if (++waited > 180) {
            return false;
        }
    }
    if (progress_cb_)
        progress_cb_({TorrentStage::FetchingMetadata, 1.0f});

    auto file_info = torrent_client_.video_file_info();
    if (!file_info.has_value()) {
        return false;
    }
    file_offset_in_torrent_ = file_info->offset_in_torrent;
    file_size_ = file_info->size;
    is_torrent_ = true;
    torrent_client_.prioritize_range(static_cast<uint64_t>(file_info->offset_in_torrent),
                                     kInitialWindowBytes);

    uint64_t tail_start = static_cast<uint64_t>(file_info->offset_in_torrent + file_info->size) -
                          std::min(kTailWindowBytes, static_cast<uint64_t>(file_info->size));
    torrent_client_.prioritize_range(tail_start, kTailWindowBytes);

    int head_waited = 0;
    float head_progress = 0.0f;
    while ((head_progress = torrent_client_.window_progress(
                static_cast<uint64_t>((file_info->offset_in_torrent)), kInitialWindowBytes)) <
           1.0f) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (progress_cb_) {
            progress_cb_({TorrentStage::DownloadingHeadTail, head_progress});
        }
        if (++head_waited > 200)
            break;
    }
    if (progress_cb_)
        progress_cb_({TorrentStage::DownloadingHeadTail, head_progress});
    if (progress_cb_)
        progress_cb_({TorrentStage::OpeningStream, 0.0f});

    if (!io_context_.open(&torrent_client_, file_info->path, file_info->offset_in_torrent,
                          file_info->size))
        return false;

    if (!demuxer_.open_with_io_context(io_context_.avio_context()))
        return false;

    io_context_.set_reporting(false);

    if (!open())
        return false;
    if (progress_cb_)
        progress_cb_({TorrentStage::Ready, 1.0f});
    return true;
}
void Pipeline::set_progress_callback(ProgressCallback cb) {
    progress_cb_ = cb;
    io_context_.set_progress_callback(progress_cb_);
}

void Pipeline::start() {
    running_ = true;
    demux_thread_ = std::thread(&Pipeline::demux_loop, this);
}
void Pipeline::stop() {
    running_ = false;
    video_queue_.close();
    audio_queue_.close();
    if (demux_thread_.joinable())
        demux_thread_.join();
}

bool Pipeline::seek(double seconds) {
    torrent_client_.set_abort_wait(true);
    if (is_torrent_) {
        double duration = demuxer_.duration_seconds();
        if (duration > 0.0) {
            double ratio = std::clamp(seconds / duration, 0.0, 1.0);
            uint64_t target = static_cast<uint64_t>(file_offset_in_torrent_) +
                              static_cast<uint64_t>(ratio * static_cast<double>(file_size_));
            torrent_client_.prioritize_range(target, kPriorityWindowBytes);
        }
    }
    video_queue_.clear();
    audio_queue_.clear();

    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    torrent_client_.set_abort_wait(false);
    if (!demuxer_.seek(seconds))
        return false;
    video_decoder_.flush();
    if (demuxer_.has_audio()) {
        audio_decoder_.flush();
    }

    latest_video_pts_seconds_.store(seconds, std::memory_order_relaxed);
    video_queue_.clear();
    audio_queue_.clear();
    return true;
}

void Pipeline::decode_video_packet(const AVPacket *packet) {
    DecoderSendResult result = DecoderSendResult::Error;
    (void)result;
    std::vector<smart_frame> ready_frames;

    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
        result = video_decoder_.send_packet(packet);

        while (auto frame = video_decoder_.receive_frame()) {
            ready_frames.push_back(std::move(*frame));
        }
    }

    for (auto &frame : ready_frames) {
        if (!video_resampler_ready_) {
            AVPixelFormat real_format = static_cast<AVPixelFormat>(frame->format);
            if (!video_resampler_.open(frame->width, frame->height, real_format))
                continue;
            video_resampler_ready_ = true;
        }

        auto resampled = video_resampler_.convert(frame.get());
        if (resampled.has_value()) {
            double pts_seconds =
                static_cast<double>(resampled.value()->pts) * av_q2d(demuxer_.video_time_base());
            latest_video_pts_seconds_.store(pts_seconds, std::memory_order_relaxed);
            video_queue_.push(std::move(*resampled));
        }
    }
}
void Pipeline::decode_audio_packet(const AVPacket *packet) {
    DecoderSendResult result = DecoderSendResult::Error;
    (void)result;
    std::vector<smart_frame> ready_frames;

    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
        result = audio_decoder_.send_packet(packet);

        while (auto frame = audio_decoder_.receive_frame()) {
            ready_frames.push_back(std::move(*frame));
        }
    }

    for (auto &frame : ready_frames) {

        auto resampled = audio_resampler_.convert(frame.get());
        if (resampled.has_value()) {
            audio_queue_.push(std::move(*resampled));
        }
    }
}

void Pipeline::demux_loop() {
    while (running_) {
        std::optional<DemuxedPacket> packet;
        {
            std::lock_guard<std::mutex> lock(pipeline_mutex_);
            packet = demuxer_.read_next_packet();
        }

        if (!packet.has_value()) {
            video_queue_.close();
            audio_queue_.close();
            break;
        }

        switch (packet->type) {
        case PacketType::VIDEO: {

            decode_video_packet(packet->packet.get());
            break;
        }

        case PacketType::AUDIO: {
            decode_audio_packet(packet->packet.get());
            break;
        }

        case PacketType::OTHER:
            break;

        case PacketType::ERROR:
            break;
        }
    }
}

FrameQueue<smart_frame> &Pipeline::video_frames() { return video_queue_; }
FrameQueue<smart_frame> &Pipeline::audio_frames() { return audio_queue_; }

Clock &Pipeline::clock() { return clock_; }

AVRational Pipeline::audio_time_base() const { return demuxer_.audio_time_base(); }

AVRational Pipeline::video_time_base() const { return demuxer_.video_time_base(); }

std::optional<std::pair<int, int>> Pipeline::video_stream_size() const {
    return demuxer_.video_stream_size();
}

bool Pipeline::has_audio() const { return demuxer_.has_audio(); }

double Pipeline::buffered_seconds() const {
    return latest_video_pts_seconds_.load(std::memory_order_relaxed) - clock_.get_time();
}

double Pipeline::duration_seconds() const { return demuxer_.duration_seconds(); }

float Pipeline::overall_progress() const { return torrent_client_.overall_progress(); }
