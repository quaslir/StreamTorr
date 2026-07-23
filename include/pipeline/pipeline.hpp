#include "decoder/demuxer.hpp"
#include "decoder/smart_items.hpp"
#include "decoder/video_decoder.hpp"
#include "decoder/audio_decoder.hpp"
#include "decoder/frame_queue.hpp"
#include "decoder/clock.hpp"
#include <thread>
class Pipeline {
    private:
        Demuxer demuxer_;
        VideoDecoder video_decoder_;
        AudioDecoder audio_decoder_;
        FrameQueue<smart_frame> video_queue_;
        FrameQueue<smart_frame> audio_queue_;
        Clock clock_;

        std::thread demux_thread_;
        std::atomic<bool> running_{false};

        void demux_loop();
        void decode_video_packet(const AVPacket* packet);
        void decode_audio_packet(const AVPacket* packet);
    public:
        Pipeline();
        bool open(const std::string& filename);
        void start();
        void stop();

        FrameQueue<smart_frame>& video_frames();
        FrameQueue<smart_frame>& audio_frames();
        Clock& clock();
};
