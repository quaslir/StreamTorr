#include "decoder/demuxer.hpp"
#include <assert.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
#include <optional>
#include <utility>

Demuxer::Demuxer() : format_context(avformat_alloc_context()) {}

bool Demuxer::open(const std::string& filename) {
    AVFormatContext* raw_ctx = format_context.release();

if(avformat_open_input(&raw_ctx, filename.c_str(), nullptr, nullptr) < 0) {
    return false;
}
format_context.reset(raw_ctx);
if(avformat_find_stream_info(format_context.get(), nullptr) < 0) {
    return false;
}

int video_index = av_find_best_stream(format_context.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

if(video_index < 0) return false;

video_stream_index = video_index;

int audio_index = av_find_best_stream(format_context.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

audio_stream_index = audio_index;

open_ = true;
return true;

}


bool Demuxer::is_open() const {
    return open_;
}

bool Demuxer::has_video() const {
    return video_stream_index >= 0;
}
bool Demuxer::has_audio() const {
    return audio_stream_index >= 0;
}

std::optional<AVCodecParameters *> Demuxer::video_stream_info() const {
    if(!has_video()) return std::nullopt;

    return format_context->streams[video_stream_index]->codecpar;

}

std::optional<AVCodecParameters *> Demuxer::audio_stream_info() const {
if(!has_audio()) return std::nullopt;

return  format_context->streams[audio_stream_index]->codecpar;
}

std::optional<DemuxedPacket> Demuxer::read_next_packet() {
    smart_packet packet{av_packet_alloc()};
    int result = av_read_frame(format_context.get(), packet.get());

    if(result == AVERROR_EOF) {
        return std::nullopt;
    }

    if(result < 0) {
        return DemuxedPacket(PacketType::ERROR, std::move(packet));
    }
    PacketType packet_type;
    if(packet->stream_index == video_stream_index) packet_type = PacketType::VIDEO;
    else if(packet->stream_index == audio_stream_index) packet_type = PacketType::AUDIO;
    else packet_type = PacketType::OTHER;

    return DemuxedPacket(packet_type, std::move(packet));

}

AVRational Demuxer::audio_time_base() const {
    return format_context->streams[audio_stream_index]->time_base;
}

AVRational Demuxer::video_time_base() const {
    assert(has_video());
    return format_context->streams[video_stream_index]->time_base;
}

std::optional<std::pair<int, int>> Demuxer::video_stream_size() const {
    AVCodecParameters * codecpar = format_context->streams[video_stream_index]->codecpar;
    if(!codecpar) return std::nullopt;

    return std::make_pair(codecpar->width, codecpar->height);
}
