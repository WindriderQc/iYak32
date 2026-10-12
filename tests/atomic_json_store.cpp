#include "api/AtomicJsonStore.h"
#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <iostream>
struct MemoryStore;
struct MemoryFile {
    MemoryStore* store; std::string path; bool valid; size_t position = 0;
    MemoryFile(MemoryStore* owner, const char* name, bool opened) : store(owner), path(name), valid(opened) {}
    explicit operator bool() const { return valid; }
    size_t size() const; int read(); size_t readBytes(char* buffer, size_t length);
    size_t write(uint8_t byte); size_t write(const uint8_t* bytes, size_t length);
    void flush(); void close() { valid = false; }
};
struct MemoryStore {
    std::map<std::string,std::string> files; int mutation = 0, crashAt = 0; bool shortWrite = false, failRename = false;
    void change() { if (++mutation == crashAt) throw std::runtime_error("power loss"); }
    MemoryFile open(const char* path, const char* mode) {
        if (!strcmp(mode,"w")) { change(); files[path] = ""; }
        return MemoryFile(this,path,exists(path));
    }
    bool exists(const char* path) const { return files.count(path); }
    bool remove(const char* path) { change(); files.erase(path); return true; }
    bool rename(const char* from, const char* to) {
        change(); if (failRename || !exists(from) || exists(to)) return false;
        files[to] = files[from]; files.erase(from); return true;
    }
};
size_t MemoryFile::size() const { return valid ? store->files.at(path).size() : 0; }
int MemoryFile::read() { return position < size() ? static_cast<unsigned char>(store->files[path][position++]) : -1; }
size_t MemoryFile::readBytes(char* buffer, size_t length) { size_t readCount = 0; int byte; while (readCount < length && (byte = read()) != -1) buffer[readCount++] = byte; return readCount; }
size_t MemoryFile::write(uint8_t byte) { return write(&byte,1); }
size_t MemoryFile::write(const uint8_t* bytes, size_t length) { store->change(); if (store->shortWrite) return 0; store->files[path].append(reinterpret_cast<const char*>(bytes),length); return length; }
void MemoryFile::flush() { store->change(); }
int value(MemoryStore& store) { JsonDocument doc; assert(AtomicJsonStore::read(store,"/timers",doc)); return doc["value"].as<int>(); }
int main() {
    JsonDocument next; next["value"] = 2;
    MemoryStore original; original.files["/timers"] = "{\"value\":1}"; original.files["/timers.bak"] = "{\"value\":0}";
    auto complete = original; assert(AtomicJsonStore::save(complete,"/timers",next)); assert(value(complete) == 2);
    const int steps = complete.mutation;
    for (int at = 1; at <= steps; ++at) {
        auto interrupted = original; interrupted.crashAt = at;
        try { AtomicJsonStore::save(interrupted,"/timers",next); } catch (const std::runtime_error&) {}
        interrupted.crashAt = 0; assert(value(interrupted) == 1);
    }
    auto corrupt = original; corrupt.files["/timers"] = "{half"; corrupt.files["/timers.bak"] = "{\"value\":1}";
    auto repaired = corrupt; assert(AtomicJsonStore::save(repaired,"/timers",next)); assert(value(repaired) == 2);
    for (int at = 1; at <= repaired.mutation; ++at) {
        auto interrupted = corrupt; interrupted.crashAt = at;
        try { AtomicJsonStore::save(interrupted,"/timers",next); } catch (const std::runtime_error&) {}
        interrupted.crashAt = 0; assert(value(interrupted) == 1);
    }
    auto partial = original; partial.shortWrite = true;
    assert(!AtomicJsonStore::save(partial,"/timers",next)); assert(value(partial) == 1);
    auto refused = original; refused.failRename = true;
    assert(!AtomicJsonStore::save(refused,"/timers",next)); assert(value(refused) == 1);
    std::cout << "PASS atomic JSON snapshots: interrupted writes/renames, corrupt primary, short write, refused rename\n";
}
