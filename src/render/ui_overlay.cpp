#include "render/ui_overlay.hpp"
#include "configuration/config.hpp"
#include "fmt/format.h"
#include <SDL_ttf.h>
#include <algorithm>
#include <chrono>
#include <fmt/core.h>
#include <iostream>
#include <string>
bool UIOverlay::open() {
    const std::string font_path = std::string{ASSETS_DIR} + "/fonts/Inter-Regular.ttf";
    font_ = TTF_OpenFont(font_path.c_str(), FONT_SIZE);
    if (!font_)
        return false;
    return true;
}

SDL_Rect UIOverlay::draw_panel(SDL_Renderer *renderer, int window_w, int window_h) {
    SDL_Rect rect{0, window_h - kPanelHeight, window_w, kPanelHeight};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    SDL_RenderFillRect(renderer, &rect);
    return rect;
}
void UIOverlay::draw_progress_bar(SDL_Renderer *renderer, const SDL_Rect &rect, int window_w,
                                  float download_progress, double current_time, double duration) {
    int bar_x = kProgressBarMargin;
    int bar_y = rect.y + kProgressBarY;
    int bar_w = window_w - 2 * kProgressBarMargin;

    float download_ratio = std::clamp(download_progress, 0.0f, 1.0f);
    int downloaded_w = static_cast<int>(static_cast<float>(bar_w) * download_ratio);
    SDL_Rect download_rect{bar_x, bar_y, downloaded_w, kProgressBarHeight};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &download_rect);

    float play_ratio = (duration > 0.0) ? static_cast<float>(current_time / duration) : 0.0f;

    play_ratio = std::clamp(play_ratio, 0.0f, 1.0f);

    int played_w = static_cast<int>(static_cast<float>(bar_w) * play_ratio);
    SDL_Rect played_visual_rect{bar_x, bar_y, played_w, kProgressBarHeight};

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &played_visual_rect);
    progress_bar_bounds_ =
        SDL_Rect{bar_x, bar_y - kHitPadding, bar_w, kProgressBarHeight + 2 * kHitPadding};
}
void UIOverlay::draw_volume_bar(SDL_Renderer *renderer, int font_x, int font_y, int text_h,
                                float volume) {
    int vol_x = font_x - kVolumeBarWidth - kProgressBarMargin;
    int vol_y = font_y + (text_h - kVolumeBarHeight) / 2;

    SDL_Rect volume_rect{vol_x, vol_y, kVolumeBarWidth, kVolumeBarHeight};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &volume_rect);

    int filled_w =
        static_cast<int>(static_cast<float>(kVolumeBarWidth) * std::clamp(volume, 0.0f, 1.0f));

    SDL_Rect volume_filled_rect{vol_x, vol_y, filled_w, kVolumeBarHeight};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &volume_filled_rect);
    volume_bar_bounds_ =
        SDL_Rect{vol_x, vol_y - kHitPadding, kVolumeBarWidth, kVolumeBarHeight + 2 * kHitPadding};
}

std::tuple<int, int, int> UIOverlay::render_time(SDL_Renderer *renderer, int window_w,
                                                 int position_y, double current_time,
                                                 double duration) {
    std::string time_text = time_to_string(current_time) + " / " + time_to_string(duration);

    SDL_Surface *font_surface =
        TTF_RenderText_Blended(font_, time_text.c_str(), SDL_Color{255, 255, 255, 255});
    if (!font_surface)
        return {0, 0, 0};
    SDL_Texture *font_texture = SDL_CreateTextureFromSurface(renderer, font_surface);
    SDL_FreeSurface(font_surface);
    if (!font_texture) {
        return {0, 0, 0};
    }

    int text_w{0}, text_h{0};
    SDL_QueryTexture(font_texture, nullptr, nullptr, &text_w, &text_h);
    int font_x = window_w - text_w - kProgressBarMargin;
    int font_y = position_y + kProgressBarY + kProgressBarHeight + 10;
    SDL_Rect font_rect{font_x, font_y, text_w, text_h};
    SDL_RenderCopy(renderer, font_texture, nullptr, &font_rect);
    SDL_DestroyTexture(font_texture);

    return {font_x, font_y, text_h};
}

void UIOverlay::draw_play_button(SDL_Renderer *renderer, int position_y, bool is_playing) {
    int btn_y = position_y - kPlayButtonSize / 2;
    play_button_bounds_ = SDL_Rect{kPlayButtonMargin, btn_y, kPlayButtonSize, kPlayButtonSize};

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    if (is_playing) {
        int btn_bar_w = kPlayButtonSize / 3;
        SDL_Rect pause_bar_1{kPlayButtonMargin, btn_y, btn_bar_w, kPlayButtonSize};
        SDL_Rect pause_bar_2{kPlayButtonMargin + kPlayButtonSize - btn_bar_w, btn_y, btn_bar_w,
                             kPlayButtonSize};
        SDL_RenderFillRect(renderer, &pause_bar_1);
        SDL_RenderFillRect(renderer, &pause_bar_2);
    } else {
        SDL_Vertex verts[3] = {
            {{static_cast<float>(kPlayButtonMargin), static_cast<float>(btn_y)},
             {255, 255, 255, 255},
             {0, 0}},
            {{static_cast<float>(kPlayButtonMargin), static_cast<float>(btn_y + kPlayButtonSize)},
             {255, 255, 255, 255},
             {0, 0}},
            {{static_cast<float>(kPlayButtonMargin + kPlayButtonSize),
              static_cast<float>(btn_y) + (static_cast<float>(kPlayButtonSize) / 2)},
             {255, 255, 255, 255},
             {0, 0}}};
        SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);
    }
}

void UIOverlay::draw_fullscreen_icon(SDL_Renderer *renderer, int window_w, int position_y) {
    int fs_x = window_w - kFullscreenIconSize - kFullscreenIconMargin;
    int fs_y = position_y - kFullscreenIconSize / 2;

    fullscreen_button_bounds_ =
        SDL_Rect{fs_x - kHitPadding, fs_y - kHitPadding, kFullscreenIconSize + 2 * kHitPadding,
                 kFullscreenIconSize + 2 * kHitPadding};

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int corner_len = kFullscreenIconSize / 3;

    auto draw_thick_hline = [&](int x1, int x2, int y) {
        for(int t = 0; t < kThickness; t++) {
            SDL_RenderDrawLine(renderer, x1, y+t, x2, y + t);
        }
    };

    auto draw_thick_vline = [&](int x, int y1, int y2) {
        for(int t = 0; t < kThickness; t++) {
            SDL_RenderDrawLine(renderer, x + t, y1, x + t, y2);
        }
    };


    draw_thick_hline(fs_x,fs_x + corner_len, fs_y);
    draw_thick_vline(fs_x, fs_y, fs_y + corner_len);

    draw_thick_hline(fs_x + kFullscreenIconSize - corner_len,
                       fs_x + kFullscreenIconSize, fs_y);
    draw_thick_vline(fs_x + kFullscreenIconSize - kThickness, fs_y,
                       fs_y + corner_len);

    draw_thick_vline(fs_x, fs_y + kFullscreenIconSize - corner_len,
                       fs_y + kFullscreenIconSize);
    draw_thick_hline(fs_x, fs_x + corner_len,
                       fs_y + kFullscreenIconSize - kThickness);

    draw_thick_hline(fs_x + kFullscreenIconSize - corner_len,
                    fs_x + kFullscreenIconSize,
                       fs_y + kFullscreenIconSize - kThickness);
    draw_thick_vline(fs_x + kFullscreenIconSize - kThickness, fs_y + kFullscreenIconSize - corner_len,fs_y + kFullscreenIconSize);
}

void UIOverlay::draw(SDL_Renderer *renderer, int window_w, int window_h, double current_time,
                     double duration, float download_progress, bool is_playing, float volume) {
    if (std::chrono::steady_clock::now() - last_mouse_active_ > kTimeoutMouse)
        return;

    SDL_Rect rect = draw_panel(renderer, window_w, window_h);
            int icon_center_y = rect.y + kPanelHeight / 2;
    draw_progress_bar(renderer, rect, window_w, download_progress, current_time, duration);
draw_fullscreen_icon(renderer, window_w, icon_center_y + 6);
int fullscreen_left_edge = fullscreen_button_bounds_.x;
    auto [font_x, font_y, text_h] = render_time(renderer, fullscreen_left_edge - kProgressBarMargin, rect.y, current_time, duration);
    draw_volume_bar(renderer, font_x, font_y, text_h, volume);

    draw_play_button(renderer, icon_center_y + 6, is_playing);
}

std::string UIOverlay::time_to_string(double seconds) {
    int total_seconds = static_cast<int>(seconds);
    int hours = total_seconds / 3600;
    int mins = (total_seconds % 3600) / 60;
    int secs = total_seconds % 60;
    return fmt::format("{:02}:{:02}:{:02}", hours, mins, secs);
}

HitResult UIOverlay::handle_click(int x, int y, double duration) {
    HitResult result{};
    SDL_Point p{x, y};

    if (SDL_PointInRect(&p, &play_button_bounds_)) {
        result.play_pause_clicked = true;
        return result;
    }
    if(SDL_PointInRect(&p, &fullscreen_button_bounds_)) {
        result.fullscreen_toggled = true;
        return result;
    }
    if (SDL_PointInRect(&p, &progress_bar_bounds_)) {
        result.seek_requested = true;
        float ratio = static_cast<float>(x - progress_bar_bounds_.x) /
                      static_cast<float>(progress_bar_bounds_.w);
        result.seek_to_seconds = static_cast<double>(std::clamp(ratio, 0.0f, 1.0f)) * duration;
        return result;
    }

    if (SDL_PointInRect(&p, &volume_bar_bounds_)) {
        result.volume_changed = true;
        float ratio =
            static_cast<float>(x - volume_bar_bounds_.x) / static_cast<float>(volume_bar_bounds_.w);
        result.new_volume = std::clamp(ratio, 0.0f, 1.0f);
        return result;
    }
    return result;
}

FrameInput UIOverlay::poll_events() {
    SDL_Event event;
    FrameInput input;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEMOTION) {
            last_mouse_active_ = std::chrono::steady_clock::now();
        } else if (event.type == SDL_QUIT) {
            input.event = RenderEvent::WINDOW_CLOSED;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
            input.event = RenderEvent::PAUSE;
        } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            input.mouse_clicked = true;
            input.mouse_x = event.button.x;
            input.mouse_y = event.button.y;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT) {
            input.event = RenderEvent::SEEK_BACKWARD;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) {
            input.event = RenderEvent::SEEK_FORWARD;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            input.event = RenderEvent::DISABLE_FULLSCREEN;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f) {
            input.event = RenderEvent::ENABLE_FULLSCREEN;
        }
    }

    return input;
}
