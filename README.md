# panda3ds-wasm

Work in progress: running a Nintendo 3DS emulator ([Panda3DS](https://github.com/wheremyfoodat/Panda3DS)) inside `WKWebView` on iOS, with guest ARM code recompiled to WebAssembly at runtime so that WebKit's JIT can compile it.
Feasibility study: [wkwebview-jit-bench](https://github.com/gagarinyury/wkwebview-jit-bench).

Licensed under **GPL-3.0-or-later**, like Panda3DS. See [LICENSE](LICENSE).

Not affiliated with Nintendo or Apple. No game data, firmware or keys are included or will be.

## Status

| Check | Result |
|---|---|
| Dynarmic A32 front end + IR build with Emscripten (wasm32) | ✅ two small fixes (`size_t` assumed 64-bit, function pointers cast to `u64`) |
| Real ARMv6 code decoded to IR inside WASM | ✅ [`probe/ir_dump.cpp`](probe/ir_dump.cpp), output in [`probe/data/hash_loop.ir.txt`](probe/data/hash_loop.ir.txt): a 29-instruction IR block optimises to 17 |
| Whole Panda3DS core + SDL front end links to `Alber.js` / `Alber.wasm` (5.2 MB) | ✅ ~50 lines of CMake/source changes in Panda3DS, ~140 lines in Dynarmic (stub wasm backend included) |
| IR → WASM backend (`Dynarmic::A32::Jit` for wasm) | ✅ v0: executes real ARM and Thumb code; integer, memory, flags, conditions, branches, calls, CP15 TLS. Not yet: VFP/FP, packed/saturating ops, 64-bit shifts |
| Differential tests ([`tests/jit`](tests/jit)): 12 C kernels × ARM/Thumb × 32 inputs vs. the same C compiled to wasm | ✅ 24/24 pass (return value and scratch memory compared) |
| Boots a homebrew `.3dsx` in a browser | — |

The only place Panda3DS touches Dynarmic is `Dynarmic::A32::Jit` (see `src/core/CPU/cpu_dynarmic.cpp`), so the new backend plugs in behind the same interface and Panda3DS itself stays almost untouched.

## Build

```sh
git clone https://github.com/emscripten-core/emsdk.git && ./emsdk/emsdk install latest && ./emsdk/emsdk activate latest
git clone --recurse-submodules https://github.com/wheremyfoodat/Panda3DS.git
git -C Panda3DS apply ../patches/panda3ds-emscripten.patch
git -C Panda3DS/third_party/dynarmic apply ../../../patches/dynarmic-wasm32.patch
source emsdk/emsdk_env.sh && scripts/build-core.sh && cmake --build build-core
```

The patches were made against Panda3DS `92e53d2` and Dynarmic `e77b1ba`.
