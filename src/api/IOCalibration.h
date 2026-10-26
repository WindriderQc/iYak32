#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace IOCalibration {
inline double convert(double raw, JsonVariantConst calibration) {
    const double low = calibration["raw_min"].as<double>();
    const double high = calibration["raw_max"].as<double>();
    double ratio = (raw - low) / (high - low);
    if (calibration["clamp"] | true) ratio = std::max(0.0, std::min(1.0, ratio));
    const double start = calibration["value_min"].as<double>();
    return start + ratio * (calibration["value_max"].as<double>() - start);
}
inline const char* validate(JsonVariantConst calibration) {
    if (calibration.isNull()) return nullptr;
    if (!calibration.is<JsonObjectConst>()) return "La calibration doit être un objet.";
    for (const char* key : {"raw_min", "raw_max"}) {
        if (!calibration[key].is<int>() || calibration[key].as<int>() < 0 || calibration[key].as<int>() > 4095)
            return "Les références ADC doivent être des entiers de 0 à 4095.";
    }
    if (calibration["raw_min"].as<int>() == calibration["raw_max"].as<int>()) return "Les deux références ADC doivent être différentes.";
    for (const char* key : {"value_min", "value_max"}) {
        if (!calibration[key].is<double>() || !std::isfinite(calibration[key].as<double>()) || std::abs(calibration[key].as<double>()) > 1000000000)
            return "Les valeurs de calibration doivent être des nombres finis entre -1e9 et 1e9.";
    }
    if (!calibration["unit"].is<const char*>() || std::strlen(calibration["unit"].as<const char*>()) > 16)
        return "L'unité doit contenir au maximum 16 caractères.";
    if (!calibration["clamp"].isNull() && !calibration["clamp"].is<bool>()) return "La limitation de plage doit être un booléen.";
    return nullptr;
}
}
