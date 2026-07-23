#include "decoder/clock.hpp"
#include <atomic>

void Clock::update(double pts_seconds) {
time.store(pts_seconds, std::memory_order_relaxed);
}

double Clock::get_time() const {
    return time.load(std::memory_order_relaxed);
}
