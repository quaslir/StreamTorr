#include <catch2/catch_test_macros.hpp>
#include "decoder/demuxer.hpp"

namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;
constexpr const char* kNonExistentPath = "this/path/does/not/exist.mp4";
} 

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
    int subtitle_packets = 0;
    int other_packets = 0;
    int error_packets = 0;


    constexpr int kMaxIterations = 200000;
    int iterations = 0;

    while (iterations++ < kMaxIterations) {
        auto packet = demuxer.read_next_packet();
        if (!packet.has_value()) {
            break; 
        }

        switch (packet->type) {
            case PacketType::VIDEO: ++video_packets; break;
            case PacketType::AUDIO: ++audio_packets; break;
            case PacketType::SUBTITLE: ++subtitle_packets; break;
            case PacketType::OTHER: ++other_packets; break;
            case PacketType::ERROR: ++error_packets; break;
        }
    }

    REQUIRE(iterations < kMaxIterations); 
    REQUIRE(video_packets > 0);         
    REQUIRE(error_packets == 0);        


    INFO("video packets: " << video_packets);
    INFO("audio packets: " << audio_packets);
    INFO("subtitle packets: " << subtitle_packets);
    INFO("other packets: " << other_packets);
}
