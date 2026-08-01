
#include <chrono>
extern "C" {
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
}
#include <string>
struct HitResult {
    bool seek_requested{false};
    double seek_to_seconds{0.0};
    bool play_pause_clicked{false};
    bool volume_changed{false};
    float new_volume{0.0f};
};
enum RenderEvent { NONE, WINDOW_CLOSED, PAUSE, SEEK_FORWARD, SEEK_BACKWARD, DISABLE_FULLSCREEN, ENABLE_FULLSCREEN };

struct FrameInput {
    RenderEvent event{RenderEvent::NONE};
    bool mouse_clicked{false};
    int mouse_x{0}, mouse_y{0};
};

class UIOverlay {
  private:
    TTF_Font *font_{nullptr};
    SDL_Rect progress_bar_bounds_{};
    SDL_Rect play_button_bounds_{};
    SDL_Rect volume_bar_bounds_{};
    std::chrono::steady_clock::time_point last_mouse_active_{};

    static std::string time_to_string(double seconds);
  public:
    // ~UIOverlay();
    bool open();
    void draw(SDL_Renderer *renderer, int window_w, int window_h, double current_time,
              double duration, float download_progress, bool is_playing, float volume);
    HitResult handle_click(int x, int y, double duration);
     FrameInput poll_events();
};
