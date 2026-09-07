#include "uo/time.hpp"
#include <chrono>

namespace uo {

Tick SteadyClock::now_tick() const {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<Tick>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

WallTime SystemWallClock::now_wall() const {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<WallTime>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

} // namespace uo
