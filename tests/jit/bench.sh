#!/bin/sh
# Throughput benchmark: ./bench.sh [kernel]
set -e
cd "$(dirname "$0")"
./build-arm.sh >/dev/null
. ../../emsdk/emsdk_env.sh >/dev/null 2>&1
D=../../Panda3DS/third_party/dynarmic
B=../../build-core
emcc bench.c -O2 -w -c -I. -ffp-contract=off -o out/ref_bench.o
em++ jit_bench.cpp out/ref_bench.o -std=c++20 -O1 -w -I. -I$D/src -I$D/externals/mcl/include -I$D/externals/fmt/include -I$D/externals/robin-map/include -I../../Panda3DS/third_party/boost \
  -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION \
  $B/third_party/dynarmic/src/dynarmic/libdynarmic.a $B/third_party/dynarmic/externals/mcl/src/libmcl.a $B/third_party/fmt/libfmt.a \
  -sENVIRONMENT=node -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1048576 -o out/jit_bench.js
node out/jit_bench.js "$@"
