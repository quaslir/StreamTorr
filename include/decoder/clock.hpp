
#include <atomic>
class Clock {
    private:
    std::atomic<double> time;
    public:
    void update(double pts_seconds);

    double get_time() const;

};
