#include "render/audio_renderer.hpp"
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
    device_ =  SDL_OpenAudioDevice(nullptr,0, &desired, nullptr,SDL_AUDIO_ALLOW_ANY_CHANGE);
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

bool AudioRenderer::render_frame(const uint8_t* data, uint32_t size) {

    int res = SDL_QueueAudio(device_, data, size);
    return res == 0;
}

void AudioRenderer::close() {
if(device_ != 0) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
}
}

uint32_t AudioRenderer::queued_size() const {
    return SDL_GetQueuedAudioSize(device_);
}
