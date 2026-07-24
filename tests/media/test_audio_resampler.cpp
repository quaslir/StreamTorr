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

// Expected target format, matching what AudioResampler::open() configures
// internally (stereo, out_sample_fmt_, out_sample_rate_). If those defaults
// change, update these expectations to match.
constexpr int kExpectedChannels = 2;
} // namespace

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
            break; // EOF
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

            // --- Contract checks on the resampled frame ---
            REQUIRE(out != nullptr);
            REQUIRE(out->format == AV_SAMPLE_FMT_S16); // matches out_sample_fmt_
            REQUIRE(out->sample_rate == 48000);         // matches out_sample_rate_
            REQUIRE(out->ch_layout.nb_channels == kExpectedChannels);
            REQUIRE(out->nb_samples > 0);

            // PTS must be carried over from the input frame, not left
            // uninitialized/garbage from av_frame_alloc() — this was a bug
            // we fixed earlier (new_frame->pts = frame->pts;).
            REQUIRE(out->pts == frame->get()->pts);

            // Data buffer must actually be allocated (av_frame_get_buffer
            // succeeded) and non-null for a real, non-empty frame.
            REQUIRE(out->data[0] != nullptr);

            // Only test a bounded number of frames — this file has thousands
            // of audio frames and we don't need to convert all of them to
            // trust the resampler works correctly.
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
    REQUIRE(convert_fail_count == 0); // no silent resample failures expected on a clean file

    INFO("converted frames: " << converted_count << ", failed: " << convert_fail_count);
}

TEST_CASE("AudioResampler handles multiple consecutive frames without state corruption",
          "[audio_resampler][integration]") {
    // Regression-style check: swr_convert carries internal state (e.g. for
    // resampling ratios that aren't exact integers) across calls. Converting
    // several frames in a row should never crash or silently produce
    // zero-sample output once the resampler has been warmed up.
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
