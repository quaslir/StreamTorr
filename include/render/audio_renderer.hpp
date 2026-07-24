#include "smart_items.hpp"
#include <SDL2/SDL.h>


class AudioRenderer {
    private:
        SDL_AudioDeviceID device_{0};

    public:
        AudioRenderer() = default;
        AudioRenderer(const AudioRenderer&) = delete;
        AudioRenderer& operator=(const AudioRenderer&) = delete;

        AudioRenderer(AudioRenderer&&) noexcept = default;
        AudioRenderer& operator=(AudioRenderer&&) noexcept = default;

        bool open(int sample_rate, uint8_t channels, SDL_AudioFormat format);
        bool render_frame(const uint8_t* data, uint32_t size);

        void close();
};
