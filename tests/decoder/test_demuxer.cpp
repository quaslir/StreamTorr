#include <catch2/catch_test_macros.hpp>
#include "decoder/demuxer.hpp"

namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;
constexpr const char* kNonExistentPath = "this/path/does/not/exist.mp4";
} // namespace

TEST_CASE("Demuxer starts closed", "[demuxer]") {
    Demuxer demuxer;
    REQUIRE_FALSE(demuxer.is_open());
}

TEST_CASE("Demuxer::open fails gracefully on a missing file", "[demuxer]") {
    Demuxer demuxer;
    REQUIRE_FALSE(demuxer.open(kNonExistentPath));
}

TEST_CASE("Demuxer::open succeeds on a valid media file", "[demuxer]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));
    REQUIRE(demuxer.is_open());
}

TEST_CASE("Demuxer finds a video stream in a valid media file", "[demuxer]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));
    REQUIRE(demuxer.has_video());
    auto video_container = demuxer.video_stream_info();
    if(video_container.has_value()) {
    REQUIRE(video_container.value()->width > 0);
    REQUIRE(video_container.value()->height > 0);
    // codec_id should be a real codec, not the "unknown" sentinel.
    REQUIRE(video_container.value()->codec_id != AV_CODEC_ID_NONE);
    }
}

TEST_CASE("Demuxer reports audio info consistently with has_audio", "[demuxer]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    if (demuxer.has_audio()) {
        auto audio_container = demuxer.audio_stream_info();
        if(audio_container.has_value()) {
        REQUIRE(audio_container.value()->sample_rate > 0);
        REQUIRE(audio_container.value()->codec_id != AV_CODEC_ID_NONE);
        }
    } else {
        // Contract check: info on a file with no audio should come back as a
        // default/empty struct, not garbage from an out-of-bounds read.
        auto audio_container = demuxer.audio_stream_info();
        if(audio_container.has_value()) {
        REQUIRE(audio_container.value()->sample_rate == 0);
        }
    }
}

TEST_CASE("Demuxer::read_next_packet reads until EOF without crashing", "[demuxer]") {
    Demuxer demuxer;
    REQUIRE(demuxer.open(kTestVideoPath));

    int video_packets = 0;
    int audio_packets = 0;
    int other_packets = 0;
    int error_packets = 0;

    // Safety cap so a bug that never returns std::nullopt doesn't hang the
    // test suite forever — a real file will finish in far fewer iterations.
    constexpr int kMaxIterations = 200000;
    int iterations = 0;

    while (iterations++ < kMaxIterations) {
        auto packet = demuxer.read_next_packet();
        if (!packet.has_value()) {
            break; // EOF reached — the expected way out of this loop.
        }

        switch (packet->type) {
            case PacketType::VIDEO: ++video_packets; break;
            case PacketType::AUDIO: ++audio_packets; break;
            case PacketType::OTHER: ++other_packets; break;
            case PacketType::ERROR: ++error_packets; break;
        }
    }

    REQUIRE(iterations < kMaxIterations); // loop terminated via EOF, not the safety cap
    REQUIRE(video_packets > 0);           // a real video file must yield video packets
    REQUIRE(error_packets == 0);          // no read errors expected on a clean test file

    // Cross-check against `ffprobe -select_streams v:0 -count_packets
    // -show_entries stream=nb_read_packets` on the same file — the numbers
    // should match exactly.
    INFO("video packets: " << video_packets);
    INFO("audio packets: " << audio_packets);
    INFO("other packets: " << other_packets);
}
