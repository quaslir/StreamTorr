#pragma once
#include "smart_items.hpp"
enum class DecoderSendResult {
    Ok,
    NeedsMoreOutput,
    Error
};

enum class PacketType {
    VIDEO,
    AUDIO,
    OTHER,
    ERROR
};

struct DemuxedPacket {
    PacketType type;
    smart_packet packet;
};
