#!/usr/bin/env python3
"""Read-only smoke check for a BasicMode iYak32 on the private LAN."""

import argparse
import json
import time
from urllib.error import HTTPError
from urllib.request import urlopen


PRIVATE_FILES = ("/esp32config.json", "/config.txt", "/mqtt.txt")
SECRET_FIELDS = ("pass", "wifi_password", "mqttuser", "mqtt_user", "mqttpass", "mqtt_pass", "tides_api_key")


def fetch(base, path):
    try:
        with urlopen(base + path, timeout=5) as response:
            return response.status, response.read()
    except HTTPError as error:
        return error.code, error.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="device IP or hostname on the private LAN")
    parser.add_argument("--wait", type=float, default=5, help="seconds between uptime samples")
    args = parser.parse_args()
    base = "http://" + args.host
    failures = []

    def get_json(path):
        status, body = fetch(base, path)
        if status != 200:
            failures.append(f"{path}: HTTP {status}")
            return None
        try:
            return json.loads(body)
        except ValueError:
            failures.append(f"{path}: invalid JSON")
            return None

    first = get_json("/api/system/uptime")
    basic = get_json("/api/basicmode/status")
    wifi = get_json("/api/wifi/status")
    mqtt = get_json("/api/mqtt/status")
    config = get_json("/data")

    if isinstance(basic, dict) and not all(key in basic for key in ("led1", "led2", "led3")):
        failures.append("BasicMode LED status is incomplete")
    if isinstance(wifi, dict) and not wifi.get("connected"):
        failures.append("Wi-Fi is disconnected")
    if isinstance(mqtt, dict) and mqtt.get("enabled") and not mqtt.get("connected"):
        failures.append("MQTT is enabled but disconnected")
    if isinstance(config, dict) and any(config.get(key) for key in SECRET_FIELDS):
        failures.append("/data exposes a configured secret")

    for path in PRIVATE_FILES:
        status, _ = fetch(base, path)
        if status != 404:
            failures.append(f"{path}: expected HTTP 404, got {status}")

    time.sleep(max(args.wait, 0))
    second = get_json("/api/system/uptime")
    if isinstance(first, dict) and isinstance(second, dict):
        before, after = first.get("uptime"), second.get("uptime")
        if not isinstance(before, int) or not isinstance(after, int) or after <= before:
            failures.append(f"uptime did not increase: {before} -> {after}")
        print(f"uptime: {before} -> {after} seconds")

    print("BasicMode:", "present" if isinstance(basic, dict) else "missing")
    print("Wi-Fi:", "connected" if isinstance(wifi, dict) and wifi.get("connected") else "disconnected")
    print("MQTT:", "connected" if isinstance(mqtt, dict) and mqtt.get("connected") else "disconnected or disabled")
    print("private config routes:", "blocked" if not any("expected HTTP 404" in item for item in failures) else "check failed")
    if failures:
        for failure in failures:
            print("FAIL:", failure)
        return 1
    print("PASS: read-only device smoke check")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
