#!/bin/sh
# Builds every example in 3ds-examples inside the devkitPro container; results in ../run/examples/<flattened-path>.3dsx and build-summary.txt
set -e
cd "$(dirname "$0")"
colima status >/dev/null 2>&1 || colima start --cpu 4 --memory 6 --disk 20 --vm-type vz
docker image inspect panda-wasm-ctr >/dev/null 2>&1 || docker build -t panda-wasm-ctr .
[ -d 3ds-examples ] || git clone --depth 1 https://github.com/devkitPro/3ds-examples.git
mkdir -p ../run/examples
docker run --rm -v "$PWD/3ds-examples":/ex panda-wasm-ctr sh -c '
  cd /ex
  for mk in $(find . -name Makefile | sort); do
    d=$(dirname "$mk")
    if (cd "$d" && make -j4 >/tmp/build.log 2>&1); then echo "OK   $d"; else echo "FAIL $d: $(grep -E "error|Error|No rule|fatal" /tmp/build.log | head -1)"; fi
  done' > build-summary.txt
for f in $(find 3ds-examples -name '*.3dsx'); do
  n=$(echo "$f" | sed 's#^3ds-examples/##; s#/[^/]*\.3dsx$##; s#/#__#g')
  cp "$f" "../run/examples/$n.3dsx"
done
grep -c '^OK' build-summary.txt | sed 's/^/built OK: /'; grep -c '^FAIL' build-summary.txt | sed 's/^/build failures: /'
