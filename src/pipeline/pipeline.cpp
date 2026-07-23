
#include "pipeline/pipeline.hpp"
#include "decoder/demuxer.hpp"
#include "decoder/video_decoder.hpp"
#include <thread>

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
        video_queue_.push(std::move(*frame));
    }

    if(result == DecoderSendResult::NeedsMoreOutput) {
        video_decoder_.send_packet(packet);

        while(auto frame = video_decoder_.receive_frame()) {
            video_queue_.push(std::move(*frame));
        }
    }
}
void Pipeline::decode_audio_packet(const AVPacket* packet) {
    DecoderSendResult result =  audio_decoder_.send_packet(packet);

    while(auto frame = audio_decoder_.receive_frame()) {
        audio_queue_.push(std::move(*frame));
    }

    if(result == DecoderSendResult::NeedsMoreOutput) {
        audio_decoder_.send_packet(packet);

        while(auto frame = audio_decoder_.receive_frame()) {
            audio_queue_.push(std::move(*frame));
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
