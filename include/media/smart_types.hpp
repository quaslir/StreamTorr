#pragma once

extern "C" {
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
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

struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const { if (ctx) sws_freeContext(ctx); }
};
using smart_sws = std::unique_ptr<SwsContext, SwsContextDeleter>;
