#!/bin/sh
# libctru hello-world (devkitPro 3ds-examples) on Panda3DS + WASM JIT: checks the rendered top screen. Needs run/ctr-hello.3dsx (guest-ctr/build.sh).
set -e
cd "$(dirname "$0")/../run"
OUT=$(node ../build-core/AlberHeadless.js ctr-hello.3dsx 40 shot 2>&1)
echo "$OUT" | grep -E "headless\] (done|top)"
echo "$OUT" | grep -q "top screen: .*(520 non-black pixels)" && echo "PASS: top screen shows 'Hello World!' (520 lit pixels, matches the reference run)" || { echo "FAIL"; exit 1; }
python3 -I ../scripts/ppm2png.py shot-top.ppm shot-top.png 2
