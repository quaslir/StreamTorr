#include "io/torrent_io_context.hpp"
#include "torrent/torrent_client.hpp"
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sys/wait.h>
#include <thread>
extern "C" {
    #include <libavformat/avio.h>
    #include <libavutil/error.h>
    #include <libavutil/mem.h>
}


bool TorrentIOContext::open(TorrentClient * client, const std::filesystem::path& file_path, int64_t file_offset_in_torrent, int64_t total_size) {
    std::fprintf(stderr, "[io] open: file_offset_in_torrent=%lld, total_size=%lld\n",
        static_cast<long long>(file_offset_in_torrent), static_cast<long long>(total_size));
    if(!client) return false;
    client_ = client;
    path_ = file_path;
    file_offset_in_torrent_ = file_offset_in_torrent;
    total_size_ = total_size;
    current_position_ = 0;


    constexpr auto kFileWaitTimeout = std::chrono::seconds(30);
    auto wait_start = std::chrono::steady_clock::now();
    while(!std::filesystem::exists(file_path)) {
        if(std::chrono::steady_clock::now() - wait_start > kFileWaitTimeout) {
             std::fprintf(stderr, "[io] open: file never appeared: %s\n", file_path.c_str());
             return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
        std::ifstream file(file_path, std::ios::binary);
        if(!file.is_open()) return false;
    file_stream_ = std::move(file);
    constexpr size_t kBufferSize = 256 * 1024;
  avio_buffer_ =  static_cast<uint8_t*>(av_malloc(kBufferSize));
  if(!avio_buffer_) {
      return false;
  }

avio_context_ =   avio_alloc_context(avio_buffer_, kBufferSize, 0, this, &TorrentIOContext::read_packet_callback, nullptr,&TorrentIOContext::seek_callback);
if(!avio_context_) {
    av_free(avio_buffer_);
    avio_buffer_ = nullptr;
    return false;
}
return true;

}

int TorrentIOContext::read_packet(uint8_t * buf, int buf_size) {
    if(current_position_ >= total_size_) return AVERROR_EOF;
    int64_t remaining = total_size_ - current_position_;
    int to_read = static_cast<int>(std::min<int64_t>(buf_size, remaining));
    int64_t absolute_offset = current_position_ + file_offset_in_torrent_;
    constexpr int64_t kRepriorityStep = 4 * 1024 * 1024;
    constexpr uint64_t kPriorityWindowBytes = 4 * 1024 * 1024;
    if(last_prioritized_pos_ < 0 || std::abs(current_position_ - last_prioritized_pos_) > kRepriorityStep) {
        client_->prioritize_range(static_cast<uint64_t>(absolute_offset), kPriorityWindowBytes);
        last_prioritized_pos_ = current_position_;
    }

    constexpr uint32_t kTimeoutMs = 30000;
    if(!client_->wait_for_range(static_cast<uint64_t>(absolute_offset), static_cast<uint64_t>(to_read), kTimeoutMs))     return AVERROR(EIO);
    file_stream_.clear();
    file_stream_.seekg(current_position_);
    file_stream_.read(reinterpret_cast<char *>(buf), to_read);
   std::streamsize gcount =  file_stream_.gcount();
   if(gcount == 0){
       for(int attempt = 0; attempt < 5 && gcount == 0; attempt++) {
           std::this_thread::sleep_for(std::chrono::milliseconds(50));
           file_stream_.clear();
           file_stream_.seekg(current_position_);
           file_stream_.read(reinterpret_cast<char *>(buf), to_read);
           gcount = file_stream_.gcount();
       }
       if(gcount == 0) return AVERROR_EOF;
   }
   current_position_ += gcount;
   return static_cast<int>(gcount);

}

int TorrentIOContext::read_packet_callback(void * opaque, uint8_t * buf, int buf_size) {
    auto * self = static_cast<TorrentIOContext*>(opaque);
    return self->read_packet(buf, buf_size);
}

int64_t TorrentIOContext::seek(int64_t offset, int whence) {
    int64_t new_position = current_position_;
    switch(whence) {
        case SEEK_SET :
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position += offset;
            break;
        case SEEK_END :
            new_position = total_size_ + offset;
            break;
        case AVSEEK_SIZE:
            return total_size_;

        default: return -1;
    }
    if(new_position < 0 || new_position > total_size_) {
        return -1;
    }

    current_position_ = new_position;
    return current_position_;
}

int64_t TorrentIOContext::seek_callback(void * opaque, int64_t offset, int whence) {
    auto * self = static_cast<TorrentIOContext*>(opaque);
    return self->seek(offset, whence);
}

AVIOContext * TorrentIOContext::avio_context() const {
    return avio_context_;
}

TorrentIOContext::~TorrentIOContext() {
    if(avio_context_) {
        av_free(avio_context_->buffer);
        avio_context_free(&avio_context_);
    }
}
