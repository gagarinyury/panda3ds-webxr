#!/bin/sh
# Builds the harness with Emscripten and runs it under Node. Usage: ./run.sh [test-name]
set -e
cd "$(dirname "$0")"
. ../../emsdk/emsdk_env.sh >/dev/null 2>&1
D=../../Panda3DS/third_party/dynarmic
B=../../build-core
emcc tests.c -O2 -w -c -I. -o out/tests_ref.o
em++ jit_test.cpp out/tests_ref.o -std=c++20 -O1 -w -I. -I$D/src -I$D/externals/mcl/include -I$D/externals/fmt/include -I$D/externals/robin-map/include -I../../Panda3DS/third_party/boost \
  -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION \
  $B/third_party/dynarmic/src/dynarmic/libdynarmic.a $B/third_party/dynarmic/externals/mcl/src/libmcl.a $B/third_party/fmt/libfmt.a \
  -sENVIRONMENT=node -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1048576 -o out/jit_test.js
node out/jit_test.js "$@"
