#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include <cctype>

namespace IOHomeAssistant {
inline bool entity(const char* value) {
    if (!value || !*value || std::strlen(value) > 192) return false;
    const char* dot = std::strchr(value, '.'); if (!dot || dot == value || !dot[1]) return false;
    for (const char* p = value; *p; ++p) if (*p != '.' && *p != '_' && !(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9')) return false;
    return !std::strchr(dot + 1, '.');
}
inline const char* validate(JsonVariantConst settings) {
    if (settings.isNull()) return nullptr;
    if (!settings.is<JsonObjectConst>()) return "Les liens Home Assistant doivent être un objet.";
    for (JsonPairConst field : settings.as<JsonObjectConst>()) {
        const char* key = field.key().c_str();
        if (std::strcmp(key, "url") && std::strcmp(key, "device_id") && std::strcmp(key, "entities")) return "Liens HA : URL, appareil et entités uniquement. Aucun jeton.";
    }
    const char* url = settings["url"] | ""; size_t n = std::strlen(url);
    size_t prefix = !std::strncmp(url, "http://", 7) ? 7 : !std::strncmp(url, "https://", 8) ? 8 : 0;
    if (!prefix || n <= prefix || n > 256 || url[prefix] == '/') return "Adresse HA : origine HTTP ou HTTPS requise.";
    for (size_t i = prefix; i < n; ++i) if (static_cast<unsigned char>(url[i]) <= 32 || static_cast<unsigned char>(url[i]) >= 127 || std::strchr("@?#\\\"'", url[i]) || (url[i] == '/' && i != n - 1)) return "Adresse HA : serveur sans identifiants, chemin, paramètres ou fragment.";
    if (!settings["device_id"].isNull()) {
        if (!settings["device_id"].is<const char*>()) return "Identifiant d’appareil HA : texte requis.";
        const char* id = settings["device_id"] | ""; size_t size = std::strlen(id);
        if (size && size != 32) return "Identifiant d’appareil HA : 32 caractères hexadécimaux.";
        for (const char* p = id; *p; ++p) if (!std::isxdigit(static_cast<unsigned char>(*p))) return "Identifiant d’appareil HA invalide.";
    }
    if (!settings["entities"].is<JsonObjectConst>() || settings["entities"].size() > 48) return "Configurez jusqu’à 48 liens d’entités HA.";
    for (JsonPairConst entry : settings["entities"].as<JsonObjectConst>()) {
        const char* key = entry.key().c_str(); if (!*key || std::strlen(key) > 48) return "Clé d’entité HA invalide.";
        for (const char* p = key; *p; ++p) if (!std::isalnum(static_cast<unsigned char>(*p)) && *p != '_') return "Clé d’entité HA invalide.";
        if (!entry.value().is<const char*>() || !entity(entry.value().as<const char*>())) return "Entité HA : domaine.nom en minuscules requis.";
    }
    return nullptr;
}
}
