#include "api/IOConfig.h"
#include "api/ConfigFileStore.h"
#include <cassert>
#include <map>
#include <iostream>
#include <stdexcept>
using namespace IOAutomation;
struct FakeFS {
    std::map<std::string,std::string> files; int cut = -1, operations = 0; bool partial = false, failRename = false;
    void checkpoint() { if (++operations == cut) throw std::runtime_error("power loss"); }
    bool exists(const std::string& p) { return files.count(p); }
    std::string read(const std::string& p) { return exists(p) ? files[p] : ""; }
    bool write(const std::string& p, const std::string& value) { files[p] = partial ? value.substr(0, value.size()/2) : value; checkpoint(); return !partial; }
    bool remove(const std::string& p) { files.erase(p); checkpoint(); return true; }
    bool rename(const std::string& a, const std::string& b) { if (failRename && b == "/c.json") return false; if (!exists(a) || exists(b)) return false; files[b] = files[a]; files.erase(a); checkpoint(); return true; }
};
int main() {
    JsonDocument doc;
    deserializeJson(doc, R"({"io_pins":[{"gpio":32,"label":"Sonde","mode":"INPUT","type":"ANALOG_INPUT"},{"gpio":15,"label":"Pompe","mode":"OUTPUT"}],"scenes":[{"id":"water","name":"Arroser","actions":[{"gpio":15,"active":true,"duration_ms":1000}]}],"automation":{"paused":false,"timezone":"America/Toronto","schedules":[{"id":"am","name":"Matin","enabled":true,"scene_id":"water","time":"08:00","days":[1,2,3,4,5]}],"rules":[{"id":"dry","name":"Sec","enabled":true,"scene_id":"water","gpio":32,"operator":"below","threshold":30,"hysteresis":5,"stable_ms":500,"cooldown_ms":1000}]}})");
    assert(!IOConfig::validate(doc)); auto rule = doc["automation"]["rules"][0].as<JsonVariantConst>();
    RuleState state; assert(!evaluate(rule,state,20,100)); // No automatic startup.
    assert(!evaluate(rule,state,36,200)); assert(!evaluate(rule,state,36,700)); assert(!state.condition);
    assert(!evaluate(rule,state,29,800)); assert(evaluate(rule,state,29,1300)); assert(!evaluate(rule,state,29,1800));
    assert(!evaluate(rule,state,32,2000)); assert(state.condition); // Hysteresis retains active.
    assert(!evaluate(rule,state,36,2100)); assert(!evaluate(rule,state,36,2600)); assert(!state.condition);
    assert(!evaluate(rule,state,29,2700)); assert(evaluate(rule,state,29,3200));
    state.unavailable(); assert(!evaluate(rule,state,20,4000)); assert(!evaluate(rule,state,20,5000));
    RuleState cooldown; evaluate(rule,cooldown,40,0); evaluate(rule,cooldown,20,1); assert(evaluate(rule,cooldown,20,501)); evaluate(rule,cooldown,40,502); evaluate(rule,cooldown,40,1002); evaluate(rule,cooldown,20,1003); assert(evaluate(rule,cooldown,20,1503));
    doc["automation"]["rules"][0]["cooldown_ms"] = 10000;
    RuleState consumed; evaluate(rule,consumed,40,0); evaluate(rule,consumed,20,1); assert(evaluate(rule,consumed,20,501)); evaluate(rule,consumed,40,502); evaluate(rule,consumed,40,1002); evaluate(rule,consumed,20,1003); assert(!evaluate(rule,consumed,20,1503)); assert(!evaluate(rule,consumed,20,15000));
    assert(!evaluate(rule,consumed,NAN,16000)); assert(!evaluate(rule,consumed,20,17000));
    RuleState wrap; evaluate(rule,wrap,40,0xfffffe00U); evaluate(rule,wrap,20,0xffffff00U); assert(evaluate(rule,wrap,20,0x00000100U));
    assert(offset(1772953199,"America/Toronto") == -18000); assert(offset(1772953200,"America/Toronto") == -14400);
    assert(offset(1793512799,"America/Toronto") == -14400); assert(offset(1793512800,"America/Toronto") == -18000);
    assert(offset(1793512800,"UTC") == 0);
    // Window boundaries and overnight hours use the configured local timezone.
    auto window = doc["automation"]["rules"][0]["window"].to<JsonObject>(); window["start"]="08:00"; window["end"]="20:00";
    assert(!IOConfig::validate(doc));
    const time_t midnight = 1791849600; // 2026-10-13 00:00 UTC.
    assert(!windowAllowed(rule,midnight+8*3600-1,"UTC")); assert(windowAllowed(rule,midnight+8*3600,"UTC"));
    assert(windowAllowed(rule,midnight+20*3600-1,"UTC")); assert(!windowAllowed(rule,midnight+20*3600,"UTC"));
    assert(!windowAllowed(rule,midnight+8*3600,"America/Toronto")); assert(windowAllowed(rule,midnight+12*3600,"America/Toronto"));
    assert(!windowAllowed(rule,100,"UTC"));
    window["start"]="22:00";window["end"]="06:00";
    assert(windowAllowed(rule,midnight+23*3600,"UTC")); assert(windowAllowed(rule,midnight+5*3600,"UTC")); assert(!windowAllowed(rule,midnight+6*3600,"UTC"));
    window["start"]="08:00";window["end"]="20:00";doc["automation"]["rules"][0]["cooldown_ms"]=1000;
    RuleState gated;
    assert(attempt(rule,gated,40,0,midnight,"UTC")==Attempt::NONE);
    assert(attempt(rule,gated,20,1,midnight,"UTC")==Attempt::NONE);
    assert(attempt(rule,gated,20,501,midnight,"UTC")==Attempt::OUTSIDE_WINDOW);
    assert(attempt(rule,gated,20,2000,midnight+8*3600,"UTC")==Attempt::NONE); // Opening never replays a consumed edge.
    assert(attempt(rule,gated,40,2001,midnight+8*3600,"UTC")==Attempt::NONE);
    assert(attempt(rule,gated,40,2501,midnight+8*3600,"UTC")==Attempt::NONE);
    assert(attempt(rule,gated,20,2502,midnight+8*3600,"UTC")==Attempt::NONE);
    assert(attempt(rule,gated,20,3002,midnight+8*3600,"UTC")==Attempt::FIRE);
    RuleState noClock; attempt(rule,noClock,40,0,100,"UTC");attempt(rule,noClock,20,1,100,"UTC");
    assert(attempt(rule,noClock,20,501,100,"UTC")==Attempt::CLOCK_UNAVAILABLE);
    assert(attempt(rule,noClock,20,2000,midnight+8*3600,"UTC")==Attempt::NONE);
    RuleState initial;assert(attempt(rule,initial,20,0,midnight+8*3600,"UTC")==Attempt::NONE);assert(attempt(rule,initial,20,2000,midnight+8*3600,"UTC")==Attempt::NONE);
    for (const char* invalid : {"24:00", "8:00", "08:60", "abcde", "20:00"}) {window["start"]=invalid;assert(IOConfig::validate(doc));}
    window["start"]="08:00";window["end"]=true;assert(IOConfig::validate(doc));
    doc["automation"]["rules"][0].remove("window");assert(!IOConfig::validate(doc));assert(windowAllowed(rule,100,"UTC"));
    JsonObject schedule = doc["automation"]["schedules"][0].as<JsonObject>(); schedule["time"] = "01:30"; schedule["days"].to<JsonArray>().add(0);
    assert(next(schedule,1793508000,"America/Toronto") == 1793511000); // First repeated hour.
    ScheduleState ss; assert(!due(schedule,ss,1793510940,"America/Toronto")); assert(due(schedule,ss,1793511000,"America/Toronto")); assert(!due(schedule,ss,1793511030,"America/Toronto")); assert(!due(schedule,ss,1793514600,"America/Toronto"));
    schedule["time"] = "02:30"; assert(next(schedule,1772946000,"America/Toronto") == 1773556200); // Skip missing spring hour.
    ScheduleState boot; assert(!due(schedule,boot,1773556200,"America/Toronto")); assert(!due(schedule,boot,100,"America/Toronto"));
    doc["scenes"][0]["actions"][0].remove("duration_ms"); assert(IOConfig::validate(doc)); doc["scenes"][0]["actions"][0]["duration_ms"]=1000;
    doc["automation"]["rules"][0]["gpio"]=15; assert(IOConfig::validate(doc)); doc["automation"]["rules"][0]["gpio"]=32;
    doc["automation"]["schedules"][0]["time"]="24:00"; assert(IOConfig::validate(doc));
    const std::string old = R"({"v":1})", updated=R"({"v":2})";
    for (int cut=1;cut<=5;cut++) { FakeFS fs;fs.files["/c.json"]=old;fs.cut=cut;try { ConfigFileStore::save(fs,"/c.json",updated); } catch(const std::runtime_error&) {} auto recovered=ConfigFileStore::read(fs,"/c.json");assert(recovered==old || recovered==updated); }
    FakeFS fs; fs.files["/c.json"]=old;fs.partial=true; assert(!ConfigFileStore::save(fs,"/c.json",updated));assert(fs.read("/c.json")==old);
    fs.partial=false;fs.failRename=true;assert(!ConfigFileStore::save(fs,"/c.json",updated));assert(ConfigFileStore::read(fs,"/c.json")==old);
    fs.failRename=false;assert(ConfigFileStore::save(fs,"/c.json",updated));assert(fs.read("/c.json")==updated);assert(!fs.exists("/c.json.bak"));
    fs.files["/c.json"]="{";fs.files["/c.json.bak"]=old;assert(ConfigFileStore::read(fs,"/c.json")==old);assert(!ConfigFileStore::save(fs,"/c.json",updated));assert(fs.files["/c.json.bak"]==old);
    std::cout<<"PASS: rules, stability, hysteresis, cooldown/edges, unavailable/reboot, wraparound, DST schedules, validation and interrupted config saves\n";
}
