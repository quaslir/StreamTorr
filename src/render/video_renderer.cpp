#include "render/video_renderer.hpp"
#include <SDL2/SDL.h>

bool VideoRenderer::open(int width, int height, const char *window_title) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Window *window = SDL_CreateWindow(window_title, 0, 0, width, height,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window)
        return false;
    window_.reset(window);

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        return false;
    renderer_.reset(renderer);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
                                             SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture)
        return false;
    texture_.reset(texture);

    texture_width_ = width;
    texture_height_ = height;
    return true;
}

bool VideoRenderer::resize_texture(int width, int height) {
if(width == texture_width_ && height==texture_height_) return true;

SDL_Texture * texture = SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height);
if(!texture) return false;
texture_.reset(texture);
texture_width_ = width;
texture_height_ = height;
texture_is_valid_ = false;
return true;
}
void VideoRenderer::close() {
    texture_is_valid_ = false;
    texture_.reset();
    renderer_.reset();
    window_.reset();
}

bool VideoRenderer::update_texture(const AVFrame *frame) {
    if (!frame || !texture_)
        return false;
    if(frame->width != texture_width_ || frame->height != texture_height_) {
        if(!resize_texture(frame->width, frame->height)) return false;
    }
    if(!texture_) return false;
    int result_update_texture = SDL_UpdateYUVTexture(
        texture_.get(), nullptr, frame->data[0], frame->linesize[0], frame->data[1],
        frame->linesize[1], frame->data[2], frame->linesize[2]);
    if (result_update_texture < 0)
        return false;
    texture_is_valid_ = true;
    return true;
}

void VideoRenderer::draw_frame() {
    SDL_SetRenderDrawColor(renderer_.get(), 0, 0, 0, 255);
    SDL_RenderClear(renderer_.get());
    if(texture_is_valid_) {
        SDL_RenderCopy(renderer_.get(), texture_.get(), nullptr, nullptr);
    }
}
 void VideoRenderer::toggle_fullscreen(bool fullscreen_on) {
     SDL_SetWindowFullscreen(window_.get(), fullscreen_on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
 }
void VideoRenderer::present() { SDL_RenderPresent(renderer_.get()); }
SDL_Renderer *VideoRenderer::renderer() const { return renderer_.get(); }
std::pair<int, int> VideoRenderer::window_size() const {
    int w{0}, h{0};
    SDL_GetRendererOutputSize(renderer_.get(), &w, &h);
    return {w, h};
}
