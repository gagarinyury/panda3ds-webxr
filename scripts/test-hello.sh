#!/bin/sh
# End-to-end check: homebrew guest -> Panda3DS core -> WASM JIT. Expected values come from running the same C natively (see guest/expect.c).
set -e
cd "$(dirname "$0")/.."
./guest/build.sh >/dev/null
cd run
OUT=$(node ../build-core/AlberHeadless.js hello.3dsx 600 2>&1 || true)
echo "$OUT" | grep -E "OutputDebugString|FATAL"
echo "$OUT" | grep -q "hello from the WASM JIT" && echo "$OUT" | grep -q "hash=7df230fe" && echo "$OUT" | grep -q "fpu=4062d686" && echo "PASS: guest output matches native reference" || { echo "FAIL"; exit 1; }
