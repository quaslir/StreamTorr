#include "decoder/demuxer.hpp"
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

Demuxer::Demuxer() : format_context(avformat_alloc_context()) {}

bool Demuxer::open(const std::string& filename) {
    AVFormatContext* raw_ctx = format_context.release();

if(avformat_open_input(&raw_ctx, filename.c_str(), nullptr, nullptr) < 0) {
    return false;
}
format_context.reset(raw_ctx);
if(avformat_find_stream_info(format_context.get(), nullptr) < 0) {
    return false;
}

int video_index = av_find_best_stream(format_context.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

if(video_index < 0) return false;

video_stream_index = video_index;


return true;

}
