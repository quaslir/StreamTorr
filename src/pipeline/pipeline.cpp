
#include "pipeline/pipeline.hpp"
#include "decoder/demuxer.hpp"
#include "decoder/video_decoder.hpp"
#include "media/video_resampler.hpp"
#include <chrono>
#include <cstdint>
#include <libavutil/pixfmt.h>
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
    AVPixelFormat src_format = video_decoder_.get_codec_context()->pix_fmt;
    if(!video_resampler_.open(video_info.value()->width, video_info.value()->height, src_format)) return false;

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


bool Pipeline::open_torrent(const std::string& magnet, const std::filesystem::path& download_dir) {
    if(!torrent_client_.add_source(magnet, download_dir)) return false;

    int waited = 0;

    while(!torrent_client_.has_metadata()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if(++waited > 180) {
            return false;
        }
    }

    auto file_info = torrent_client_.video_file_info();
    if(!file_info.has_value()) {
        return false;
    }
    constexpr uint64_t kInitialWindowBytes = 16 * 1024 * 1024;
    torrent_client_.prioritize_range(static_cast<uint64_t>(file_info->offset_in_torrent), kInitialWindowBytes);

    constexpr uint64_t kTailWindowBytes = 16 * 1024 * 1024;
    uint64_t tail_start = static_cast<uint64_t>(file_info->offset_in_torrent + file_info->size) - std::min(kTailWindowBytes, static_cast<uint64_t>(file_info->size));
    torrent_client_.prioritize_range(tail_start, kTailWindowBytes);
    if(!io_context_.open(&torrent_client_, file_info->path, file_info->offset_in_torrent, file_info->size)) return false;
    if(!demuxer_.open_with_io_context(io_context_.avio_context())) return false;

    if(demuxer_.has_video()) {
        auto video_info = demuxer_.video_stream_info();
        if(!video_info.has_value()) return false;

        bool video_decoder_open = video_decoder_.init(video_info.value());

        if(!video_decoder_open) return false;
        AVPixelFormat src_format = video_decoder_.get_codec_context()->pix_fmt;

        if(!video_resampler_.open(video_info.value()->width, video_info.value()->height, src_format)) return false;

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

void Pipeline::decode_video_packet(const AVPacket* packet) {
    DecoderSendResult result =  video_decoder_.send_packet(packet);

    while(auto frame = video_decoder_.receive_frame()) {
        auto resampled_frame = video_resampler_.convert(frame->get());
        if(resampled_frame.has_value()) {
        video_queue_.push(std::move(*resampled_frame));
        }
    }

    if(result == DecoderSendResult::NeedsMoreOutput) {
        video_decoder_.send_packet(packet);

        while(auto frame = video_decoder_.receive_frame()) {
            auto resampled_frame = video_resampler_.convert(frame->get());
            if(resampled_frame.has_value()) {
            video_queue_.push(std::move(*resampled_frame));
            }
        }
    }
}
void Pipeline::decode_audio_packet(const AVPacket* packet) {
    DecoderSendResult result =  audio_decoder_.send_packet(packet);

    while(auto frame = audio_decoder_.receive_frame()) {
        auto resampled_frame = audio_resampler_.convert(frame->get());
        if(resampled_frame.has_value()) {
        audio_queue_.push(std::move(*resampled_frame));
        }
    }

    if(result == DecoderSendResult::NeedsMoreOutput) {
        audio_decoder_.send_packet(packet);

        while(auto frame = audio_decoder_.receive_frame()) {
            auto resampled_frame = audio_resampler_.convert(frame->get());
            if(resampled_frame.has_value()) {
            audio_queue_.push(std::move(*resampled_frame));
            }
        }
    }
}


void Pipeline::demux_loop() {
    while(running_) {
       auto packet =  demuxer_.read_next_packet();

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
           case PacketType::AUDIO :
           decode_audio_packet(packet->packet.get());
           break;

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
