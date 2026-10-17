#!/bin/sh
# Copies the browser build (page, emulator, homebrew ROMs) into ios/Site, which the app serves from its bundle under panda://app/
set -e
cd "$(dirname "$0")/.."
rm -rf ios/Site.new && mkdir -p ios/Site.new/web ios/Site.new/build-core ios/Site.new/run/examples
cp web/*.html ios/Site.new/web/
cp web/*.wasm ios/Site.new/web/ 2>/dev/null || true
cp web/*.json ios/Site.new/web/ 2>/dev/null || true
cp web/*.js ios/Site.new/web/ 2>/dev/null || true
cp web/*.txt ios/Site.new/web/ 2>/dev/null || true
cp build-core/AlberWeb.js build-core/AlberWeb.wasm ios/Site.new/build-core/
cp run/ctr-hello.3dsx ios/Site.new/run/ 2>/dev/null || true
cp run/examples/*.3dsx ios/Site.new/run/examples/ 2>/dev/null || true
mkdir -p ios/Site.new/run/homebrew && cp run/homebrew/*.3dsx ios/Site.new/run/homebrew/ 2>/dev/null || true
# game data for PrBoom+ (Freedoom WAD etc.), if fetched
for f in _dl/prboom/x/3ds/PrBoom-Plus/prboom-plus.wad _dl/prboom/x/etc/timidity.cfg _dl/freedoom/x/freedoom1.wad; do [ -f run/homebrew/$f ] && mkdir -p ios/Site.new/run/homebrew/$(dirname $f) && cp run/homebrew/$f ios/Site.new/run/homebrew/$f; done; true
mkdir -p ios/Site && rsync -a --delete ios/Site.new/ ios/Site/ && rm -rf ios/Site.new   # updated in place: a server started in ios/Site keeps serving without a gap
du -sh ios/Site
