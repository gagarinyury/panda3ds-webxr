#!/bin/sh
# Builds guest/hello.c into ../run/hello.3dsx (no devkitPro needed: the 3DSX container is written by pack3dsx.py).
set -e
cd "$(dirname "$0")"
EM=../emsdk/upstream/bin
mkdir -p out ../run
/usr/bin/clang --target=armv6k-none-eabihf -mcpu=arm1176jzf-s -mfpu=vfpv2 -mfloat-abi=hard -marm -O2 -ffreestanding -fno-builtin -nostdlib \
  -fno-jump-tables -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-math-errno -ffp-contract=off -c hello.c -o out/hello.o
# the bare 3DSX has only a code segment: refuse objects that need .rodata/.data/.bss
if $EM/llvm-readobj -S out/hello.o | grep -E 'Name: \.(rodata|data|bss)' >/dev/null; then echo "error: hello.o has data sections; keep everything in .text"; exit 1; fi
$EM/llvm-objcopy -O binary -j .text out/hello.o out/hello.bin
$EM/llvm-nm out/hello.o | awk '$2 ~ /^[tT]$/ {print $3, $1}' > out/hello.syms
python3 -I ../tests/jit/patch-relocs.py out/hello.o out/hello.bin out/hello.syms arm
python3 -I pack3dsx.py out/hello.bin ../run/hello.3dsx
