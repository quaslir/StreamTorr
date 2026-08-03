

#include "decoder/smart_items.hpp"
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <optional>
#include <string>
struct SubtitleEvent {
    double start_time{0.0};
    double end_time{0.0};
    std::string text{};
};

class SubtitleDecoder {
  private:
    smart_codec_context codec_context{nullptr};

  public:
    bool init(AVCodecParameters * codecpar);
  std::optional<smart_subtitle> decode(const AVPacket * packet);
};
