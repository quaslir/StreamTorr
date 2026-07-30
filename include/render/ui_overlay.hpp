
extern "C" {
#include <SDL2/SDL.h>
#include <SDL_ttf.h>
}
#include <string>
struct HitResult {
    bool seek_requested;
    double seek_to_seconds;
    bool play_pause_clicked;
    bool volume_changed{false};
    float new_volume{0.0f};
};

class UIOverlay {
  private:
    TTF_Font *font_{nullptr};
    SDL_Rect progress_bar_bounds_{};
    SDL_Rect play_button_bounds_{};
    SDL_Rect volume_bar_bounds_{};
    bool visible_{true};

    static std::string time_to_string(double seconds);
  public:
    // ~UIOverlay();
    bool open();
    void draw(SDL_Renderer *renderer, int window_w, int window_h, double current_time,
              double duration, float download_progress, bool is_playing, float volume);
    // HitResult handle_click(int x, int y, int w, int h);
};
