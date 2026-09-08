#include "my_timer.h"

void myShuffle::timer::tick()
{
    _end = timep_t{};
    _start = ClockT::now(); 
}

double myShuffle::timer::tock() {
    _end = ClockT::now(); 
    return duration();
}

double myShuffle::timer::duration() const { 
    // Use gsl_Expects if your project supports it.
    return std::chrono::duration_cast<DT>(_end - _start).count() / 1000000.0; 
}