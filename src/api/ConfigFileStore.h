#pragma once
#include <ArduinoJson.h>
#include <string>

// The adapter supplies exists/read/write/remove/rename. Testable without SPIFFS or hardware.
namespace ConfigFileStore {
enum class Root { Object, Array };
inline bool valid(const std::string& text, Root root = Root::Object) {
    if (text.empty()) return false;
    JsonDocument doc;
    if (deserializeJson(doc, text)) return false;
    return root == Root::Array ? doc.is<JsonArray>() : doc.is<JsonObject>();
}
template<class FS> std::string read(FS& fs, const std::string& path, Root root = Root::Object) {
    std::string main = fs.read(path);
    if (valid(main, root)) return main;
    std::string backup = fs.read(path + ".bak");
    // A staged .tmp is never committed merely by rebooting.
    return valid(backup, root) ? backup : main;
}
template<class FS> bool save(FS& fs, const std::string& path, const std::string& content, Root root = Root::Object) {
    if (!valid(content, root)) return false;
    const std::string temp = path + ".tmp", backup = path + ".bak";
    if (!fs.write(temp, content) || fs.read(temp) != content) return false;
    bool hasMain = fs.exists(path), mainValid = valid(fs.read(path), root);
    // Preserve a recoverable backup if the primary cannot be read or parsed.
    if (hasMain && !mainValid) return false;
    if (hasMain) {
        if (fs.exists(backup) && !fs.remove(backup)) return false;
        if (!fs.rename(path, backup)) return false;
    }
    if (!fs.rename(temp, path)) {
        if (hasMain) fs.rename(backup, path);
        return false;
    }
    if (fs.exists(backup)) fs.remove(backup);
    return true;
}
}
