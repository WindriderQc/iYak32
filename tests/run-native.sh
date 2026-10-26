#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
IYAK_NATIVE_DIR=$(mktemp -d)
trap 'rm -rf "$IYAK_NATIVE_DIR"' EXIT
IYAK_JSON_INCLUDE=${IYAK_JSON_INCLUDE:-.pio/libdeps/esp32dev/ArduinoJson/src}
for IYAK_TEST in tests/*.cpp; do
    IYAK_NAME=$(basename "$IYAK_TEST" .cpp)
    "${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -no-pie -I src -I tests/fixtures -I "$IYAK_JSON_INCLUDE" "$IYAK_TEST" -o "$IYAK_NATIVE_DIR/$IYAK_NAME"
    "$IYAK_NATIVE_DIR/$IYAK_NAME"
done
