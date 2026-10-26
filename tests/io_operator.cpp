#include "api/IOConfig.h"
#include "api/IODiscovery.h"
#include <cassert>
#include <iostream>
int main() {
 JsonDocument d; deserializeJson(d,R"({"device":{"name":"Atelier","icon":"home"},"io_pins":[{"gpio":15,"label":"Ventilateur","zone":"Atelier","icon":"fan","mode":"OUTPUT"},{"gpio":4,"label":"Éclairage","zone":"Garage","icon":"light","mode":"OUTPUT"},{"gpio":21,"label":"Réservée","zone":"Atelier","mode":"OUTPUT"},{"gpio":32,"label":"Sonde","zone":"Atelier","type":"ANALOG_INPUT","mode":"INPUT"}],"scenes":[{"id":"fan","name":"Ventiler","icon":"fan","actions":[{"gpio":15,"active":true,"duration_ms":1000},{"gpio":4,"active":false}]}]})");
 assert(!IOConfig::validate(d));d["device"]["name"]="   ";assert(IOConfig::validate(d));d["device"]["name"]="Atelier";
 d["io_pins"][0]["icon"]="arbitrary";assert(IOConfig::validate(d));d["io_pins"][0]["icon"]="fan";
 std::vector<IOScenes::Action> actions;std::vector<int> skipped;
 assert(!IOOperator::zonePlan(d,"Atelier",[](int g){return g!=21;},actions,skipped));assert(actions.size()==1&&actions[0].gpio==15&&!actions[0].active);assert(skipped.size()==1&&skipped[0]==21);
 assert(IOOperator::zonePlan(d,"",[](int){return true;},actions,skipped));assert(actions.empty());
 assert(IOOperator::zonePlan(d,"Inconnue",[](int){return true;},actions,skipped));
 IOOutput::Timer holds[40];holds[15].start(0xfffffff0U,1000);assert(holds[15].remaining(0x10U)==968);
 assert(IOOperator::sceneHold(d,"fan",[&](int g){return holds[g].remaining(0x10U);})==968);
 assert(IOOperator::sceneHold(d,"all_off",[&](int g){return holds[g].remaining(0x10U);})==968);
 assert(holds[15].expired(0x400U));assert(!IOOperator::sceneHold(d,"fan",[&](int g){return holds[g].remaining(0x400U);}));
 assert(IOOperator::validOverride(0)&&IOOperator::validOverride(1000)&&IOOperator::validOverride(86400000));assert(!IOOperator::validOverride(999)&&!IOOperator::validOverride(86400001));
 JsonDocument payload;IODiscovery::scene(payload,"ESP_1","Atelier","http://board",d["scenes"][0]);assert(payload["unique_id"]=="ESP_1_scene_fan");assert(payload["command_topic"]=="esp32/ESP_1/io/scene");assert(payload["device"]["name"]=="Atelier");assert(payload["retain"]==false);JsonDocument command;deserializeJson(command,payload["payload_press"].as<std::string>());assert(command["id"]=="fan");
 IODiscovery::pause(payload,"ESP_1","Atelier","http://board");assert(payload["unique_id"]=="ESP_1_automation_pause");assert(payload["optimistic"]==false);assert(payload["command_topic"]=="esp32/ESP_1/automation/pause/set");
 // Scene availability remains a compact aggregate even for the maximum action count.
 JsonArray many=d["scenes"][0]["actions"].to<JsonArray>();for(int gpio=0;gpio<26;++gpio){JsonObject action=many.add<JsonObject>();action["gpio"]=gpio;action["active"]=true;action["duration_ms"]=1000;}
 IODiscovery::scene(payload,"ESP_1","Atelier","http://board",d["scenes"][0]);assert(measureJson(payload)<1800);assert(payload["availability"].size()==2);assert(payload["availability"][1]["topic"]=="esp32/ESP_1/io/scene/fan/state");
 IODiscovery::build(payload,"ESP_1",15,"Ventilateur","switch","http://board",JsonVariantConst(),"Atelier","fan");assert(payload["icon"]=="mdi:fan");assert(payload["unique_id"]=="ESP_1_gpio15");
 // Daily ON uses the same fail-closed pause and the selected output's hold.
 assert(IOOperator::dailyStartBlock(d["automation"],0));
 d["automation"]["paused"]=true;assert(IOOperator::dailyStartBlock(d["automation"],0));
 d["automation"]["paused"]=false;assert(!IOOperator::dailyStartBlock(d["automation"],0));
 assert(IOOperator::dailyStartBlock(d["automation"],1000));
 d["automation"]["paused"]="false";assert(IOOperator::dailyStartBlock(d["automation"],0));
 // A stable edge consumed while an actuator is held never fires after its release.
 JsonDocument rule;deserializeJson(rule,R"({"operator":"above","threshold":30,"hysteresis":5,"stable_ms":500,"cooldown_ms":1000})");IOAutomation::RuleState state;
 assert(!IOAutomation::evaluate(rule,state,20,0));assert(!IOAutomation::evaluate(rule,state,40,1));assert(IOAutomation::evaluate(rule,state,40,501));assert(!IOAutomation::evaluate(rule,state,40,2000));
 assert(!IOAutomation::evaluate(rule,state,20,2001));assert(!IOAutomation::evaluate(rule,state,20,2501));assert(!IOAutomation::evaluate(rule,state,40,2502));assert(IOAutomation::evaluate(rule,state,40,3002));
 std::cout<<"PASS: zone isolation/reserved skips, metadata validation, override expiry/wrap, no delayed edges, stable HA scene/pause identities\n";
}
