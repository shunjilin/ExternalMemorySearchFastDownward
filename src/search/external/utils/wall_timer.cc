#include "wall_timer.h"

#include <chrono>
#include <ostream>

using namespace std::chrono;

namespace utils {
    WallTimer::WallTimer() : start_time(steady_clock::now()) {}

    void WallTimer::reset() {
        start_time = steady_clock::now();
        stopped = false;
    }

    bool WallTimer::is_stopped() const {
        return stopped;
    }

    void WallTimer::start() {
        start_time = steady_clock::now();
    }

    void WallTimer::stop() {
        end_time = steady_clock::now();
        stopped = true;
    }

    duration<float>::rep WallTimer::get_seconds() const {
        if (is_stopped()) {
            auto diff = end_time - start_time;
            return duration<float>(diff).count();
        }
        auto diff = steady_clock::now() - start_time;
        return duration<float>(diff).count();    
    }

    std::ostream &operator<<(std::ostream &os, const WallTimer &wall_timer) {
        os << wall_timer.get_seconds() << "s";
        return os;
    }

    WallTimer overall_wall_timer; // start a timer on inclusion
}
    

