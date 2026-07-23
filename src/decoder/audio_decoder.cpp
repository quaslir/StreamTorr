#include "decoder/audio_decoder.hpp"
#include <cerrno>
#include <optional>

bool AudioDecoder::init(AVCodecParameters * codecpar) {
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if(!codec) return false;

    AVCodecContext * raw_ctx = avcodec_alloc_context3(codec);

    if(!raw_ctx) return false;

    codec_context.reset(raw_ctx);

    if(avcodec_parameters_to_context(codec_context.get(), codecpar) < 0) {
        return false;
    }

    return avcodec_open2(codec_context.get(), codec, nullptr) == 0;
}


DecoderSendResult AudioDecoder::send_packet(const AVPacket* packet) {
int result = avcodec_send_packet(codec_context.get(), packet);

if(result == 0) return DecoderSendResult::Ok;
else if(result == AVERROR(EAGAIN)) return DecoderSendResult::NeedsMoreOutput;

return DecoderSendResult::Error;

}

std::optional<smart_frame> AudioDecoder::receive_frame() {

    smart_frame frame(av_frame_alloc());

if(!frame) return std::nullopt;

int result = avcodec_receive_frame(codec_context.get(), frame.get());

if(result == 0) {
    return frame;
}

return std::nullopt;

}

void AudioDecoder::flush() {
avcodec_flush_buffers(codec_context.get());
}