#include "decoder/smart_items.hpp"
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <optional>
#include <vector>
extern "C" {
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
}
#include "smart_types.hpp"

class AudioResampler {
    private:
        smart_swr swr_{nullptr};
        AVSampleFormat out_sample_fmt_{AV_SAMPLE_FMT_S16};
        int out_sample_rate_{48000};
        AVChannelLayout out_ch_layout_{};

        int in_sample_rate_{0};

    public:

        bool open(const AVCodecContext* decoder_ctx);

        std::optional<smart_frame> convert(const AVFrame* frame);

};
