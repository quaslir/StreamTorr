
#include <atomic>
class Clock {
    private:
    std::atomic<double> time;

    void update(double pts_seconds);

    double get_time() const;

};
