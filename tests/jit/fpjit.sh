#!/bin/sh
# VFP differential test through the JIT: ./fpjit.sh [random_pairs]
set -e
cd "$(dirname "$0")"
EM=../../emsdk/upstream/bin
mkdir -p out
FLAGS="-O2 -ffreestanding -fno-builtin -fno-jump-tables -fno-vectorize -fno-slp-vectorize -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-math-errno -ffp-contract=off"
/usr/bin/clang --target=armv6k-none-eabihf -mcpu=arm1176jzf-s -mfpu=vfpv2 -mfloat-abi=hard -marm $FLAGS -c fp_ops.c -o out/fp_ops.o
$EM/llvm-objcopy -O binary -j .text out/fp_ops.o out/fp_ops.bin
$EM/llvm-nm out/fp_ops.o | awk '$2 ~ /^[tT]$/ {print $3, $1}' > out/fp_ops.syms
python3 -I patch-relocs.py out/fp_ops.o out/fp_ops.bin out/fp_ops.syms arm >/dev/null
. ../../emsdk/emsdk_env.sh >/dev/null 2>&1
D=../../Panda3DS/third_party/dynarmic
B=../../build-core
em++ fp_jit_test.cpp -std=c++20 -O1 -w -I. -I$D/src -I$D/externals/mcl/include -I$D/externals/fmt/include -I$D/externals/robin-map/include -I../../Panda3DS/third_party/boost \
  -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION \
  $B/third_party/dynarmic/src/dynarmic/libdynarmic.a $B/third_party/dynarmic/externals/mcl/src/libmcl.a $B/third_party/fmt/libfmt.a \
  -sENVIRONMENT=node -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1048576 -o out/fp_jit_test.js
node out/fp_jit_test.js "$@"
