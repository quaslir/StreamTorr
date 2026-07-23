#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "pipeline/pipeline.hpp"


namespace {
constexpr const char* kTestVideoPath = STREAMTORR_TEST_VIDEO_PATH;
constexpr const char* kNonExistentPath = "this/path/does/not/exist.mp4";
constexpr int kMaxIterations = 200000;

// Drains a single FrameQueue in a loop until it reports closed (nullopt),
// counting how many frames came through. Meant to be run on its own thread
// so video and audio queues are consumed concurrently -- Pipeline's single
// demux_loop() thread feeds both queues, so leaving either one undrained
// will eventually block the producer on FrameQueue::push() and stall
// everything, including the queue that IS being read.
int drain_queue(FrameQueue<smart_frame>& queue) {
    int count = 0;
    int iterations = 0;
    while (iterations++ < kMaxIterations) {
        auto frame = queue.pop();
        if (!frame.has_value()) {
            break; // queue closed
        }
        ++count;
    }
    return count;
}
} // namespace

TEST_CASE("Pipeline::open fails gracefully on a missing file", "[pipeline]") {
    Pipeline pipeline;
    REQUIRE_FALSE(pipeline.open(kNonExistentPath));
}

TEST_CASE("Pipeline::open succeeds on a valid media file", "[pipeline]") {
    Pipeline pipeline;
    REQUIRE(pipeline.open(kTestVideoPath));
}

TEST_CASE("Pipeline produces both video and audio frames end-to-end",
          "[pipeline][integration]") {
    Pipeline pipeline;
    REQUIRE(pipeline.open(kTestVideoPath));

    pipeline.start();

    // Both queues must be drained concurrently -- see drain_queue() comment.
    int video_frames = 0;
    int audio_frames = 0;

    std::thread audio_thread([&pipeline, &audio_frames]() {
        audio_frames = drain_queue(pipeline.audio_frames());
    });

    video_frames = drain_queue(pipeline.video_frames());

    audio_thread.join();
    pipeline.stop();

    REQUIRE(video_frames > 0);
    REQUIRE(audio_frames > 0);

    INFO("decoded video frames via Pipeline: " << video_frames);
    INFO("decoded audio frames via Pipeline: " << audio_frames);
}

TEST_CASE("Pipeline::stop is safe to call without ever consuming frames",
          "[pipeline]") {
    // Regression check for the backpressure scenario: if nobody drains
    // video_frames()/audio_frames(), the background thread will block
    // inside FrameQueue::push() once the bounded queues fill up. stop()
    // must still be able to unblock and join it via close().
    Pipeline pipeline;
    REQUIRE(pipeline.open(kTestVideoPath));

    pipeline.start();

    // Give the background thread a moment to actually start decoding and
    // fill up the queues, so this test meaningfully exercises the
    // full-queue/close() interaction rather than stopping instantly.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    REQUIRE_NOTHROW(pipeline.stop());
}

TEST_CASE("Pipeline can be opened and run more than once (sequential runs)",
          "[pipeline]") {
    for (int run = 0; run < 2; ++run) {
        Pipeline pipeline;
        REQUIRE(pipeline.open(kTestVideoPath));
        pipeline.start();

        int video_frames = 0;
        int audio_frames = 0;

        std::thread audio_thread([&pipeline, &audio_frames]() {
            audio_frames = drain_queue(pipeline.audio_frames());
        });

        video_frames = drain_queue(pipeline.video_frames());

        audio_thread.join();
        pipeline.stop();

        REQUIRE(video_frames > 0);
        REQUIRE(audio_frames > 0);

        INFO("run " << run << " video frames: " << video_frames
                     << ", audio frames: " << audio_frames);
    }
}
