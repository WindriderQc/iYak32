#include "api/IOOutput.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace IOOutput;
    assert(level(true, false) && !level(false, false));
    assert(!level(true, true) && level(false, true));
    assert(active(false, true) && !active(true, true));
    assert(!validDuration(0) && !validDuration(99));
    assert(validDuration(100) && validDuration(3600000) && !validDuration(3600001));
    Timer timer;
    assert(!timer.expired(1000) && timer.remaining(1000) == 0);
    timer.start(1000, 5000);
    assert(timer.remaining(4999) == 1001 && !timer.expired(5999));
    assert(timer.expired(6000) && timer.remaining(6001) == 0);
    timer.start(0xfffffff0, 100);
    assert(timer.remaining(0x20) == 52 && timer.expired(0x54));
    timer.start(500, 1000); timer.cancel();
    assert(!timer.expired(5000) && timer.remaining(5000) == 0);
    timer.start(100, 2000); timer.start(200, 3000);
    assert(timer.remaining(2100) == 1100);
    std::cout << "PASS: active-low outputs, pulse duration limits, expiry, replacement, cancellation and millis wraparound\n";
}
