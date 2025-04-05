#pragma once
#include <chrono>

namespace myShuffle {
    typedef std::chrono::microseconds DT;
    typedef std::chrono::steady_clock ClockT;

    // A helper class for recording time elapse.
    class timer
    {
        using timep_t = decltype(ClockT::now());
        
        timep_t _start = ClockT::now();
        timep_t _end = {};

    public:
        timer() = default;

        void tick();
        
        double tock();
        
        double duration() const;
    };
}
