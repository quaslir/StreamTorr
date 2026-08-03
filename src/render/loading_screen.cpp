#include "render/loading_screen.hpp"
#include "configuration/config.hpp"
#include <SDL_ttf.h>
#include <string>
 bool LoadingScreen::open() {
     SDL_Window * window = SDL_CreateWindow("Loading...", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
         400, 120, SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
     if(!window) return false;
     window_.reset(window);

     SDL_Renderer * renderer = SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_ACCELERATED);
     if(!renderer) return false;
     renderer_.reset(renderer);

     const std::string font_path = std::string{ASSETS_DIR} + "/fonts/Inter-Regular.ttf";
     font_ = TTF_OpenFont(font_path.c_str(), FONT_SIZE);

     return font_ != nullptr;
 }

 void LoadingScreen::update(const std::string& stage_text, float percent) {
     SDL_Event event;

     while(SDL_PollEvent(&event)) {}

         SDL_SetRenderDrawColor(renderer_.get(), 20, 20, 20, 155);
         SDL_RenderClear(renderer_.get());

         SDL_Rect bar_bg{40, 70, 320, 20};
         SDL_SetRenderDrawColor(renderer_.get(), 60, 60, 60, 255);
         SDL_RenderFillRect(renderer_.get(), &bar_bg);

         int filled_w = static_cast<int>(320.0f * std::clamp(percent, 0.0f, 1.0f));

         SDL_Rect bar_fill{40, 70, filled_w, 20};
         SDL_SetRenderDrawColor(renderer_.get(), 100, 180, 255, 255);
         SDL_RenderFillRect(renderer_.get(), &bar_fill);

         std::string text = stage_text +" " + std::to_string(static_cast<int>(percent * 100)) + "%";
         SDL_Surface * surface = TTF_RenderText_Blended(font_, text.c_str(), SDL_Color{255, 255, 255, 255});

         if(surface) {
             SDL_Texture * texture = SDL_CreateTextureFromSurface(renderer_.get(), surface);
             SDL_FreeSurface(surface);

             if(texture) {
                 int w, h;
                 SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
                 SDL_Rect dst{(400 - w) / 2, 30, w, h};
                 SDL_RenderCopy(renderer_.get(), texture, nullptr, &dst);
                 SDL_DestroyTexture(texture);
             }
         }
         SDL_RenderPresent(renderer_.get());
     }


 void LoadingScreen::close() {
     if(font_) {
         TTF_CloseFont(font_);
         font_ = nullptr;
     }

     renderer_.reset();
     window_.reset();
 }
