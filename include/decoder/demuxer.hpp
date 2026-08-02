#pragma once

#include <cstdint>
#include <cstdio>

#include <memory>
#include <optional>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/imgutils.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

#include "smart_items.hpp"
#include "types.hpp"

class Demuxer {
  private:
    smart_format_context format_context{nullptr};

    int video_stream_index{-1};
    int audio_stream_index{-1};

    bool open_{false};

  public:
    Demuxer();
    bool open(const std::string &filename);
    bool open_with_io_context(AVIOContext *io_context);

    bool seek(double seconds);
    bool is_open() const;

    bool has_video() const;
    bool has_audio() const;

    std::optional<AVCodecParameters *> video_stream_info() const;
    std::optional<AVCodecParameters *> audio_stream_info() const;

    std::optional<std::pair<int, int>> video_stream_size() const;

    AVRational audio_time_base() const;
    AVRational video_time_base() const;
    std::optional<DemuxedPacket> read_next_packet();
    double duration_seconds() const;
};
