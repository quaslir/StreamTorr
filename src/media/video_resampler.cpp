#include "media/video_resampler.hpp"
#include "decoder/smart_items.hpp"
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
bool VideoResampler::open(int w, int h, AVPixelFormat src_format) {
    width_ = w;
    height_ = h;
    sws_.reset(sws_getContext(w, h, src_format, w, h, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr));
    return sws_ != nullptr;
}

std::optional<smart_frame> VideoResampler::convert(const AVFrame* frame) {
    smart_frame out(av_frame_alloc());

    if(!out) return std::nullopt;

    out->format = AV_PIX_FMT_YUV420P;
    out->width = width_;
    out->height = height_;
    out->pts = frame->pts;

    if(av_frame_get_buffer(out.get(), 0) < 0) return std::nullopt;

    int result = sws_scale(sws_.get(), frame->data, frame->linesize, 0, height_, out->data, out->linesize);
    if(result <= 0) return std::nullopt;

    return out;
}
