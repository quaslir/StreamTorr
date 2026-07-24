#include "player/player.hpp"
#include <chrono>
#include <cstdint>
#include <ctime>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}
#include <thread>

bool Player::open(const std::string& path) {
if(!pipeline_.open(path)) return false;
auto target_video_size = pipeline_.video_stream_size();
if(!target_video_size.has_value()) return false;
if(!video_renderer_.open(target_video_size->first, target_video_size->second)) return false;
if(!audio_renderer_.open(48000,
    2,
    AUDIO_S16SYS)) return false;
if(pipeline_.has_audio()) {
audio_time_base_ = pipeline_.audio_time_base();
}
video_time_base_ = pipeline_.video_time_base();
state_ = PlayerState::Ready;
return true;
}

void Player::audio_loop() {
for(;;) {
    auto frame = pipeline_.audio_frames().pop();

    if(!frame) break;

    AVFrame * raw = frame->get();
    double pts_seconds = static_cast<double>(raw->pts) * av_q2d(audio_time_base_);
    double frame_duration = static_cast<double>(raw->nb_samples) / raw->sample_rate;
    double frame_end_pts = pts_seconds + frame_duration;
    int data_size = raw->nb_samples * raw->ch_layout.nb_channels * av_get_bytes_per_sample(static_cast<AVSampleFormat>(raw->format));
        audio_renderer_.render_frame(raw->data[0], static_cast<uint32_t>(data_size));
    uint32_t queued_bytes = audio_renderer_.queued_size();

    uint32_t bytes_per_second = static_cast<uint32_t>(raw->sample_rate) * static_cast<uint32_t>(raw->ch_layout.nb_channels) *
        static_cast<uint32_t>(av_get_bytes_per_sample(static_cast<AVSampleFormat>(raw->format)));
    double queued_seconds = static_cast<double>(queued_bytes) / static_cast<double>(bytes_per_second);
    double corrected_block = frame_end_pts - queued_seconds;
        pipeline_.clock().update(corrected_block);
    uint32_t target_buffer_bytes = bytes_per_second / 4;


    while(queued_bytes > target_buffer_bytes) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        queued_bytes = audio_renderer_.queued_size();
    }
}
}

void Player::play() {
    if(state_ == PlayerState::Ready) {
    pipeline_.start();
    if(pipeline_.has_audio()) {
    audio_thread_ = std::thread(&Player::audio_loop, this);
    }
    state_ = PlayerState::Playing;
    }
}

void Player::update() {
    if(state_ != PlayerState::Playing) return;
    if(video_renderer_.poll_events() == RenderEvent::WINDOW_CLOSED) {
        stop();
        return;
    }

    if(!pending_frame_) {
        pending_frame_ = pipeline_.video_frames().pop();
        if(!pending_frame_) {
            state_ = PlayerState::Finished;
            return;
        }
    }

        double frame_pts = static_cast<double>(pending_frame_->get()->pts) * av_q2d(video_time_base_);

    if(!clock_primed) {
        playback_start_real_ = std::chrono::steady_clock::now();
        playback_start_pts_ = frame_pts;
        pipeline_.clock().update(frame_pts);
        clock_primed = true;
    }





    double clock_time = pipeline_.clock().get_time();

    double elapsed_real = std::chrono::duration<double>(std::chrono::steady_clock::now() - playback_start_real_).count();

    double estimated_clock = playback_start_pts_ + elapsed_real;

    if(estimated_clock > clock_time) {
        clock_time = estimated_clock;
    }

    if(frame_pts > clock_time) {
        return;
    }


    if(!video_renderer_.render_frame(pending_frame_->get())) {
        stop();
        return;
    }

    pending_frame_.reset();

}

void Player::stop() {
    pipeline_.stop();
    if(audio_thread_.joinable()) audio_thread_.join();
    video_renderer_.close();

    state_ = PlayerState::Stopped;
}
