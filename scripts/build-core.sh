#!/bin/sh
# Configure + build the Panda3DS core for WebAssembly (needs emsdk activated: source emsdk/emsdk_env.sh)
set -e
emcmake cmake -S Panda3DS -B build-core -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_VULKAN=OFF -DENABLE_METAL=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_LUAJIT=OFF \
  -DENABLE_RENDERDOC_API=OFF -DENABLE_FASTMEM=OFF -DENABLE_GIT_VERSIONING=OFF -DENABLE_WAYLAND=OFF \
  -DENABLE_OPENGL=ON -DOPENGL_PROFILE=OpenGLES -DDYNARMIC_USE_BUNDLED_EXTERNALS=ON
cmake --build build-core -j8
