#include "api/DailyTimers.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>

static time_t at(int year, int month, int day, int h, int m = 0, int s = 0) {
    std::tm date{}; date.tm_year=year-1900; date.tm_mon=month-1; date.tm_mday=day;
    date.tm_hour=h; date.tm_min=m; date.tm_sec=s; date.tm_isdst=-1; return std::mktime(&date);
}
int main() {
    using namespace DailyTimers;
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1); tzset();
    unsigned io = 0, seconds = 0;
    assert(parseMessage("15:06:00:00", io, seconds) && io == 15 && seconds == 21600);
    for (const char* bad : {"", "15", "15:6:00:00", "15:24:00:00", "15:06:60:00", "40:06:00:00", "-1:06:00:00", "15:06:00:00:extra"}) assert(!parseMessage(bad, io, seconds));
    Pending pending; Schedule schedule;
    assert(!pending.receive(true, 21600, 100, schedule));
    assert(!pending.receive(false, 82800, 20100, schedule)); // start expired
    assert(pending.receive(true, 21600, 20101, schedule) && schedule.on == 21600 && schedule.off == 82800);
    assert(!pending.receive(false, 21600, 22000, schedule));
    assert(!pending.receive(true, 21600, 22001, schedule)); // equal times rejected
    assert(!pending.receive(true, 21600, 0xfffffff0, schedule));
    assert(pending.receive(false, 82800, 0x20, schedule)); // millis wrap
    std::vector<bool> writes; auto write = [&](bool value) { writes.push_back(value); return true; };
    Runtime run; schedule = {21600, 82800, true};
    run.tick(schedule, at(2026,1,1,12), true, true, write);
    assert(writes.empty() && run.onAt == at(2026,1,2,6)); // mid-window reboot stays OFF
    run.tick(schedule, at(2026,1,2,6), true, true, write);
    run.tick(schedule, at(2026,1,2,7), true, true, write);
    assert(writes == std::vector<bool>({true}));
    run.tick(schedule, at(2026,1,2,23), true, true, write);
    assert(writes == std::vector<bool>({true,false}));
    run.reset(); writes.clear(); schedule = {82800,21600,true};
    run.tick(schedule, at(2026,1,2,1), true,true,write);
    assert(run.onAt == at(2026,1,2,23) && run.offAt == at(2026,1,3,6));
    run.tick(schedule,at(2026,1,2,23),true,true,write);
    run.tick(schedule,at(2026,1,3,5),true,true,write);
    run.tick(schedule,at(2026,1,3,6),true,true,write);
    assert(writes == std::vector<bool>({true,false}));
    // Loss of clock validity turns a timed output off and requires the next start.
    run.tick(schedule,at(2026,1,3,23),true,true,write);
    run.tick(schedule,0,false,true,write);
    assert(!run.active && !run.onAt && writes.back() == false);
    run.tick(schedule,at(2026,1,4,1),true,true,write);
    assert(run.onAt == at(2026,1,4,23));
    // A clock rollback never reactivates the current interval.
    run.tick(schedule,at(2026,1,4,23),true,true,write);
    run.tick(schedule,at(2026,1,4,22),true,true,write);
    assert(!run.active && writes.back() == false);
    run.tick(schedule,at(2026,1,4,23),true,false,write);
    assert(!run.onAt); // reconfigured/reserved pins cannot execute
    schedule = {21600,82800,true}; run.reset();
    const time_t spring = at(2026,3,7,6);
    assert(nextStart(spring,21600)-spring == 23*3600);
    const time_t autumn = at(2026,10,31,6);
    assert(nextStart(autumn,21600)-autumn == 25*3600);
    std::puts("PASS: legacy parsing, complete pairs, expiry/wrap, cold boot, overnight windows, clock loss/rollback, unavailable pins and DST days");
}
