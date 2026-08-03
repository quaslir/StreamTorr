#include <catch2/catch_test_macros.hpp>

#include "decoder/demuxer.hpp"
#include "decoder/audio_decoder.hpp"

namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;
constexpr int kMaxIterations = 200000;
}

TEST_CASE("AudioDecoder::init succeeds with real stream parameters", "[audio_decoder]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));
    if (!demuxer.has_audio()) {
        WARN("Test file has no audio stream — skipping AudioDecoder test. "
             "Pick a test file with audio to exercise this path.");
        return;
    }

    AudioDecoder audio_decoder;
    auto audio_info = demuxer.audio_stream_info();
    REQUIRE(audio_info.has_value());
    REQUIRE(audio_decoder.init(audio_info.value()));
}

TEST_CASE("Demuxer + AudioDecoder decode a full audio stream without crashing",
          "[audio_decoder][integration]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    if (!demuxer.has_audio()) {
        WARN("Test file has no audio stream — skipping AudioDecoder test.");
        return;
    }

    AudioDecoder audio_decoder;
    auto audio_info = demuxer.audio_stream_info();
    REQUIRE(audio_info.has_value());
    REQUIRE(audio_decoder.init(audio_info.value()));

    int audio_packets_sent = 0;
    int decoded_frames = 0;
    int iterations = 0;

    while (iterations++ < kMaxIterations) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) {
            break;
        }

        if (demuxed->type != PacketType::AUDIO) {
            continue;
        }

        ++audio_packets_sent;

        const DecoderSendResult send_result = audio_decoder.send_packet(demuxed->packet.get());
        REQUIRE(send_result != DecoderSendResult::Error);

        while (auto frame = audio_decoder.receive_frame()) {
            ++decoded_frames;
            REQUIRE(frame.value() != nullptr);
        }
    }

    REQUIRE(iterations < kMaxIterations);
    REQUIRE(audio_packets_sent > 0);
    REQUIRE(decoded_frames > 0);


    INFO("audio packets sent: " << audio_packets_sent);
    INFO("decoded frames: " << decoded_frames);
}

TEST_CASE("AudioDecoder::flush resets state without crashing", "[audio_decoder]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    if (!demuxer.has_audio()) {
        WARN("Test file has no audio stream — skipping AudioDecoder test.");
        return;
    }

    AudioDecoder audio_decoder;
    auto audio_info = demuxer.audio_stream_info();
    REQUIRE(audio_info.has_value());
    REQUIRE(audio_decoder.init(audio_info.value()));

    int packets_fed = 0;
    while (packets_fed < 5) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) break;
        if (demuxed->type != PacketType::AUDIO) continue;

        audio_decoder.send_packet(demuxed->packet.get());
        while (audio_decoder.receive_frame()) {
        }
        ++packets_fed;
    }

    REQUIRE_NOTHROW(audio_decoder.flush());
}
