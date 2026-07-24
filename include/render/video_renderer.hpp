#include "smart_items.hpp"
#include <SDL2/SDL.h>
extern "C" {
#include <libavutil/frame.h>
}

enum RenderEvent {
    NONE,
    WINDOW_CLOSED
};

class VideoRenderer {
    private:
        smart_window window_{nullptr};
        smart_texture texture_{nullptr};
        smart_renderer renderer_{nullptr};

        int texture_width_{0};
        int texture_height_{0};

        //bool sdl_video_initialized{false};


    public:
        VideoRenderer() = default;
        VideoRenderer(const VideoRenderer&) = delete;
        VideoRenderer& operator=(const VideoRenderer&) = delete;

        VideoRenderer(VideoRenderer&&) noexcept = default;
        VideoRenderer& operator=(VideoRenderer&&) noexcept = default;

        bool open(int width, int height,   const char* window_title = "StreamTorr");
        bool render_frame(const AVFrame* frame);
         RenderEvent poll_events();

        void close();
};
