#include "decoder/audio_decoder.hpp"
#include "decoder/clock.hpp"
#include "decoder/demuxer.hpp"
#include "decoder/frame_queue.hpp"
#include "decoder/smart_items.hpp"
#include "decoder/video_decoder.hpp"
#include "io/torrent_io_context.hpp"
#include "media/audio_resampler.hpp"
#include "media/video_resampler.hpp"
#include "decoder/subtitle_decoder.hpp"
#include "torrent/torrent_client.hpp"
#include <cstdint>
#include <filesystem>
#include <libavcodec/packet.h>
#include <libavutil/rational.h>
#include <thread>
class Pipeline {
  private:
    TorrentIOContext io_context_;
    Demuxer demuxer_;
    std::mutex pipeline_mutex_;
   mutable std::mutex subtitle_mutex_;
    VideoDecoder video_decoder_;
    AudioDecoder audio_decoder_;
    SubtitleDecoder subtitle_decoder_;
    FrameQueue<smart_frame> video_queue_;
    FrameQueue<smart_frame> audio_queue_;
    std::vector<SubtitleEvent> subtitle_events_;
    AudioResampler audio_resampler_;
    VideoResampler video_resampler_;
    Clock clock_;
    TorrentClient torrent_client_;
    ProgressCallback progress_cb_;
    std::thread demux_thread_;
    std::atomic<bool> running_{false};

    bool video_resampler_ready_{false};
    std::atomic<double> latest_video_pts_seconds_{0.0};

    int64_t file_offset_in_torrent_{0};
    int64_t file_size_{0};
    bool is_torrent_{false};
    bool subtitle_decoder_ready_{false};
    void demux_loop();
    void decode_video_packet(const AVPacket *packet);
    void decode_audio_packet(const AVPacket *packet);
    void decode_subtitle_packet(const AVPacket* packet);
    bool open();

  public:
    Pipeline();
    bool open_local(const std::string &filename);
    bool open_torrent(const std::string &magnet, const std::filesystem::path &download_dir);
    void set_progress_callback(ProgressCallback cb);
    void start();
    void stop();
    bool seek(double seconds);

    double buffered_seconds() const;
    FrameQueue<smart_frame> &video_frames();
    FrameQueue<smart_frame> &audio_frames();
    std::optional<std::string> current_subtitle_text() const;
    Clock &clock();

    AVRational audio_time_base() const;
    AVRational video_time_base() const;
    std::optional<std::pair<int, int>> video_stream_size() const;

    bool has_audio() const;

    double duration_seconds() const;
    float overall_progress() const;
};
