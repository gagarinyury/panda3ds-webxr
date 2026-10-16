# panda3ds-wasm

Work in progress: running a Nintendo 3DS emulator ([Panda3DS](https://github.com/wheremyfoodat/Panda3DS)) inside `WKWebView` on iOS, with guest ARM code recompiled to WebAssembly at runtime so that WebKit's JIT can compile it.
Feasibility study: [wkwebview-jit-bench](https://github.com/gagarinyury/wkwebview-jit-bench).

Licensed under **GPL-3.0-or-later**, like Panda3DS. See [LICENSE](LICENSE).

Not affiliated with Nintendo or Apple. No game data, firmware or keys are included or will be.

## Status

| Check | Result |
|---|---|
| Dynarmic A32 front end + IR build with Emscripten (wasm32) | ✅ builds; needs [two small patches](patches/dynarmic-wasm32.patch) |
| Panda3DS core builds with Emscripten | ⏳ |
| A real ARMv6 block decoded to IR inside WASM | ✅ [`probe/ir_dump.cpp`](probe/ir_dump.cpp), output in [`probe/data/`](probe/data/hash_loop.ir.txt) — a 29-instruction raw IR block optimises to 17 |
| IR → WASM backend | — |
