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
# VFP kernels: ARM mode only (Thumb-1 cannot address the FPU)
/usr/bin/clang --target=armv6k-none-eabihf -mcpu=arm1176jzf-s -mfpu=vfpv2 -mfloat-abi=hard -marm $FLAGS -fno-math-errno -ffp-contract=off -c tests_fp.c -o out/tests_fp.o
$EM/llvm-objcopy -O binary -j .text out/tests_fp.o out/tests_fp.bin
$EM/llvm-nm out/tests_fp.o | awk '$2 ~ /^[tT]$/ {print $3, $1}' > out/tests_fp.syms
echo "fp: $(wc -c < out/tests_fp.bin) bytes"
python3 -I patch-relocs.py out/tests_fp.o out/tests_fp.bin out/tests_fp.syms arm
# throughput kernels
/usr/bin/clang --target=armv6k-none-eabihf -mcpu=arm1176jzf-s -mfpu=vfpv2 -mfloat-abi=hard -marm $FLAGS -fno-math-errno -ffp-contract=off -c bench.c -o out/bench.o
$EM/llvm-objcopy -O binary -j .text out/bench.o out/bench.bin
$EM/llvm-nm out/bench.o | awk '$2 ~ /^[tT]$/ {print $3, $1}' > out/bench.syms
python3 -I patch-relocs.py out/bench.o out/bench.bin out/bench.syms arm
