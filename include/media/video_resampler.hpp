#pragma once

#include "decoder/smart_items.hpp"
#include "smart_types.hpp"
extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <optional>

class VideoResampler {
  private:
    smart_sws sws_{nullptr};
    int width_, height_;

  public:
    bool open(int w, int h, AVPixelFormat src_format);

    std::optional<smart_frame> convert(const AVFrame *frame);
};
