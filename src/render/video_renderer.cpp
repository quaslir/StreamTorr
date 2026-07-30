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

RenderEvent VideoRenderer::poll_events() {
    SDL_Event event;
    RenderEvent result = RenderEvent::NONE;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            result = RenderEvent::WINDOW_CLOSED;
        } else if ((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) ||
                   (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT))
            result = RenderEvent::PAUSE;

        else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT) {
            result = RenderEvent::SEEK_BACKWARD;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) {
            result = RenderEvent::SEEK_FORWARD;
        }
    }

    return result;
}

void VideoRenderer::close() {
    texture_.reset();
    renderer_.reset();
    window_.reset();
}

bool VideoRenderer::update_texture(const AVFrame *frame) {
    if (!frame || !texture_)
        return false;
    int result_update_texture = SDL_UpdateYUVTexture(
        texture_.get(), nullptr, frame->data[0], frame->linesize[0], frame->data[1],
        frame->linesize[1], frame->data[2], frame->linesize[2]);
    if (result_update_texture < 0)
        return false;
    int result_render_clear = SDL_RenderClear(renderer_.get());
    if (result_render_clear < 0)
        return false;
    int result_render_copy = SDL_RenderCopy(renderer_.get(), texture_.get(), nullptr, nullptr);
    if (result_render_copy < 0)
        return false;
    return true;
}
void VideoRenderer::present() { SDL_RenderPresent(renderer_.get()); }
SDL_Renderer *VideoRenderer::renderer() const { return renderer_.get(); }
std::pair<int, int> VideoRenderer::window_size() const {
    int w{0}, h{0};
    SDL_GetRendererOutputSize(renderer_.get(), &w, &h);
    return {w, h};
}
