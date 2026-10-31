#include "api/ConfigFileStore.h"
#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>

struct FakeFS {
    std::map<std::string, std::string> files;
    int cut = -1, operations = 0;
    bool partial = false;
    void checkpoint() { if (++operations == cut) throw std::runtime_error("power loss"); }
    bool exists(const std::string& path) { return files.count(path); }
    std::string read(const std::string& path) { auto it = files.find(path); return it == files.end() ? "" : it->second; }
    bool write(const std::string& path, const std::string& text) {
        files[path] = partial ? text.substr(0, text.size() / 2) : text;
        checkpoint(); return !partial;
    }
    bool remove(const std::string& path) { files.erase(path); checkpoint(); return true; }
    bool rename(const std::string& from, const std::string& to) {
        if (!exists(from) || exists(to)) return false;
        files[to] = files[from]; files.erase(from); checkpoint(); return true;
    }
};

int main() {
    using ConfigFileStore::Root;
    const std::string timers = R"([{"io":15,"tStart":"06:00:00","tStop":"23:00:00","enabled":true}])";
    const std::string disabled = R"([{"io":15,"tStart":"06:00:00","tStop":"23:00:00","enabled":false}])";
    const std::string object = R"({"io_pins":[]})";
    assert(ConfigFileStore::valid(object));
    assert(!ConfigFileStore::valid(timers)); // System/I/O defaults remain object-only.
    assert(ConfigFileStore::valid(timers, Root::Array));
    assert(ConfigFileStore::valid("[]", Root::Array));
    assert(!ConfigFileStore::valid(object, Root::Array));
    for (const auto* invalid : {"", "{", "[", "null", "true", "42", "\"text\""}) {
        assert(!ConfigFileStore::valid(invalid));
        assert(!ConfigFileStore::valid(invalid, Root::Array));
    }

    FakeFS fresh;
    assert(!ConfigFileStore::save(fresh, "/system.json", timers));
    assert(fresh.files.empty());
    assert(ConfigFileStore::save(fresh, "/timers.json", timers, Root::Array));
    assert(ConfigFileStore::read(fresh, "/timers.json", Root::Array) == timers);
    assert(ConfigFileStore::save(fresh, "/timers.json", disabled, Root::Array));
    assert(ConfigFileStore::read(fresh, "/timers.json", Root::Array) == disabled);
    assert(!fresh.exists("/timers.json.bak") && !fresh.exists("/timers.json.tmp"));

    for (int cut = 1; cut <= 5; ++cut) {
        FakeFS fs; fs.files["/timers.json"] = timers; fs.cut = cut;
        try { ConfigFileStore::save(fs, "/timers.json", disabled, Root::Array); }
        catch (const std::runtime_error&) {}
        const auto recovered = ConfigFileStore::read(fs, "/timers.json", Root::Array);
        assert(recovered == timers || recovered == disabled);
    }
    FakeFS partial; partial.files["/timers.json"] = timers; partial.partial = true;
    assert(!ConfigFileStore::save(partial, "/timers.json", disabled, Root::Array));
    assert(ConfigFileStore::read(partial, "/timers.json", Root::Array) == timers);

    FakeFS backup;
    backup.files["/timers.json"] = "{";
    backup.files["/timers.json.bak"] = timers;
    backup.files["/timers.json.tmp"] = disabled;
    assert(ConfigFileStore::read(backup, "/timers.json", Root::Array) == timers);
    assert(!ConfigFileStore::save(backup, "/timers.json", disabled, Root::Array));
    assert(backup.files["/timers.json.bak"] == timers);
    backup.files["/timers.json"] = object;
    assert(ConfigFileStore::read(backup, "/timers.json", Root::Array) == timers);
    backup.files["/system.json"] = timers;
    backup.files["/system.json.bak"] = object;
    assert(ConfigFileStore::read(backup, "/system.json") == object);
    assert(!ConfigFileStore::save(backup, "/system.json", object));
    assert(backup.files["/system.json.bak"] == object);
    std::cout << "PASS: typed object/array config roots, real timer arrays, atomic interruption recovery and strict object backup guards\n";
}
