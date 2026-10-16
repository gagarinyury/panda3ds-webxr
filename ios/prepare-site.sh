#!/bin/sh
# Copies the browser build (page, emulator, homebrew ROMs) into ios/Site, which the app serves from its bundle under panda://app/
set -e
cd "$(dirname "$0")/.."
rm -rf ios/Site && mkdir -p ios/Site/web ios/Site/build-core ios/Site/run/examples
cp web/*.html ios/Site/web/
cp web/*.wasm ios/Site/web/ 2>/dev/null || true
cp build-core/AlberWeb.js build-core/AlberWeb.wasm ios/Site/build-core/
cp run/ctr-hello.3dsx ios/Site/run/ 2>/dev/null || true
cp run/examples/*.3dsx ios/Site/run/examples/ 2>/dev/null || true
mkdir -p ios/Site/run/homebrew && cp run/homebrew/*.3dsx ios/Site/run/homebrew/ 2>/dev/null || true
du -sh ios/Site
