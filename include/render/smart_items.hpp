#include <SDL2/SDL.h>
#include <memory>

struct SDLWindowDeleter {
    void operator()(SDL_Window* window) const {
        if (window) SDL_DestroyWindow(window);
    }
};


struct SDLRendererDeleter {
    void operator()(SDL_Renderer* renderer) const {
        if (renderer) SDL_DestroyRenderer(renderer);
    }
};


struct SDLTextureDeleter {
    void operator()(SDL_Texture* texture) const {
        if (texture) SDL_DestroyTexture(texture);
    }
};
using smart_texture = std::unique_ptr<SDL_Texture, SDLTextureDeleter>;
using smart_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using smart_renderer = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;
