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
| IR → WASM backend (`Dynarmic::A32::Jit` for wasm) | ✅ v0: executes real ARM and Thumb code; integer, memory, flags, conditions, branches, calls, CP15 TLS. VFP scalar float/double (add, sub, mul, div, sqrt, compare, conversions) via bit-exact soft-float. Scalar saturation (SSAT, USAT, QADD, QSUB) and ARMv6 packed SIMD (uadd8, usub8, sadd8, ssub8, 16-bit packed, sel, usada8). Not yet: NEON/vector, 64-bit shifts, fast inline FP |
| VFP fast path through the JIT vs. the soft-float reference ([`tests/jit/fpjit.sh`](tests/jit/fpjit.sh)): 10 FPSCR modes × 123 k operand pairs × add/sub/mul/div/sqrt/vcmp | ✅ 7.4 M bit-exact comparisons (and a negative control that fails when the rounding fix is removed) |
| Differential tests ([`tests/jit`](tests/jit)): 14 C kernels × ARM/Thumb × 32 inputs vs. the same C compiled to wasm | ✅ 34/34 pass (14 integer kernels × ARM/Thumb, 6 VFP kernels; return value and scratch memory compared) |
| Soft-float VFP vs. Apple Silicon FPU ([`tests/fp`](tests/fp)): add/sub/mul/div/sqrt/cmp/convert, f32+f64, all 16 FPCR modes (rounding × FZ × DN), result **and** exception flags | ✅ 2 785 536 comparisons, 0 mismatches (after fixing one Dynarmic bug in float → unsigned conversion) |
| A homebrew `.3dsx` runs through the real Panda3DS core with the WASM JIT (headless, Node) | ✅ [`guest/hello.c`](guest/hello.c): prints via `svcOutputDebugString`, 2 M-iteration hash loop and a VFP computation; both results equal the native reference bit for bit (`scripts/test-hello.sh`) |
| A real **libctru** homebrew (devkitPro `3ds-examples/graphics/printing/hello-world`, built with the official devkitARM image) runs to its main loop and draws its text | ✅ [`assets/ctr-hello-world-top.png`](assets/ctr-hello-world-top.png) is the emulated top screen (`scripts/test-ctr-hello.sh`) |
| Same in a browser / `WKWebView` with graphics | — |

<p align="center"><img src="assets/ctr-hello-world-top.png" width="400" alt="Emulated 3DS top screen: Hello World!"></p>

The only place Panda3DS touches Dynarmic is `Dynarmic::A32::Jit` (see `src/core/CPU/cpu_dynarmic.cpp`), so the new backend plugs in behind the same interface and Panda3DS itself stays almost untouched.

## Speed (Node 25 / V8, guest MIPS, higher is better)

Measured with [`tests/jit/bench.sh`](tests/jit/bench.sh). Real 3DS needs ~270 MIPS per core (Old) / ~800 MIPS (New).

| kernel | interpreter-style dispatch + callbacks | + block chaining | + inline memory | + inline VFP |
|---|---:|---:|---:|---:|
| `b_hash` tight integer loop | 157 | 1 411 | 1 486 | **1 446** |
| `b_memsum` loads/stores | 213 | 684 | 1 646 | **1 625** |
| `b_calls` recursion, call/return | 97 | 309 | 911 | **917** |
| `b_fdot` VFP dot products | 120 | 200 | 248 | **1 659** |

Block chaining = blocks tail-call each other through a shared function table (`return_call_indirect`) via a direct-mapped cache.
Inline memory = the same page tables Panda3DS uses, indexed from generated code. Inline VFP = add/sub/mul/div/sqrt/compare in
round-to-nearest and round-to-zero modes (the 3DS default is round-to-zero + flush-to-zero + default-NaN) with NaN cases delegated
to the bit-exact helper. The inline path does not accumulate FPSR exception flags.

## Build

```sh
git clone https://github.com/emscripten-core/emsdk.git && ./emsdk/emsdk install latest && ./emsdk/emsdk activate latest
git clone --recurse-submodules https://github.com/wheremyfoodat/Panda3DS.git
git -C Panda3DS apply ../patches/panda3ds-emscripten.patch
git -C Panda3DS/third_party/dynarmic apply ../../../patches/dynarmic-wasm32.patch
source emsdk/emsdk_env.sh && scripts/build-core.sh && cmake --build build-core
```

The patches were made against Panda3DS `92e53d2` and Dynarmic `e77b1ba`.
