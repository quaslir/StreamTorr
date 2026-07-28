
#include "pipeline/pipeline.hpp"
#include "decoder/demuxer.hpp"
#include "decoder/smart_items.hpp"
#include "decoder/types.hpp"
#include "decoder/video_decoder.hpp"
#include "media/video_resampler.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <libavutil/pixfmt.h>
#include <mutex>
#include <thread>

Pipeline::Pipeline() : video_queue_(10), audio_queue_(30) {}

bool Pipeline::open(const std::string& filename) {
bool demuxer_open = demuxer_.open(filename);

if(!demuxer_open) return false;

if(demuxer_.has_video()) {
    auto video_info = demuxer_.video_stream_info();
    if(!video_info.has_value()) return false;

    bool video_decoder_open = video_decoder_.init(video_info.value());

    if(!video_decoder_open) return false;
}

if(demuxer_.has_audio()) {
    auto audio_info = demuxer_.audio_stream_info();
    if(!audio_info.has_value()) return false;

    bool audio_decoder_open = audio_decoder_.init(audio_info.value());

    if(!audio_decoder_open) return false;
    if(!audio_resampler_.open(audio_decoder_.get_codec_context())) return false;
}



return true;
}


void Pipeline::set_progress_callback(ProgressCallback cb) {
    progress_cb_ = cb;
    torrent_client_.set_progress_callback(progress_cb_);
}
bool Pipeline::open_torrent(const std::string& magnet, const std::filesystem::path& download_dir) {

    if(!torrent_client_.add_source(magnet, download_dir)) return false;

    int waited = 0;

    while(!torrent_client_.has_metadata()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if(progress_cb_) {
            progress_cb_({TorrentStage::FetchingMetadata, std::min(1.0f,waited / 180.0f)});
        }
        if(++waited > 180) {
            return false;
        }
    }
    if(progress_cb_) progress_cb_({TorrentStage::FetchingMetadata, 1.0f});

    auto file_info = torrent_client_.video_file_info();
    if(!file_info.has_value()) {
        return false;
    }
    constexpr uint64_t kInitialWindowBytes = 4 * 1024 * 1024;
    torrent_client_.prioritize_range(static_cast<uint64_t>(file_info->offset_in_torrent), kInitialWindowBytes);

    constexpr uint64_t kTailWindowBytes = 4 * 1024 * 1024;
    uint64_t tail_start = static_cast<uint64_t>(file_info->offset_in_torrent + file_info->size) - std::min(kTailWindowBytes, static_cast<uint64_t>(file_info->size));
    torrent_client_.prioritize_range(tail_start, kTailWindowBytes);

    int head_waited = 0;
    while(torrent_client_.window_progress(static_cast<uint64_t>((file_info->offset_in_torrent)), kInitialWindowBytes) < 1.0f){
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if(progress_cb_) {
                progress_cb_({TorrentStage::DownloadingHeadTail, torrent_client_.window_progress(static_cast<uint64_t>((file_info->offset_in_torrent)), kInitialWindowBytes)});

            }
            if(++head_waited > 200) break;
    }
    if(progress_cb_) progress_cb_({TorrentStage::Ready, 1.0f});

    if(!io_context_.open(&torrent_client_, file_info->path, file_info->offset_in_torrent, file_info->size)) return false;


    if(!demuxer_.open_with_io_context(io_context_.avio_context())) return false;


    if(demuxer_.has_video()) {
        auto video_info = demuxer_.video_stream_info();
        if(!video_info.has_value()) return false;

        bool video_decoder_open = video_decoder_.init(video_info.value());

        if(!video_decoder_open) return false;


    }

    if(demuxer_.has_audio()) {
        auto audio_info = demuxer_.audio_stream_info();
        if(!audio_info.has_value()) return false;

        bool audio_decoder_open = audio_decoder_.init(audio_info.value());

        if(!audio_decoder_open) return false;
        if(!audio_resampler_.open(audio_decoder_.get_codec_context())) return false;
    }

    return true;

}


void Pipeline::start() {
    running_ = true;
    demux_thread_ = std::thread(&Pipeline::demux_loop, this);
}
void Pipeline::stop() {
    running_ = false;
    video_queue_.close();
    audio_queue_.close();
    if(demux_thread_.joinable()) demux_thread_.join();
}

bool Pipeline::seek(double seconds) {
    video_queue_.clear();
    audio_queue_.clear();

    std::lock_guard<std::mutex> lock(pipeline_mutex_);
    if(!demuxer_.seek(seconds)) return false;
    video_decoder_.flush();
    audio_decoder_.flush();
    video_queue_.clear();
    audio_queue_.clear();
    return true;
}

void Pipeline::decode_video_packet(const AVPacket* packet) {
    DecoderSendResult result = DecoderSendResult::Error;
    (void)result;
    std::vector<smart_frame> ready_frames;

    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
                result =  video_decoder_.send_packet(packet);

                while(auto frame = video_decoder_.receive_frame()) {
                    ready_frames.push_back(std::move(*frame));
                }
    }

    for(auto& frame : ready_frames) {
        if(!video_resampler_ready_) {
            AVPixelFormat real_format = static_cast<AVPixelFormat>(frame->format);
            if(!video_resampler_.open(frame->width, frame->height, real_format)) continue;
            video_resampler_ready_ = true;
        }

        auto resampled = video_resampler_.convert(frame.get());
        if(resampled.has_value()) {
            video_queue_.push(std::move(*resampled));
        }
    }



}
void Pipeline::decode_audio_packet(const AVPacket* packet) {
    DecoderSendResult result = DecoderSendResult::Error;
    (void)result;
    std::vector<smart_frame> ready_frames;

    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
                result =  audio_decoder_.send_packet(packet);

                while(auto frame = audio_decoder_.receive_frame()) {
                    ready_frames.push_back(std::move(*frame));
                }
    }

    for(auto& frame : ready_frames) {

        auto resampled = audio_resampler_.convert(frame.get());
        if(resampled.has_value()) {
            audio_queue_.push(std::move(*resampled));
        }
    }
}


void Pipeline::demux_loop() {
    while(running_) {
        std::optional<DemuxedPacket> packet;
    {
        std::lock_guard<std::mutex> lock(pipeline_mutex_);
           packet =  demuxer_.read_next_packet();
    }

       if(!packet.has_value()) {
           video_queue_.close();
           audio_queue_.close();
           break;
       }

       switch(packet->type) {
           case PacketType::VIDEO: {

               decode_video_packet(packet->packet.get());
           break;
           }

           case PacketType::AUDIO : {
           decode_audio_packet(packet->packet.get());
           break;
           }

           case PacketType::OTHER :
           break;

           case PacketType::ERROR :
           break;

       }


    }
}

FrameQueue<smart_frame>& Pipeline::video_frames() {
    return video_queue_;
}
FrameQueue<smart_frame>& Pipeline::audio_frames() {
return audio_queue_;
}

Clock& Pipeline::clock()  {
    return clock_;
}

AVRational Pipeline::audio_time_base() const {
    return demuxer_.audio_time_base();
}

AVRational Pipeline::video_time_base() const {
    return demuxer_.video_time_base();
}

std::optional<std::pair<int, int>> Pipeline::video_stream_size() const {
    return demuxer_.video_stream_size();
}

bool Pipeline::has_audio() const {
    return demuxer_.has_audio();
}
