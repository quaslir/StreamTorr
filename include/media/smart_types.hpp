#pragma once

extern "C" {
#include <libswresample/swresample.h>
}

#include <memory>

struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const {
        if (ctx) {
            swr_free(&ctx);
        }
    }
};

using smart_swr = std::unique_ptr<SwrContext, SwrContextDeleter>;
