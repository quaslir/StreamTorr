#pragma once
extern "C" {
    #include <libavcodec/avcodec.h>


#include <libavformat/avformat.h>
}
#include <memory>
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* codec_context) {
        avcodec_free_context(&codec_context);
    }
};

struct AVFrameDeleter {
    void operator() (AVFrame * frame) {
        if(frame) {
            av_frame_free(&frame);
        }
    }
};


struct AVFormatContextDeleter {
    void operator ()(AVFormatContext * ctx) const{
        if(ctx) {
            avformat_close_input(&ctx);
        }
    }
};




struct AVPacketDeleter {
    void operator ()(AVPacket * packet) const{
        if(packet) {
            av_packet_free(&packet);
        }
    }
};




using smart_codec_context = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using smart_frame = std::unique_ptr<AVFrame, AVFrameDeleter>;
using smart_format_context = std::unique_ptr<AVFormatContext,AVFormatContextDeleter >;
using smart_packet = std::unique_ptr<AVPacket, AVPacketDeleter>;
