#pragma once

#include <cstdint>
#include <cstdio>

#include <memory>
#include <optional>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavutil/rational.h>
}

struct AVFormatContextDeleter {
    void operator ()(AVFormatContext * ctx) const{
        if(ctx) {
            avformat_close_input(&ctx);
        }
    }
};


using smart_format_context = std::unique_ptr<AVFormatContext,AVFormatContextDeleter >;

struct AVPacketDeleter {
    void operator ()(AVPacket * packet) const{
        if(packet) {
            av_packet_free(&packet);
        }
    }
};

using smart_packet = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct VideoInfo {
    AVCodecID codec_id;
    int width, height;
    AVPixelFormat format;
    int64_t bit_rate;
    AVRational framerate;
};

struct AudioInfo {
    AVCodecID codec_id;
    int sample_rate;
    AVSampleFormat format;
    int channels;
    int64_t bit_rate;
};

enum class PacketType {
    VIDEO,
    AUDIO,
    OTHER,
    ERROR
};

struct DemuxedPacket {
    PacketType type;
    smart_packet packet;
};

class Demuxer {
  private:
    smart_format_context format_context{nullptr};

    int video_stream_index{-1};
    int audio_stream_index{-1};

    bool open_{false};
  public:
      Demuxer();
      bool open(const std::string& filename);
      bool is_open() const;

      bool has_video() const;
      bool has_audio() const;

      std::optional<VideoInfo> video_stream_info() const;
      std::optional<AudioInfo> audio_stream_info() const;

      std::optional<DemuxedPacket> read_next_packet();
};
