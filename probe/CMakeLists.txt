cmake_minimum_required(VERSION 3.16)
project(dynarmic_front_probe CXX C)
set(CMAKE_CXX_STANDARD 20)
set(D ${CMAKE_CURRENT_LIST_DIR}/../Panda3DS/third_party/dynarmic)
set(S ${D}/src/dynarmic)
file(GLOB_RECURSE SRCS
  ${S}/common/*.cpp ${S}/ir/*.cpp ${S}/ir/opt/*.cpp
  ${S}/frontend/imm.cpp ${S}/frontend/A64/a64_types.cpp ${D}/externals/mcl/src/assert.cpp ${S}/frontend/A32/*.cpp ${S}/frontend/A32/translate/*.cpp ${S}/frontend/A32/translate/impl/*.cpp
)
list(FILTER SRCS EXCLUDE REGEX "llvm_disassemble|x64_disassemble|spin_lock_arm64|spin_lock_x64|/disassembler/|ir/opt/a64_")
add_library(dynfront STATIC ${SRCS})
target_include_directories(dynfront PUBLIC ${D}/src ${D}/externals/mcl/include ${D}/externals/fmt/include ${D}/externals/robin-map/include ${CMAKE_CURRENT_LIST_DIR}/../Panda3DS/third_party/boost)
target_compile_definitions(dynfront PUBLIC FMT_HEADER_ONLY=1 _LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)
target_compile_options(dynfront PRIVATE -O1 -w "SHELL:-include stdlib.h" "SHELL:-include string.h")
