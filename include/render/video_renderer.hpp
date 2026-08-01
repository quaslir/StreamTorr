#include "smart_items.hpp"
#include <SDL2/SDL.h>
extern "C" {
#include <libavutil/frame.h>
}



class VideoRenderer {
  private:
    smart_window window_{nullptr};
    smart_texture texture_{nullptr};
    smart_renderer renderer_{nullptr};

    int texture_width_{0};
    int texture_height_{0};

    bool texture_is_valid_{false};

  public:
    VideoRenderer() = default;
    VideoRenderer(const VideoRenderer &) = delete;
    VideoRenderer &operator=(const VideoRenderer &) = delete;

    VideoRenderer(VideoRenderer &&) noexcept = default;
    VideoRenderer &operator=(VideoRenderer &&) noexcept = default;

    bool open(int width, int height, const char *window_title = "StreamTorr");
    bool update_texture(const AVFrame *frame);
    void draw_frame();
    void present();
    SDL_Renderer *renderer() const;
    std::pair<int, int> window_size() const;
    void toggle_fullscreen(bool fullscreen_on = true);
    void close();
};
