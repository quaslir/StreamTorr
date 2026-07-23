#pragma once
extern "C" {
    #include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}
#include <memory>

#include <optional>
#include "smart_items.hpp"
#include "types.hpp"

class VideoDecoder {
private:
smart_codec_context codec_context{nullptr};

public:

bool init(AVCodecParameters * codecpar);
DecoderSendResult send_packet(const AVPacket* packet);
std::optional<smart_frame> receive_frame();
void flush();
};
