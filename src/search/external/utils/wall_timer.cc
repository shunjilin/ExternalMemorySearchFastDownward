#include "wall_timer.h"

#include <chrono>
#include <ostream>

using namespace std::chrono;

namespace utils {
    WallTimer::WallTimer() : start_time(high_resolution_clock::now()) {}

    void WallTimer::clear() {
        start_time = high_resolution_clock::time_point::min();
        end_time = high_resolution_clock::time_point::min();
    }

    // returns true if the timer is running
    bool WallTimer::is_started() const {
        return start_time != high_resolution_clock::time_point::min();
        //return start_time.time_since_epoch() != high_resolution_clock::duration(0);
    }

    bool WallTimer::is_stopped() const {
        return end_time != high_resolution_clock::time_point::min();
        //return end_time.time_since_epoch() != high_resolution_clock::duration(0);
    }

    void WallTimer::start() {
        start_time = high_resolution_clock::now();
    }

    void WallTimer::stop() {
        end_time = high_resolution_clock::now();
    }

    duration<float>::rep WallTimer::get_seconds() const {
        if (is_stopped()) {
            auto diff = end_time - start_time;
            return duration<float>(diff).count();
        } else if (is_started()) {
            auto diff = high_resolution_clock::now() - start_time;
            return duration<float>(diff).count();
        }
        return 0;
    }

    std::ostream &operator<<(std::ostream &os, const WallTimer &wall_timer) {
        os << wall_timer.get_seconds() << "s";
        return os;
    }

    WallTimer overall_wall_timer; // start a timer on inclusion
}
    

