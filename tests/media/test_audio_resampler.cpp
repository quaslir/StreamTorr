#include <catch2/catch_test_macros.hpp>

#include "decoder/demuxer.hpp"
#include "decoder/audio_decoder.hpp"
#include "media/audio_resampler.hpp"

#ifndef STREAMTORR_TEST_VIDEO_PATH
#define STREAMTORR_TEST_VIDEO_PATH "assets/test_media/sintel_trailer-1080p.mp4"
#endif

namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;
constexpr int kMaxIterations = 200000;
constexpr int kExpectedChannels = 2;
}

TEST_CASE("AudioResampler::open succeeds with a real decoder's codec context",
          "[audio_resampler]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    if (!demuxer.has_audio()) {
        WARN("Test file has no audio stream — skipping AudioResampler test.");
        return;
    }

    AudioDecoder audio_decoder;
    auto audio_info = demuxer.audio_stream_info();
    REQUIRE(audio_info.has_value());
    REQUIRE(audio_decoder.init(audio_info.value()));

    AudioResampler resampler;
    REQUIRE(resampler.open(audio_decoder.get_codec_context()));
}

TEST_CASE("AudioResampler::convert produces frames in the target format",
          "[audio_resampler][integration]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    if (!demuxer.has_audio()) {
        WARN("Test file has no audio stream — skipping AudioResampler test.");
        return;
    }

    AudioDecoder audio_decoder;
    auto audio_info = demuxer.audio_stream_info();
    REQUIRE(audio_info.has_value());
    REQUIRE(audio_decoder.init(audio_info.value()));

    AudioResampler resampler;
    REQUIRE(resampler.open(audio_decoder.get_codec_context()));

    int converted_count = 0;
    int convert_fail_count = 0;
    int iterations = 0;

    while (iterations++ < kMaxIterations) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) {
            break;
        }
        if (demuxed->type != PacketType::AUDIO) {
            continue;
        }

        const DecoderSendResult send_result = audio_decoder.send_packet(demuxed->packet.get());
        REQUIRE(send_result != DecoderSendResult::Error);

        while (auto frame = audio_decoder.receive_frame()) {
            auto resampled = resampler.convert(frame->get());
            if (!resampled.has_value()) {
                ++convert_fail_count;
                continue;
            }
            ++converted_count;

            const AVFrame* out = resampled->get();

            REQUIRE(out != nullptr);
            REQUIRE(out->format == AV_SAMPLE_FMT_S16);
            REQUIRE(out->sample_rate == 48000);
            REQUIRE(out->ch_layout.nb_channels == kExpectedChannels);
            REQUIRE(out->nb_samples > 0);

            REQUIRE(out->pts == frame->get()->pts);

            REQUIRE(out->data[0] != nullptr);

            if (converted_count >= 50) {
                break;
            }
        }

        if (converted_count >= 50) {
            break;
        }
    }

    REQUIRE(iterations < kMaxIterations);
    REQUIRE(converted_count > 0);
    REQUIRE(convert_fail_count == 0);

    INFO("converted frames: " << converted_count << ", failed: " << convert_fail_count);
}

TEST_CASE("AudioResampler handles multiple consecutive frames without state corruption",
          "[audio_resampler][integration]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    if (!demuxer.has_audio()) {
        WARN("Test file has no audio stream — skipping AudioResampler test.");
        return;
    }

    AudioDecoder audio_decoder;
    auto audio_info = demuxer.audio_stream_info();
    REQUIRE(audio_info.has_value());
    REQUIRE(audio_decoder.init(audio_info.value()));

    AudioResampler resampler;
    REQUIRE(resampler.open(audio_decoder.get_codec_context()));

    int converted_count = 0;
    int iterations = 0;
    int zero_sample_frames = 0;

    while (iterations++ < kMaxIterations && converted_count < 100) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) break;
        if (demuxed->type != PacketType::AUDIO) continue;

        audio_decoder.send_packet(demuxed->packet.get());
        while (auto frame = audio_decoder.receive_frame()) {
            auto resampled = resampler.convert(frame->get());
            if (resampled.has_value()) {
                ++converted_count;
                if (resampled->get()->nb_samples == 0) {
                    ++zero_sample_frames;
                }
            }
        }
    }

    REQUIRE(converted_count >= 100);
    REQUIRE(zero_sample_frames == 0);
}