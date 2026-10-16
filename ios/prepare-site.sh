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
# game data for PrBoom+ (Freedoom WAD etc.), if fetched
for f in _dl/prboom/x/3ds/PrBoom-Plus/prboom-plus.wad _dl/prboom/x/etc/timidity.cfg _dl/freedoom/x/freedoom1.wad; do [ -f run/homebrew/$f ] && mkdir -p ios/Site/run/homebrew/$(dirname $f) && cp run/homebrew/$f ios/Site/run/homebrew/$f; done; true
du -sh ios/Site
