#pragma once

#include <memory>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

struct AVFormatContextDeleter {
    void operator ()(AVFormatContext * ctx) const{
        if(ctx) {
            avformat_close_input(&ctx);
        }
    }
};

using smart_format_context = std::unique_ptr<AVFormatContext,AVFormatContextDeleter >;
class Demuxer {
  private:
    smart_format_context format_context{nullptr};

    int video_stream_index{0};
  public:
      Demuxer();
      bool open(const std::string& filename);
};
