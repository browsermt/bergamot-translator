#ifndef __BERGAMOT_TIMER_H 
#define __BERGAMOT_TIMER_H

// https://stackoverflow.com/a/19800231/4565794
//
// Careful: This won't work if the user changes his time between Timer() and
// the call to elapsed() if !std::chrono::high_resolution_clock::is_steady -
// which is the case on Linux!

#include <iostream>
#include <chrono>

namespace marian {
namespace bergamot {
class Timer {
public:
    Timer() : beg_(clock_::now()) {}
    void reset() { beg_ = clock_::now(); }
    double elapsed() const { 
        return std::chrono::duration_cast<second_>
            (clock_::now() - beg_).count(); }

private:
    typedef std::chrono::high_resolution_clock clock_;
    typedef std::chrono::duration<double, std::ratio<1> > second_;
    std::chrono::time_point<clock_> beg_;
};

} // namespace bergamot 
} // namespace marian

#endif // __BERGAMOT_TIMER_H
