#!/bin/sh
# Builds a libctru example with the official devkitPro image (via Colima) and copies the .3dsx to ../run/.
# Usage: guest-ctr/build.sh [example path inside 3ds-examples] [output name]
set -e
cd "$(dirname "$0")"
colima status >/dev/null 2>&1 || colima start --cpu 4 --memory 4 --disk 20 --vm-type vz
docker image inspect panda-wasm-ctr >/dev/null 2>&1 || docker build -t panda-wasm-ctr .
[ -d 3ds-examples ] || git clone --depth 1 https://github.com/devkitPro/3ds-examples.git
EX=${1:-graphics/printing/hello-world}
(cd "3ds-examples/$EX" && docker run --rm -v "$PWD":/work -w /work panda-wasm-ctr make)
cp 3ds-examples/$EX/*.3dsx "../run/${2:-ctr-hello}.3dsx"
echo "built ../run/${2:-ctr-hello}.3dsx"
