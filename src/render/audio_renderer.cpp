#include "render/audio_renderer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
bool AudioRenderer::open(int sample_rate, uint8_t channels, SDL_AudioFormat format) {
    SDL_AudioSpec desired{};
    desired.freq = sample_rate;
    desired.channels = channels;
    desired.samples = 4096;
    desired.format = format;
    desired.callback = nullptr;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, SDL_AUDIO_ALLOW_ANY_CHANGE);
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

bool AudioRenderer::render_frame(const uint8_t *data, uint32_t size, float volume) {
    int sdl_volume = static_cast<int>(std::clamp(volume, 0.0f, 1.0f) * SDL_MIX_MAXVOLUME);
    std::vector<uint8_t> mixed(size, 0);
    SDL_MixAudioFormat(mixed.data(), data, AUDIO_S16SYS, size, sdl_volume);
    int res = SDL_QueueAudio(device_, mixed.data(), size);
    return res == 0;
}

void AudioRenderer::close() {
    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

uint32_t AudioRenderer::queued_size() const { return SDL_GetQueuedAudioSize(device_); }

void AudioRenderer::pause(bool should_pause) {
    SDL_PauseAudioDevice(device_, should_pause ? 1 : 0);
}
