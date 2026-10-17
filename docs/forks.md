# Which repository is for what

| Repository | Branch | What lives there | Edit here? |
|---|---|---|---|
| `gagarinyury/panda3ds-wasm` (this one) | `main` | web page (`web/`), iOS app (`ios/`), scripts, tests, docs, generated `patches/` | yes |
| `gagarinyury/Panda3DS` (fork of `wheremyfoodat/Panda3DS`) | `wasm` | all changes to the emulator core: renderer, kernel, services, Emscripten build | yes: this is the source of truth |
| `gagarinyury/dynarmic` (fork of `azahar-emu/dynarmic`) | `wasm32` | the WebAssembly JIT backend (`src/dynarmic/backend/wasm/`) and small portability fixes | yes: this is the source of truth |

Rules:

1. Core and JIT changes are committed in the **fork branches**. `patches/*.patch` are generated, never edited: run `scripts/regen-patches.sh`
   (it diffs the branches against the upstream commits they started from: Panda3DS `92e53d2`, dynarmic `e77b1ba`).
2. `Panda3DS/` is git-ignored here: it is a separate checkout of the fork (`git clone --recurse-submodules -b wasm https://github.com/gagarinyury/Panda3DS.git`).
   The Panda3DS branch pins the dynarmic fork as its submodule, so after changing dynarmic: commit and push there first, then bump the pointer in Panda3DS.
3. Anything meant for upstream goes into a **separate branch cut from upstream `master`** (one topic per branch), never from `wasm`. Nothing has been sent upstream yet.
4. Game images, firmware and screenshots of commercial games are never committed (`run/*/` is ignored).

Current heads (17 Oct 2026): Panda3DS `wasm` `03bee38` (pins dynarmic `9b3178d`), dynarmic `wasm32` `9b3178d` (table-call helpers + block packing; `JIT_MAX_BLOCKS` opt-in cap). See [webkit-instances-and-batching.md](webkit-instances-and-batching.md).
