#include "demuxer.hpp"
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
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


using smart_codec_context = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using smart_frame = std::unique_ptr<AVFrame, AVFrameDeleter>;
enum class DecoderSendResult {
    Ok,
    NeedsMoreOutput,
    Error
};

class VideoDecoder {
private:
smart_codec_context codec_context{nullptr};

public:

bool init(AVCodecParameters * codecpar);
DecoderSendResult send_packet(const AVPacket* packet);
std::optional<smart_frame> receive_frame();
void flush();
};