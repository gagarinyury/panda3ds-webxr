#!/bin/sh
# Builds tests.c for ARMv6 (Apple clang has the ARM target) and extracts raw code + symbol offsets.
set -e
cd "$(dirname "$0")"
EM=../../emsdk/upstream/bin
mkdir -p out
FLAGS="-O2 -ffreestanding -fno-builtin -fno-jump-tables -fno-vectorize -fno-slp-vectorize -mno-unaligned-access -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables"
for mode in arm thumb; do
  /usr/bin/clang --target=armv6k-none-eabi -mcpu=arm1176jzf-s -m$mode $FLAGS -c tests.c -o out/tests_$mode.o
  $EM/llvm-objcopy -O binary -j .text out/tests_$mode.o out/tests_$mode.bin
  $EM/llvm-nm out/tests_$mode.o | awk '$2 ~ /^[tT]$/ {print $3, $1}' > out/tests_$mode.syms
  echo "$mode: $(wc -c < out/tests_$mode.bin) bytes"
  python3 -I patch-relocs.py out/tests_$mode.o out/tests_$mode.bin out/tests_$mode.syms $mode
done
