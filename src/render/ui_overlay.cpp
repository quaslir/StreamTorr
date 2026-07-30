#include "render/ui_overlay.hpp"
#include "configuration/config.hpp"
#include "fmt/format.h"
#include <SDL_ttf.h>
#include <iostream>
#include <string>
#include <fmt/core.h>
bool UIOverlay::open() {
    const std::string font_path = std::string{ASSETS_DIR} + "/fonts/Inter-Regular.ttf";
    font_ = TTF_OpenFont(font_path.c_str(), FONT_SIZE);
    if (!font_)        return false;
    return true;
}

void UIOverlay::draw(SDL_Renderer *renderer, int window_w, int window_h, double current_time,
                     double duration, float download_progress, bool is_playing, float volume) {
    SDL_Rect rect{0, window_h - kPanelHeight, window_w, kPanelHeight};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &rect);

    int bar_x = kProgressBarMargin;
    int bar_y = rect.y + kProgressBarY;
    int bar_w = window_w - 2 * kProgressBarMargin;

    SDL_Rect download_rect{bar_x, bar_y, bar_w, kProgressBarHeight};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &download_rect);

    float play_ratio = (duration > 0.0) ? static_cast<float>(current_time / duration) : 0.0f;

    play_ratio = std::clamp(play_ratio, 0.0f, 1.0f);

    int played_w = static_cast<int>(static_cast<float>(bar_w) * play_ratio);
    progress_bar_bounds_ = SDL_Rect{bar_x, bar_y, played_w, kProgressBarHeight};

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &progress_bar_bounds_);

    std::string time_text = time_to_string(current_time) + " / " + time_to_string(duration);

    SDL_Surface * font_surface = TTF_RenderText_Blended(font_, time_text.c_str(), SDL_Color{255, 255, 255, 255});
    if(!font_surface) return;
    SDL_Texture* font_texture =  SDL_CreateTextureFromSurface(renderer, font_surface);
          SDL_FreeSurface(font_surface);
    if(!font_texture) {
        return;
    }

    int text_w{0}, text_h{0};
    SDL_QueryTexture(font_texture, nullptr, nullptr, &text_w, &text_h);
    int font_x = window_w - text_w - kProgressBarMargin;
    int font_y = rect.y + kProgressBarY + kProgressBarHeight + 10;
    SDL_Rect font_rect{font_x, font_y, text_w, text_h};
    SDL_RenderCopy(renderer, font_texture, nullptr, &font_rect);
    SDL_DestroyTexture(font_texture);


    int btn_y = (rect.y + (kPanelHeight - kPlayButtonSize) / 2) + 5;
    play_button_bounds_ = SDL_Rect{kPlayButtonMargin, btn_y, kPlayButtonSize, kPlayButtonSize};




    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);


    if(is_playing) {
         int btn_bar_w = kPlayButtonSize / 3;
        SDL_Rect pause_bar_1{kPlayButtonMargin, btn_y, btn_bar_w, kPlayButtonSize};
        SDL_Rect pause_bar_2{kPlayButtonMargin + kPlayButtonSize - btn_bar_w, btn_y, btn_bar_w, kPlayButtonSize};
        SDL_RenderFillRect(renderer, &pause_bar_1);
        SDL_RenderFillRect(renderer, &pause_bar_2);
    } else {
        SDL_Vertex verts[3] = {
    {{static_cast<float>(kPlayButtonMargin), static_cast<float>(btn_y)}, {255,255,255,255}, {0, 0}},
    {{static_cast<float>(kPlayButtonMargin), static_cast<float>(btn_y + kPlayButtonSize)}, {255,255,255,255}, {0, 0}},
    {{static_cast<float>(kPlayButtonMargin + kPlayButtonSize), static_cast<float>(btn_y + (static_cast<float>(kPlayButtonSize) / 2))}, {255,255,255,255}, {0, 0}}
        };
        SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);
    }
    (void)download_progress;
    (void)is_playing;
    (void)volume;
}

 std::string UIOverlay::time_to_string(double seconds) {
int total_seconds = static_cast<int>(seconds);
int hours = total_seconds / 3600;
int mins = (total_seconds % 3600) / 60;
int secs = total_seconds % 60;
return fmt::format("{:02}:{:02}:{:02}", hours, mins, secs);
}
