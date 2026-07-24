#include "media/audio_resampler.hpp"
#include "decoder/smart_items.hpp"
#include <cstdint>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
#include <optional>
#include <sys/types.h>

bool AudioResampler::open(const AVCodecContext* decoder_ctx) {
    av_channel_layout_default(&out_ch_layout_, 2);
    SwrContext * raw_swr{nullptr};

    if(swr_alloc_set_opts2(&raw_swr, &out_ch_layout_, out_sample_fmt_, out_sample_rate_, &decoder_ctx->ch_layout, decoder_ctx->sample_fmt,
        decoder_ctx->sample_rate, 0 ,nullptr) < 0) return false;
    swr_.reset(raw_swr);
    if(swr_init(swr_.get()) < 0) return false;
    in_sample_rate_ = decoder_ctx->sample_rate;
    return true;
}

std::optional<smart_frame> AudioResampler::convert(const AVFrame* frame) {
   int samples =  swr_get_out_samples(swr_.get(), frame->nb_samples);
   AVFrame * raw_frame = av_frame_alloc();
   smart_frame new_frame(raw_frame);
   if(!new_frame) return std::nullopt;

   new_frame->format = out_sample_fmt_;
   av_channel_layout_copy(&new_frame->ch_layout, &out_ch_layout_);
   new_frame->sample_rate = out_sample_rate_;
   new_frame->nb_samples = samples;
   if(av_frame_get_buffer(new_frame.get(), 0) < 0) return std::nullopt;
   int converted = swr_convert(swr_.get(), new_frame->data, new_frame->nb_samples, const_cast<const uint8_t **>(frame->data), frame->nb_samples);
   if(converted < 0) return std::nullopt;

   new_frame->nb_samples = converted;

   return new_frame;

}
