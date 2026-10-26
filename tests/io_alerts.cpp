#include "api/IOConfig.h"
#include "api/IOActivity.h"
#include <cassert>
#include <iostream>
using namespace IOAlerts;
int main() {
    JsonDocument doc;
    deserializeJson(doc,R"({"io_pins":[{"gpio":32,"label":"Sol","mode":"INPUT","type":"ANALOG_INPUT"}],"alerts":[{"id":"dry","name":"Sol sec","enabled":true,"source":"gpio","gpio":32,"operator":"below","threshold":30,"hysteresis":5,"stable_ms":500}]})");
    assert(!IOConfig::validate(doc)); auto alert=doc["alerts"][0].as<JsonVariantConst>(); State state;
    assert(evaluate(alert,state,20,100)==Change::NONE);assert(evaluate(alert,state,20,599)==Change::NONE);assert(evaluate(alert,state,20,600)==Change::ACTIVE);assert(evaluate(alert,state,20,5000)==Change::NONE);
    assert(evaluate(alert,state,34,5100)==Change::NONE);assert(state.active); // Hysteresis.
    assert(evaluate(alert,state,35,5200)==Change::NONE);assert(evaluate(alert,state,34,5500)==Change::NONE);assert(state.active); // Unstable recovery is rejected.
    assert(evaluate(alert,state,35,5600)==Change::NONE);assert(evaluate(alert,state,35,6100)==Change::CLEARED);
    assert(evaluate(alert,state,29,6200)==Change::NONE);assert(evaluate(alert,state,31,6400)==Change::NONE);assert(evaluate(alert,state,29,7000)==Change::NONE);assert(evaluate(alert,state,29,7500)==Change::ACTIVE);
    assert(evaluate(alert,state,NAN,7600)==Change::NONE);assert(!state.active&&!state.initialized);assert(evaluate(alert,state,20,8000)==Change::NONE);assert(evaluate(alert,state,20,8500)==Change::ACTIVE);
    State wrap;assert(evaluate(alert,wrap,0,0xffffff00U)==Change::NONE);assert(evaluate(alert,wrap,0,0x100U)==Change::ACTIVE);
    doc["alerts"][0]["source"]="temperature";doc["alerts"][0]["operator"]="above";assert(!IOConfig::validate(doc));State high;
    assert(evaluate(alert,high,31,0)==Change::NONE);assert(evaluate(alert,high,31,500)==Change::ACTIVE);assert(evaluate(alert,high,26,600)==Change::NONE);assert(high.active);assert(evaluate(alert,high,25,700)==Change::NONE);assert(evaluate(alert,high,25,1200)==Change::CLEARED);
    doc["alerts"][0]["source"]="gpio";doc["io_pins"][0]["type"]="DIGITAL";assert(IOConfig::validate(doc));doc["io_pins"][0]["type"]="ANALOG_INPUT";
    doc["alerts"][0]["gpio"]=33;assert(IOConfig::validate(doc));doc["alerts"][0]["gpio"]=32;doc["alerts"][0]["hysteresis"]=-1;assert(IOConfig::validate(doc));doc["alerts"][0]["hysteresis"]=0;
    doc["alerts"][0]["stable_ms"]=true;assert(IOConfig::validate(doc));doc["alerts"][0]["stable_ms"]=500;assert(!IOConfig::validate(doc));doc["alerts"].as<JsonArray>().add(doc["alerts"][0]);assert(IOConfig::validate(doc));doc.remove("alerts");assert(!IOConfig::validate(doc));
    IOActivity::Journal<3> journal;IOActivity::Event e;e.io.trace=42;e.kind=IOActivity::Kind::RULE;std::strcpy(e.io.label,"Condition");journal.add(e);e.kind=IOActivity::Kind::SCENE;journal.add(e);e.kind=IOActivity::Kind::GPIO;e.io.kind=IOEvents::Kind::PULSE;e.io.active=true;journal.add(e);e.io.kind=IOEvents::Kind::EXPIRED;e.io.active=false;journal.add(e);
    assert(journal.size()==3&&journal.oldest()==2&&journal.latest()==4);for(size_t i=0;i<3;i++)assert(journal.at(i).io.trace==42);assert(journal.at(2).io.kind==IOEvents::Kind::EXPIRED&&!journal.at(2).io.active);
    std::cout<<"PASS: initial threshold alert/stability, hysteresis and noisy recovery, unavailable/recovery, zero/wraparound, validation/reference removal and bounded correlated activity\n";
}
