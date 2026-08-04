#include "decoder/subtitle_decoder.hpp"
#include "decoder/smart_items.hpp"
#include <iostream>
#include <memory>
extern "C" {
 #include <libavcodec/codec_par.h>
 #include <libavcodec/avcodec.h>
 #include <libavcodec/codec.h>
 #include <libavcodec/packet.h>
}


bool SubtitleDecoder::init(AVCodecParameters * codecpar) {
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec)
        return false;

    AVCodecContext *raw_ctx = avcodec_alloc_context3(codec);

    if (!raw_ctx)
        return false;

    codec_context.reset(raw_ctx);

    if (avcodec_parameters_to_context(codec_context.get(), codecpar) < 0) {
        return false;
    }

    return avcodec_open2(codec_context.get(), codec, nullptr) == 0;
}

std::optional<smart_subtitle> SubtitleDecoder::decode(const AVPacket * packet) {
    auto raw_sub = new AVSubtitle();
    smart_subtitle sub{raw_sub};
    int got_subtitle = 0;
    int result = avcodec_decode_subtitle2(codec_context.get(), sub.get(), &got_subtitle, packet);
    if(result < 0 || !got_subtitle) return std::nullopt;
    return sub;
}
