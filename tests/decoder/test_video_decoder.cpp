#include <catch2/catch_test_macros.hpp>

#include "decoder/demuxer.hpp"
#include "decoder/video_decoder.hpp"


namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;


constexpr int kMaxIterations = 200000;
} 

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
            break;
        }

        if (demuxed->type != PacketType::VIDEO) {
            continue; 
        }

        ++video_packets_sent;

        const DecoderSendResult send_result = video_decoder.send_packet(demuxed->packet.get());
        REQUIRE(send_result != DecoderSendResult::Error);


        while (auto frame = video_decoder.receive_frame()) {
            ++decoded_frames;
            REQUIRE(frame.value() != nullptr);
        }
    }

    REQUIRE(iterations < kMaxIterations);  
    REQUIRE(video_packets_sent > 0);     
    REQUIRE(decoded_frames > 0);            


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


    int packets_fed = 0;
    while (packets_fed < 5) {
        auto demuxed = demuxer.read_next_packet();
        if (!demuxed.has_value()) break;
        if (demuxed->type != PacketType::VIDEO) continue;

        video_decoder.send_packet(demuxed->packet.get());
        while (video_decoder.receive_frame()) {
   
        }
        ++packets_fed;
    }

    REQUIRE_NOTHROW(video_decoder.flush());
}
