#!/bin/sh
# Regenerates patches/*.patch from the local fork branches (Panda3DS: branch "wasm", dynarmic: branch "wasm32").
# Base commits = the upstream commits the forks started from; keep them in sync with docs/forks.md.
set -e
cd "$(dirname "$0")/.."
PANDA_BASE=92e53d2       # wheremyfoodat/Panda3DS master: "Add Linux aarch64 build (#839)"
DYNARMIC_BASE=e77b1ba    # azahar-emu/dynarmic: "cmake: Do not find_package if target already defined (#13)"
git -C Panda3DS/third_party/dynarmic diff "$DYNARMIC_BASE" HEAD > patches/dynarmic-wasm32.patch
git -C Panda3DS diff "$PANDA_BASE" HEAD --ignore-submodules=all > patches/panda3ds-emscripten.patch
wc -l patches/*.patch
