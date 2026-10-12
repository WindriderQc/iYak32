#pragma once
#include <ArduinoJson.h>
#include <string>

// Host-testable storage adapter. A complete previous snapshot remains readable
// at every interrupted rename; a corrupt primary never replaces a good backup.
namespace AtomicJsonStore {
template<class Storage>
bool read(Storage& store, const char* path, JsonDocument& doc) {
    const std::string backup = std::string(path)+".bak";
    const char* candidates[] = {path, backup.c_str()};
    for (const char* candidate : candidates) {
        auto file = store.open(candidate,"r");
        if (!file || file.size() > 8192) { file.close(); continue; }
        doc.clear(); const bool ok = !deserializeJson(doc,file); file.close();
        if (ok) return true;
    }
    doc.clear(); return false;
}
template<class Storage>
bool save(Storage& store, const char* path, const JsonDocument& doc) {
    const std::string staged = std::string(path)+".new", backup = std::string(path)+".bak";
    const size_t expected = measureJson(doc);
    if (expected > 8192) return false;
    auto file = store.open(staged.c_str(),"w");
    if (!file) return false;
    const size_t bytes = serializeJson(doc,file); file.flush(); file.close();
    auto verify = store.open(staged.c_str(),"r"); JsonDocument checked;
    bool ok = bytes == expected && verify && verify.size() == expected && !deserializeJson(checked,verify);
    verify.close(); if (!ok) return false;
    auto current = store.open(path,"r"); JsonDocument previous;
    bool valid = current && current.size() <= 8192 && !deserializeJson(previous,current);
    current.close();
    if (store.exists(path)) {
        if (valid) {
            if (store.exists(backup.c_str()) && !store.remove(backup.c_str())) return false;
            if (!store.rename(path,backup.c_str())) return false;
        } else if (!store.remove(path)) return false; // Keep the valid backup.
    }
    if (!store.rename(staged.c_str(),path)) {
        if (!store.exists(path) && store.exists(backup.c_str())) store.rename(backup.c_str(),path);
        return false;
    }
    return true;
}
}
