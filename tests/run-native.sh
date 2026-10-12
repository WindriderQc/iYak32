#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
IYAK_NATIVE_DIR=$(mktemp -d)
trap 'rm -rf "$IYAK_NATIVE_DIR"' EXIT
IYAK_JSON_INCLUDE=${IYAK_JSON_INCLUDE:-.pio/libdeps/esp32dev/ArduinoJson/src}
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -I src tests/daily_timers.cpp -o "$IYAK_NATIVE_DIR/daily"
"$IYAK_NATIVE_DIR/daily"
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -I src -I "$IYAK_JSON_INCLUDE" tests/atomic_json_store.cpp -o "$IYAK_NATIVE_DIR/storage"
"$IYAK_NATIVE_DIR/storage"
