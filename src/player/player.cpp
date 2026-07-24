#include "player/player.hpp"
#include <cstdint>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <thread>

bool Player::open(const std::string& path) {
if(!pipeline_.open(path)) return false;
if(!video_renderer_.open(500, 500)) return false;
if(!audio_renderer_.open(48000,
    2,
    AUDIO_S16SYS)) return false;
state_ = PlayerState::Ready;
return true;
}

void Player::audio_loop() {
for(;;) {
    auto frame = pipeline_.audio_frames().pop();

    if(!frame) break;

    AVFrame * raw = frame->get();

    int data_size = raw->nb_samples * raw->ch_layout.nb_channels * av_get_bytes_per_sample(static_cast<AVSampleFormat>(raw->format));

    audio_renderer_.render_frame(raw->data[0], static_cast<uint32_t>(data_size));
}
}

void Player::play() {
    if(state_ == PlayerState::Ready) {
    pipeline_.start();
    audio_thread_ = std::thread(&Player::audio_loop, this);
    state_ = PlayerState::Playing;
    }
}

void Player::update() {
    if(state_ != PlayerState::Playing) return;
    if(video_renderer_.poll_events() == RenderEvent::WINDOW_CLOSED) {
        stop();
        return;
    }

    auto frame = pipeline_.video_frames().pop();
    if(!frame) {
        state_ = PlayerState::Finished;
        return;
    }

    if(!video_renderer_.render_frame(frame->get())) {
        stop();
        return;
    }

}

void Player::stop() {
    pipeline_.stop();
    if(audio_thread_.joinable()) audio_thread_.join();
    video_renderer_.close();

    state_ = PlayerState::Stopped;
}
