#include "decoder/demuxer.hpp"
#include "decoder/smart_items.hpp"
#include "decoder/video_decoder.hpp"
#include "decoder/audio_decoder.hpp"
#include "decoder/frame_queue.hpp"
#include "decoder/clock.hpp"
#include "io/torrent_io_context.hpp"
#include "media/audio_resampler.hpp"
#include "media/video_resampler.hpp"
#include <libavutil/rational.h>
#include <filesystem>
#include "torrent/torrent_client.hpp"
#include "io/torrent_io_context.hpp"
#include <thread>
class Pipeline {
    private:
                TorrentIOContext io_context_;
        Demuxer demuxer_;
        VideoDecoder video_decoder_;
        AudioDecoder audio_decoder_;
        FrameQueue<smart_frame> video_queue_;
        FrameQueue<smart_frame> audio_queue_;
        AudioResampler audio_resampler_;
        VideoResampler video_resampler_;
        Clock clock_;
        TorrentClient torrent_client_;

        std::thread demux_thread_;
        std::atomic<bool> running_{false};

        bool video_resampler_ready_{false};
        void demux_loop();
        void decode_video_packet(const AVPacket* packet);
        void decode_audio_packet(const AVPacket* packet);
    public:
        Pipeline();
        bool open(const std::string& filename);
        bool open_torrent(const std::string& magnet, const std::filesystem::path& download_dir);
        void start();
        void stop();

        FrameQueue<smart_frame>& video_frames();
        FrameQueue<smart_frame>& audio_frames();
        Clock& clock();

        AVRational audio_time_base() const;
        AVRational video_time_base() const;
        std::optional<std::pair<int, int>> video_stream_size() const;

        bool has_audio() const;
};
