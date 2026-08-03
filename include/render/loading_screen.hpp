#pragma once

#include "render/smart_items.hpp"
extern "C" {
    #include <SDL2/SDL.h>
    #include <SDL_ttf.h>
}

#include <string>

class LoadingScreen {
    private:
        smart_window window_{nullptr};
        smart_renderer renderer_{nullptr};
        TTF_Font * font_{nullptr};

    public:
        bool open();
        void update(const std::string& stage_text, float percent);
        void close();
};
