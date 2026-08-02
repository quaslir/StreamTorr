
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
#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
#include <mutex>
#include <optional>
#include <thread>
namespace {
std::string strip_ass_tags(const char* ass_line) {
    if (!ass_line) return {};
    std::string input(ass_line);

    // ASS-строка имеет формат: "Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text"
    // нам нужно только поле Text — это всё после 9-й запятой
    int commas_seen = 0;
    size_t text_start = 0;
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == ',') {
            commas_seen++;
            if (commas_seen == 9) {
                text_start = i + 1;
                break;
            }
        }
    }
    std::string text_field = (text_start > 0) ? input.substr(text_start) : input;

    // вырезаем теги форматирования {\...}
    std::string result;
    result.reserve(text_field.size());
    bool inside_tag = false;
    for (char c : text_field) {
        if (c == '{') { inside_tag = true; continue; }
        if (c == '}') { inside_tag = false; continue; }
        if (inside_tag) continue;
        if (c == '\\' ) continue;  // ASS также использует \N для переноса строки вне тегов иногда
        result.push_back(c);
    }

    // \N и \n внутри ASS означают перенос строки — заменим на пробел для простоты (или на '\n', если хочешь многострочность)
    size_t pos;
    while ((pos = result.find("N")) != std::string::npos && pos > 0 && result[pos-1] == '\\') {
        result.replace(pos - 1, 2, " ");
    }

    return result;
}
} // anonymous namespace
Pipeline::Pipeline() : video_queue_(video_queue_size), audio_queue_(audio_queue_size) {}

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

    if(demuxer_.has_subtitles()) {
        auto subtitle_info = demuxer_.subtitle_stream_info();
        if(subtitle_info.has_value()) {
           if(subtitle_decoder_.init(*subtitle_info)) {
               subtitle_decoder_ready_ = true;
           }
        }
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
    torrent_client_.set_progress_callback(progress_cb_);
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
    std::lock_guard<std::mutex> subtitle_lock(subtitle_mutex_);
    subtitle_events_.clear();
    return true;
}

void Pipeline::decode_video_packet(const AVPacket *packet) {
    std::vector<smart_frame> ready_frames;

    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
        video_decoder_.send_packet(packet);

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
    std::vector<smart_frame> ready_frames;

    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
        audio_decoder_.send_packet(packet);

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

void Pipeline:: decode_subtitle_packet(const AVPacket* packet) {
    if(!subtitle_decoder_ready_) return;
std::optional<smart_subtitle> sub;
{
    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    sub = subtitle_decoder_.decode(packet);
}

if(!sub.has_value()) return;

AVSubtitle * raw = sub->get();

double pts_seconds = static_cast<double>(packet->pts) * av_q2d(demuxer_.subtitle_time_base());
double start = pts_seconds +  static_cast<double>(raw->start_display_time) / 1000.0;
double end = pts_seconds +  static_cast<double>(raw->end_display_time) / 1000.0;


for(unsigned int i = 0; i < raw->num_rects; i++) {
    AVSubtitleRect* rect = raw->rects[i];
    std::string text;

    if(rect->ass) text = strip_ass_tags(rect->ass);
    else if(rect->text) text = rect->text;
    if(text.empty()) continue;

    std::lock_guard<std::mutex> lock(subtitle_mutex_);
    subtitle_events_.push_back({start, end, text});
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
        case PacketType::SUBTITLE: {
            decode_subtitle_packet(packet->packet.get());
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

std::optional<std::string> Pipeline::current_subtitle_text() const {
    double time = clock_.get_time();
    std::lock_guard<std::mutex> lock(subtitle_mutex_);

   for(const auto& ev : subtitle_events_) {
       if(time >= ev.start_time && time <= ev.end_time) {
           return ev.text;
       }
   }
return std::nullopt;
}

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
