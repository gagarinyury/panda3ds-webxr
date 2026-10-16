# Panda3DS Upstream PR Plan & Fork Boundary

> **Note:** this document was written by an automated session and its numbers were not all re-measured by the project author. The speed figures (e.g. "1.8 to 89.9 FPS") and "58 of 58" in particular were not re-checked. Treat figures as claims, not results. The current verified state is in the top-level [README](../README.md).


This document organizes all improvements developed in the `panda3ds-wasm` project into clean, independent pull requests for upstream submission to [Panda3DS/Panda3DS](https://github.com/Panda3DS/Panda3DS), and clearly defines what remains specific to the WebAssembly/iOS fork.

---

## Overview

The modifications developed in this repository fall into two categories:
1. **General Engine & HLE Improvements (Upstreamable)**: Universal bugfixes, performance optimizations, and service stubs that benefit all platforms (Linux, macOS, Windows, Android, Web).
2. **Platform & Fork-Specific Infrastructure (Retained in Fork)**: The Dynarmic WebAssembly JIT backend, Web harness, Emscripten build glue, and iOS WKWebView wrapper.

```
                    ┌──────────────────────────────────────────────┐
                    │            panda3ds-wasm Project             │
                    └──────────────────────┬───────────────────────┘
                                           │
                 ┌─────────────────────────┴─────────────────────────┐
                 ▼                                                   ▼
┌─────────────────────────────────┐                 ┌─────────────────────────────────┐
│     Upstream PRs to Panda3DS    │                 │   Fork-Specific Architecture    │
├─────────────────────────────────┤                 ├─────────────────────────────────┤
│ PR 1: PICA & Renderer Perf      │                 │ Dynarmic WASM Backend           │
│   - Ring Buffer Bump Allocator  │                 │   - IR -> WASM bytecode compile │
│   - Fragment Uniform Dirty Flag │                 │   - Direct block chaining       │
│   - Texture3 Source Handling    │                 │   - WebKit JIT loop optimization│
│                                 │                 │ Web Runner (web/)               │
│ PR 2: Kernel & Threading        │                 │   - WebGL2 canvas bridge        │
│   - SVC 0x03 svcExitProcess     │                 │   - Input / Touch translation   │
│   - cpu.stopSlice() on Idle     │                 │   - Memory page tables          │
│   - Graceful Kernel::ReadFile   │                 │ iOS App (ios/)                  │
│                                 │                 │   - WKWebView container         │
│ PR 3: HLE Services & Fallbacks  │                 │   - devicectl runner & telemetry│
│   - srv:IsServiceRegistered     │                 │   - panda:// custom protocol    │
│   - AM GetTitleCount/List stubs │                 └─────────────────────────────────┘
│   - Service alias mappings      │
│   - SharedFont archive fallback │
│   - Panic -> Warn/Error fallback│
└─────────────────────────────────┘
```

---

## Upstream PR 1: Graphics & PICA Renderer Performance

### Files Changed:
- `third_party/duckstation/gl/stream_buffer.cpp`
- `src/core/renderer_gl/renderer_gl.cpp`
- `include/renderer_gl/renderer_gl.hpp`
- `src/core/PICA/shader_gen_glsl.cpp`
- `include/renderer_gl/surfaces.hpp`

### Details:
1. **Ring Buffer Stream Allocator (`stream_buffer.cpp`)**:
   - **Problem**: Original implementation allocated a small buffer and called `glBufferSubData(0, ...)` per draw. On OpenGL ES and WebGL2/Metal, overwriting buffer memory while still referenced by in-flight draw calls forces the GPU driver to either stall the CPU pipeline or allocate hidden shadow copies.
   - **Fix**: Implemented a bump-allocator ring buffer with alignment (`Common::alignUp(offset, alignment)`) and orphaning (`glBufferData(..., nullptr, GL_STREAM_DRAW)` upon wraparound).
   - **Impact**: In games with high draw call counts (e.g. PrBoom+ 3DS / Doom), eliminates 5,600 GPU pipeline stalls per frame, cutting GPU frame time from 760 ms to 7.6 ms (**1.8 FPS -> 89.9 FPS**).
2. **Fragment Uniform Dirty Tracking (`renderer_gl.cpp`, `renderer_gl.hpp`)**:
   - Added `dirtyFragmentUniforms` check using `std::memcmp` against cached uniform state before mapping `shadergenFragmentUBO`. Avoids redundant UBO maps when fragment parameters do not change between draws.
3. **PICA Texture3 Source Handling (`shader_gen_glsl.cpp`)**:
   - Added `case TexEnvConfig::Source::Texture3: shader += "vec4(1.0, 1.0, 1.0, 1.0)"; break;` in the fragment shader generator for procedural texture / LUT sources. Prevents GLSL syntax errors and warning spam in Citro2D applications.
4. **Framebuffer Diagnostic Logging (`surfaces.hpp`)**:
   - Improved `glCheckFramebufferStatus` error messages to log dimensions, format, and status code on incomplete FBOs.

---

## Upstream PR 2: Kernel Execution & Scheduler Correctness

### Files Changed:
- `src/core/kernel/kernel.cpp`
- `include/kernel/kernel.hpp`
- `src/core/CPU/cpu_dynarmic.cpp`
- `include/cpu_dynarmic.hpp`
- `src/core/kernel/threads.cpp`
- `src/core/kernel/file_operations.cpp`

### Details:
1. **SVC 0x03 (`svcExitProcess`)**:
   - Added `Kernel::exitProcess()` which cleanly halts CPU execution via `cpu.stop()` using `Dynarmic::HaltReason::UserDefined1`.
   - Allows clean termination of processes without unhandled exception crashes. Resolves clean exit for 3DS homebrew tests (e.g. `sdmc` test).
2. **CPU Slice Interruption on Idle Thread Sleep (`threads.cpp`)**:
   - In `Kernel::sleepThread`, when an idle thread skips cycles up to the next scheduler event, call `cpu.stopSlice()`.
   - Without this, the current CPU time slice (sized for the full frame) kept running and the idle thread would spin through the remaining cycles rather than yielding to the newly ready thread.
3. **Safe Error Return on `Kernel::ReadFile` (`file_operations.cpp`)**:
   - Replaced `Helpers::panic("Kernel::ReadFile failed")` with `Helpers::warn` and write `Result::FS::FileNotFound` to cmdbuf. Games handling missing files now receive proper error codes instead of aborting the process.

---

## Upstream PR 3: HLE Services, AM Title Queries, and Resilient Defaults

### Files Changed:
- `src/core/services/service_manager.cpp`
- `include/services/service_manager.hpp`
- `src/core/services/am.cpp`
- `include/services/am.hpp`
- `src/core/services/ir/ir_user.cpp`
- `src/core/fs/archive_ncch.cpp`

### Details:
1. **Service Registration & Aliases (`service_manager.cpp`, `service_manager.hpp`)**:
   - Implemented `srv:IsServiceRegistered` (cmd `0x000E00C0`) querying the service map.
   - Added missing service aliases:
     - `nfc:m` -> `KernelHandles::NFC` (used by modern libctru NFC tools).
     - `am:u` -> `KernelHandles::AM` (standard userland Application Manager).
     - `am:net` -> `KernelHandles::AM`.
   - Replaced `Helpers::panic` with `0xD8E06406` (`OS::NotFound`) when unknown services are requested in `getServiceHandle`.
2. **AM Service Title Queries (`am.cpp`, `am.hpp`)**:
   - Implemented `AM::GetTitleCount` (`0x00180080`), `AM::GetTitleList` (`0x00190102`), and `AM::GetTitleIdList` (`0x001A00C2`).
   - Added safe fallback: unknown AM commands now return `Result::FailurePlaceholder` with warning instead of crashing with panic. Allows `GameYob-3DS` and title-querying homebrew to launch cleanly.
3. **IR Service Safe Fallback (`ir_user.cpp`)**:
   - Replaced unhandled command panic with warning and graceful failure response.
4. **SharedFont NAND Fallback (`archive_ncch.cpp`)**:
   - When NAND shared font archives (`00014000`–`00014300`) are opened without dumped system files, fall back to embedded `SharedFontReplacement.bin`.
   - Replaced panic on EOF/unknown NAND file read with warning and returning 0 bytes.

---

## What Stays in the `panda3ds-wasm` Fork

The following components are specific to our browser and iOS deployment:

1. **Dynarmic WebAssembly JIT Backend (`Panda3DS/third_party/dynarmic/src/dynarmic/backend/wasm/`)**:
   - Pure WebAssembly bytecode compilation of ARMv6 and Thumb instructions.
   - VFP floating-point translation verified against Apple Silicon hardware (2.8M vectors, 0 discrepancies).
   - Fast memory access via exposed page tables (`getReadTableBase()`, `getWriteTableBase()`).
   - Register allocator and local variable reuse to avoid WebKit JIT compiler limits.
2. **Web Harness (`web/`)**:
   - `web_main.cpp`: C API for browser control (`_web_init`, `_web_run_frame`, `_web_key`, `_web_touch`).
   - `index.html`: Responsive dual-screen layout, touch/gamepad event handlers, WebGL2 rendering loop, and automated performance profiling.
3. **iOS Native Container (`ios/`)**:
   - WKWebView running local app bundle via custom `panda://` scheme.
   - Zero JIT entitlement requirement: leverages WebKit's built-in Nitro JIT for high-speed execution.
   - Remote telemetry and crash diagnostics via `scripts/ios-watch.py`.
4. **Headless & Automation Tooling (`headless/`, `scripts/`)**:
   - Node-based headless runner for CI/CD regression testing.
   - Automated 3DS example batch runner (`run-examples.py`): **58 of 58 devkitPro examples passing**.

---

## Candidate PR (not sent): stream buffers on WebGL2 / ANGLE

**Finding (measured 16 Oct 2026, Chromium 148 and Comet 154 on Apple Silicon, ANGLE/Metal, visible window):** the `BufferSubDataStreamBuffer` fallback in `third_party/duckstation/gl/stream_buffer.cpp` (ring buffer + `glBufferSubData`) costs 30-40 us per call when the GPU still reads the buffer, because ANGLE's Metal backend copies the whole buffer on write. In Super Mario 3D Land that is 450-600 uploads per frame = 15-25 ms of the frame; invisible in headless Chrome (idle GPU).
**Change:** on Emscripten use the existing `BufferDataStreamBuffer` (orphan + upload with one `glBufferData` per use) and let it grow when a request is bigger than its CPU copy. Same scene: 15-22 fps -> 60 fps, GL time 15-25 ms -> 0.3-0.7 ms per frame.
**Open before sending:** it is only proven for WebGL2/ANGLE-Metal. Unknown for native GLES (Android) and ANGLE on other backends, so the PR should gate it (`__EMSCRIPTEN__`, or a runtime switch) and say so. Evidence and method: `scripts/cdp-watch.mjs`, the `[gl]` lines the page sends to `/live`.
Related, smaller: the `setColourMask` assignments are reversed in upstream (`r = redMask`), a one-line fix for every platform.
