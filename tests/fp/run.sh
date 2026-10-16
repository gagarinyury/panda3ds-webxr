#!/bin/sh
# Native (Apple Silicon) oracle for the soft-float VFP code. Usage: ./run.sh [random_pairs_per_mode]
set -e
cd "$(dirname "$0")"
D=../../Panda3DS/third_party/dynarmic
mkdir -p out
SRC="$(ls $D/src/dynarmic/common/fp/*.cpp $D/src/dynarmic/common/fp/op/*.cpp) $D/src/dynarmic/common/u128.cpp $D/src/dynarmic/common/math_util.cpp $D/externals/mcl/src/assert.cpp $D/src/dynarmic/backend/wasm/fp_soft.cpp"
clang++ -std=c++20 -O1 -w -I$D/src -I$D/externals/mcl/include -I$D/externals/fmt/include -DFMT_HEADER_ONLY=1 fp_oracle.cpp $SRC -o out/fp_oracle
./out/fp_oracle "$@"
