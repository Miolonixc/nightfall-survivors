#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# This script runs INSIDE the pinned Linux ARM64 build image.
# Use muOS SDL2 for its Mali framebuffer driver; C++ runtime is linked in.
make -B BINARY=build/nightfall.aarch64 \
    CXXFLAGS='-O2 -std=c++17 -Wall -Wextra -Wpedantic -Werror -mcpu=cortex-a53 -static-libstdc++ -static-libgcc' \
    SDL_LIBS='-L/usr/local/lib -lSDL2 -pthread -lm -ldl'
mkdir -p build
ldd build/nightfall.aarch64 > build/dependencies.txt
readelf --version-info build/nightfall.aarch64 > build/symbol-versions.txt
./build/nightfall.aarch64 --self-test
./build/nightfall.aarch64 --headless --fast --ui-test --frames 70
python3 tests/integration.py ./build/nightfall.aarch64
python3 tests/updater.py
