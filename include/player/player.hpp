#include "decoder/smart_items.hpp"
#include "pipeline/pipeline.hpp"
#include "render/video_renderer.hpp"
#include "render/audio_renderer.hpp"
#include <chrono>
#include <libavutil/rational.h>
#include <thread>

enum class PlayerState
{
    Idle,
    Ready,
    Playing,
    Paused,
    Stopped,
    Finished,
    Buffering
};

constexpr double kLowWatermark = 1.0;
constexpr double kHighWatermark = 2.0;
class Player {
    private:
    Pipeline pipeline_;
    ProgressCallback progress_cb_;
    VideoRenderer video_renderer_;
    AudioRenderer audio_renderer_;
    Clock clock_;
    PlayerState state_{PlayerState::Idle};
    std::thread audio_thread_;

    std::optional<smart_frame> pending_frame_;
    AVRational video_time_base_{};
    AVRational audio_time_base_{};
    bool clock_primed{false};
    std::chrono::steady_clock::time_point   playback_start_real_;
    double playback_start_pts_{0.0};

    std::chrono::steady_clock::time_point pause_started_at_;
    void audio_loop();
    void seek(double seconds);
    void toggle_pause();
    public:

        bool open(const std::string& path);
        bool open_torrent(const std::string& magnet, const std::filesystem::path& download_dir);
        void set_progress_callback(ProgressCallback cb);
        void play();


        void update();

        void stop();

        PlayerState state() const;
};
