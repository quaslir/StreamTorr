#include <catch2/catch_test_macros.hpp>

#include "decoder/demuxer.hpp"
#include "decoder/video_decoder.hpp"


namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;

// Safety cap so a bug that never signals EOF/EAGAIN correctly doesn't hang
// the test suite forever.
constexpr int kMaxIterations = 200000;
} // namespace

TEST_CASE("VideoDecoder::init succeeds with real stream parameters", "[video_decoder]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));
    REQUIRE(demuxer.has_video());

    VideoDecoder video_decoder;
    auto video_info = demuxer.video_stream_info();
            REQUIRE(video_info.has_value());
            REQUIRE(video_decoder.init(video_info.value()));

}

TEST_CASE("Demuxer + VideoDecoder decode a full video stream without crashing",
          "[video_decoder][integration]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));
    REQUIRE(demuxer.has_video());

    VideoDecoder video_decoder;

    auto video_info = demuxer.video_stream_info();

         REQUIRE(video_info.has_value());
            REQUIRE(video_decoder.init(video_info.value()));



    int video_packets_sent = 0;
    int decoded_frames = 0;
    int iterations = 0;

    while (iterations++ < kMaxIterations) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) {
            break; // EOF from the demuxer — expected way out of the loop.
        }

        if (demuxed->type != PacketType::VIDEO) {
            continue; // audio/other packets are irrelevant to this test
        }

        ++video_packets_sent;

        const DecoderSendResult send_result = video_decoder.send_packet(demuxed->packet.get());
        REQUIRE(send_result != DecoderSendResult::Error);

        // Drain every frame the decoder is willing to give up before moving
        // on to the next packet — a single packet can yield zero, one, or
        // more frames depending on B-frame reordering.
        while (auto frame = video_decoder.receive_frame()) {
            ++decoded_frames;
            REQUIRE(frame.value() != nullptr);
        }
    }

    REQUIRE(iterations < kMaxIterations);   // terminated via EOF, not the safety cap
    REQUIRE(video_packets_sent > 0);        // sanity: the file actually has video packets
    REQUIRE(decoded_frames > 0);            // sanity: at least something got decoded

    // Cross-check against:
    //   ffprobe -select_streams v:0 -count_frames -show_entries stream=nb_read_frames
    // on the same file. Note this counts *decoded* frames, which can differ
    // from the raw packet count checked in test_demuxer.cpp when B-frames
    // are involved.
    INFO("video packets sent: " << video_packets_sent);
    INFO("decoded frames: " << decoded_frames);
}

TEST_CASE("VideoDecoder::flush resets state without crashing", "[video_decoder]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));
    REQUIRE(demuxer.has_video());
          VideoDecoder video_decoder;
     auto video_info = demuxer.video_stream_info();

REQUIRE(video_info.has_value());

    REQUIRE(video_decoder.init(video_info.value()));

    // Decode a handful of packets first so there's actually some internal
    // state to reset.
    int packets_fed = 0;
    while (packets_fed < 5) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) break;
        if (demuxed->type != PacketType::VIDEO) continue;

        video_decoder.send_packet(demuxed->packet.get());
        while (video_decoder.receive_frame()) {
            // drain, contents not checked here
        }
        ++packets_fed;
    }

    REQUIRE_NOTHROW(video_decoder.flush());
}
