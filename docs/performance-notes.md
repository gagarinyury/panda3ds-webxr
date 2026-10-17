# Performance notes (iPhone Air, iOS 27, Mario 3D Land demo scene)

Everything below was measured, not assumed. Phone cold (`thermal 0`), 13 samples per row, a 1-pixel `readPixels` after every frame (`GL_SYNC:1,GL_FINISH:1`)
measures how long the GPU process needs per frame ("GPU wait"). Scene: the "Press A" demo (top screen runs the 3D level).

| Variant (env) | GPU wait | fps (with the sync) | Verdict |
|---|---|---|---|
| normal | 9.9 ms | 36.5 | baseline |
| no draw calls at all, every upload kept (`GL_NODRAW:1`) | 1.0 ms | 59.9 | NOT a clean test of the uploads: with no draw the orphaned buffers are never used, so the driver never really allocates them (this note first said "uploads cost nothing": wrong, see the micro benchmark below) |
| draws clipped to 1 pixel (`GL_TINYSCISSOR:1`) | 9.1 ms | 38.6 | pixel work is not the cost |
| `precision mediump float` (`GL_MEDIUMP:1`) | 10.0 ms | 34.3 | no gain (phone warmed to thermal 1 in that run) |
| skip redundant `bindFramebuffer`/`framebufferTexture2D` (`GL_FBCACHE:1`, 95-99.99 % skipped) | 9.6 ms | 38.4 | the driver already ignored them |
| 8-bit indices widened to 16 bits (`GL_INDEX16:1`) | 9.1 ms | 38.7 | within noise |
| sound off (`AUDIO:0`) | 9.6 ms | 36.4 | sound is not the cause |
| index / vertex uploads dropped (`GL_DROP:idx|vtx`) | 6.4 / 1.0 ms | 49.4 / 58.2 | NOT a clean test: the picture breaks, so there is less to draw |

(First reading, partly wrong, kept for the record) What this left: ~9 ms per frame in the **draw calls themselves** (176 per frame, ~50 µs each) inside the browser's GPU process / ANGLE-Metal, not in pixel work,
buffer allocation, framebuffer rebinding or sound. On the Mac the same scene needs ~1 ms. Reducing it would take fewer draws (batching compatible draws) or fewer frames
rendered (skip every second frame when the phone is warm); both are untried.

Other measured facts:
- The phone reaches `thermal 1` after ~110 s of this scene (on a cable, full brightness) and then runs at ~42 fps instead of ~56; `thermal 2` gives 30-37 fps.
- Waiting for the GPU every frame (`GL_SYNC`) makes it worse in heavy scenes (it serialises GPU and CPU time): 35-47 fps instead of ~56. Off by default.
- `gl.finish()` returns at once on WebKit and measures nothing.
- CPU profile (Mac): 55 % guest code in the WASM JIT, Panda3DS C++ < 6 %, WebGL calls ~1 ms per frame after the buffer fix.
- Slow frames > 30 ms: mostly JIT compilation (300-600 blocks in a frame), shader compilation 30-600 ms, rare.
- Looking ahead for the next block gave nothing (tested, removed): the spikes are entries into big new code regions.

## Draw-call micro benchmark (env `BENCH_DRAWS:1`, iPhone Air, 176 draws a frame, 120 frames, time until the GPU has finished)

Same scene in every mode: each draw has its own 300 vertices, 450 indices and a 256-byte uniform block.

| Mode | ms/frame | us/draw | (352 draws: us/draw) |
|---|---|---|---|
| WebGL2, new buffer for every upload (`glBufferData`, what Panda does) | 3.00 | 17.0 | 22.0 |
| WebGL2, pool of buffers filled with `glBufferSubData` | 1.13 | 6.4 | 8.0 |
| WebGL2, one upload per frame, draws use offsets | 0.60 | 3.4 | 3.0 |
| WebGPU, one `writeBuffer` per frame, dynamic uniform offsets | 0.40 | 2.3 | 2.2 |

Mac (same test, headless Chrome): 8.0 / 4.3 / 2.4 / 1.5 us/draw at 176 draws.

Reading: the cost of the draw calls comes mostly from creating a buffer for every upload (6x against one upload per frame); WebGPU gains only ~30 % over the best WebGL2 mode, so a WebGPU renderer is not worth
it for this. The buffer pool (`BufferPoolStreamBuffer` in `third_party/duckstation/gl/stream_buffer.cpp`, opt-in with env `GL_STREAM_POOL:1`, size `GL_POOL_SIZE`) is 2.7x cheaper per draw in the benchmark.
**In the game on the iPhone it did not help** (17 Oct, Mario 3D Land, frames 1100-2700, `GL_SYNC:1,GL_FINISH:1`): orphaning 9.9 ms GPU wait / 18.1 ms emu per frame; pool of 640: 9.7 / 18.0 (but iOS killed the WebContent process in 2 of 2 runs around frame 1700-1800, `thermal` 0); pool of 96: 11.2 / 20.2, not killed (1 run). So the benchmark win does not carry over, and the per-upload buffer creation is not the main cost of a frame. Default is orphaning again.

## Where the time of a frame goes (17 Oct, Mac, Chrome 148, Mario 3D Land, frames 1200+)

- `probe-run.html`: `web_frame` 3.4 ms mean; JS-side WebGL calls 0.4 ms (~12 %); GPU 0.4 ms; 61 draws per frame on average. On the iPhone the WebGL share is higher (17 us x 176 draws ~ 3 ms of 15-18 ms, estimate from the micro benchmark), but still not the main part.
- `scripts/cdp-cpuprof.mjs` (V8 sampling, 100 us): 84 % idle; of the busy 16 %, ~66 % is unnamed `wasm-function` (guest JIT blocks), ~17 % browser (`program`), ~16 % glue (`Jit::Run`, js-to-wasm, WebGL). Panda C++ functions are invisible.
- `scripts/cdp-dispatch-stats.mjs` (counters `Module.jit.s` in dynarmic): ~1570 returns to the dispatcher (`jit_js_run`) per frame, ~182 k guest ticks per frame, ~116 ticks per return. Block chaining works; the dispatcher costs an estimated ~8 % of the busy time, so better chaining can gain at most that.
- Time inside `Jit::Run()` (counters `Module.jit.s[3]`, `[4]`): 2.4-3.3 ms per frame, of which 0.2-0.3 ms are host callbacks (SVC etc.); guest code incl. inline memory access, MMIO helpers and dispatcher returns ~2.2-3.0 ms for ~183 k ticks = **12-16 ns per tick**. Same with V8 `--no-liftoff` (14-16 ns), so the compile tier is not the reason (V8 only; JavaScriptCore not tested).
- A tick is an ARM11 cycle from Panda's `getCyclesForInstruction`, not an instruction, so ns per instruction is lower than that and unknown. Emitter facts: memory access is inlined through the page table; guest registers live in the state struct in linear memory (each GetRegister/SetRegister is a real load/store). Open: how big the gap to a native Dynarmic JIT really is. Cheapest answer: run the same scene in a native Panda3DS build on the same Mac and compare guest ms per frame.
- Consequence: moving only the WebGL calls into a worker would gain ~12-20 % of a frame, not a factor of two.
- A pool-buffer run on the iPhone (frames to 1805) ended with `webcontent_terminated` (iOS killed the WebContent process); the cause is not known. The pool's buffer memory is stable on the Mac (1921 buffers, 23.5 MB, `scripts/cdp-bufmem.mjs`). Not seen in ~10 earlier runs with the old buffers.

### Native comparison (17 Oct, same Mac M5, same ROM, frames 1200-2400)

Native Panda3DS (`cmake -S Panda3DS -B build-native -DCMAKE_BUILD_TYPE=Release -DENABLE_VULKAN=OFF -DENABLE_METAL=OFF -DENABLE_LUAJIT=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_RENDERDOC_API=OFF -DENABLE_USER_BUILD=ON`), env `CPU_STATS=1` prints per 120 frames the time inside `jit->Run()` and inside the SVC handlers.
- The native `Run()` is 14-20 ms per frame, but 13-19 ms of that is inside SVC handlers (the frame limiter waits there) - not comparable with the web build.
- Native guest code outside SVC handlers (including MMIO callbacks, like the web number): **~0.7 ms per frame**. Web build: **2.2-3.0 ms per frame**. So the WASM JIT is ~3-4x slower than the native ARM64 Dynarmic on the same CPU - not 10x. (The scheduler ticks per frame are the timeline, 4.47 M = 268 MHz / 60, not executed cycles, so ticks cannot be compared between the builds.)
- Consequence: better generated code can bring maybe 1.3-2x on the guest part (registers in locals instead of the state struct is the obvious candidate); that is a real but bounded gain.

### Claims from an external code review, checked (17 Oct)

- `FragmentUniforms` was not zeroed before the `memcmp` dedupe (real UB: unset members and padding hold stack garbage). Zeroed now (`GL_UNIFORMS_UNINIT:1` restores the old behaviour). **No measurable effect**: UBO uploads per draw 1.37 before and after (`scripts/cdp-gl-counts.mjs`, Mac, frames 1200+).
- `preserveDrawingBuffer` (default on) vs `GL_NOPRESERVE:1` on the iPhone, Mario 3D Land, frames 1100-2700, 29 samples each, no sync probes: speed 53.4 vs 53.5, 3D loop 28.5 vs 28.7 Hz, emu 7.8 vs 7.9 ms, thermal 0. **No effect.**
- The `link[]` table is direct-mapped (`(pc >> 1) & 0x3FFF`) and filled only by the host's `GetBlock()`, so collisions between hot blocks are possible; how many of the ~1570 dispatcher returns per frame they cause is not measured (upper bound of the gain: the dispatcher's ~8 % of busy time on the Mac).
- `DropRange` keeps every table slot allocated and clears the whole `link[]` (documented in the code); it runs only when the guest invalidates code, not on the hot path.
- Not a bug: after an SVC the block ends with `CheckHalt{PopRSBHint}`, `ChainDynamic` reads the live PC from the state.
- Not applicable: a frame-skip of 3D draws (Mario 3D Land already renders 3D every second video frame); a render worker (WebGL calls are ~12-20 % of a frame).

### Replaying a recorded frame, and the block link table (17 Oct, evening)

- `web/glrec.js` records the WebGL calls of one emulated frame (plus the metadata to rebuild its objects) and `web/probe-glreplay.html` replays it in a bare page, with flags for the context attributes of the emulator, 9 ms of busy work per frame and Emscripten-style heap-view uploads. First version had a bug (buffer data recorded empty: `srcOffset`/`length` are arguments 3 and 4), which made every early replay result worthless; fixed.
- iPhone Air, cold, bare page, the real stream of calls of **one gameplay frame of W1-1 (345 draws, 8711 calls)** with 9 ms of busy work per frame: **58.9 Hz**, JS 9.3 ms, GPU wait 4.1 ms. The same game in the emulator on the same cold phone (`thermal nominal`): 22 Hz. So the stream of WebGL calls and its GPU cost do not explain the 22 Hz; the loss is something else in the emulator's environment. The demo-scene frame (119 draws): 60 Hz, GPU wait 4.2 ms; dropping `bindFramebuffer` 2.8 ms, `bindBufferRange` 2.5 ms, all draws 2.05 ms (Panda re-attaches the framebuffer texture and rebinds the buffers on every draw: 117 `framebufferTexture2D`, 132 `bindFramebuffer`, 566 `bindBuffer` per frame).
- Building the ~176 objects of that frame (about 30 shader programs) takes tens of seconds on the phone: roughly one second per shader program, the same order as the 1-2 s freezes in Mario Kart.
- Moving the loop and the GL into a Worker (OffscreenCanvas): the synthetic frame (9 ms busy + 176 draws) already runs at 60 Hz on the main thread, and in a worker too (60-63). No gain expected from a worker alone.
- Block link table (`kLinkEntries`, direct-mapped on `pc >> 1`): 16 k entries -> 1579 returns to the dispatcher per frame in the title scene (Mac), 64 k -> 553, 256 k -> 320; guest time on the Mac 2.2-3.0 -> 1.7-2.5 ms with 64 k, no further gain with 256 k (4 MB, cache misses). 64 k kept. Phone effect not yet measured (there the game scene has ~9000 returns per frame and guest time grows with the returns).
- Folium (App Store, native, Azahar core `Cytrus`, no JIT: `System/core/arm` has dynarmic, dyncom and `fastinterp`): visibly smoother than this build in Mario Kart on the same phone (user's report).

### Real Safari on the Mac as a lab, and what was wrong with some of my tools (17 Oct, night)

- `scripts/safari-ab.py name=query ...` opens each variant in real Safari (WebKit, the engine class of the iPhone's WKWebView), reads the page's `/live` telemetry and prints loop / paint rate. Chrome at 60 Hz, Safari on the same page: 36-49 Hz in a light scene, the same machine.
- Timer pacing and the desktop layout (which paces with a timer) are always worse in Safari: 15.8 / 17.9 / 24.1 Hz and 6-7 ms of emulator time per frame, against 36-49 Hz and 1.6-4.8 ms with rAF. Same on the phone earlier.
- Results on the Mac are noisy: the same variant gave 47.6 and 35.9 Hz in two runs, with the machine under memory pressure (31 GB used, 133 MB free, 10 GB compressed, 3 M swap-ins) and a forgotten emulator tab in the test Chrome. Only differences of 2x or more are trustworthy there.
- `?ping=1` (MessageChannel main-thread probe) perturbs the page: with it Safari ran at 19-26 Hz, without at 49 Hz. The first version with a 1 ms timer in the loop read as "main thread busy 1000 ms/s" (timer clamp). Do not use it for conclusions about the loop rate.
- Block link table 16 k -> 64 k entries on the phone, W1-1: returns to the dispatcher 9178 -> 3431 per frame, guest ms 7.0 -> 6.5-6.8, speed 44 -> 42.6, loop 22 -> 21.4 Hz: **the dispatcher is not what costs the time**.
- Fast-forward (`ff=<frame>`) works but the phone only gains ~1.4x over real time; one run of it was killed by iOS (WebContent terminated at frame ~3950, 63.9 k JIT modules).
- Audio: the "slow down below the middle of the buffer" regulator (pitch drop) was tried and removed on the user's feedback; back to +/-1.5 %.

### preserveDrawingBuffer off by default (17 Oct, night) - the first thing that moved the heavy scene in WebKit

Real Safari on the Mac, one automatic route (`ff=9800` + `A` every 300 frames, `scripts/safari-ab.py` with `ROUTE=...`), heavy 3D scene (~800-1000 k guest ticks per frame, the W1 world map / level), loop rate of the page:
- default (canvas copied on every present): 13.1, 16.1 Hz; no draws at all: 39.9 Hz (so the draws and what comes with them are the cost); old ring buffer 6.1; pool 10.6.
- `GL_NOPRESERVE` (no copy of the drawing buffer): 19.5, 46.3, 31.2 Hz (emulator time per frame 7.1 / 4.6 / 5.1 ms against 10.4 / 7.2).
- In a light scene it makes no difference (49 Hz either way; phone, demo scene: 53.4 vs 53.5), which is why the first phone test said "no effect".
- Now the default: `preserveDrawingBuffer` is only set with env `GL_PRESERVE` (the page adds it for `shots=`, `glrec` and desktop `report=1` test runs, or `preserve=1`). On the phone, `report=1` runs no longer have a readable canvas at the end (lit-pixel counts read as 0).
- An earlier replay of a recorded frame in Safari (944 draws) gave 5 Hz only because the recorder over-estimated texture upload sizes (27 MB of zeros per frame); fixed (`texBytes`), the result was discarded.

### Live WebAssembly instances cost a fixed price per frame in WebKit (17 Oct, night)
Bare replay page (`web/probe-glreplay.html`, a recorded heavy frame of 345 draws plus 9 ms of busy work, Safari on the Mac), with ballast kept alive while the frame loop runs:

| ballast | loop |
|---|---|
| 0 - 10 000 instances | 59 Hz |
| 20 000 | 57.8 Hz |
| 40 000 | 43 Hz |
| 70 000 | 25 - 33 Hz |
| 256 MB of memory only | no effect |
| 70 000 separate Modules, no instances | 59 Hz |
| 70 000 instances of ONE module | 24.8 Hz |

So the cost is per live `WebAssembly.Instance`, not per module, not per byte. Our JIT makes one Module + Instance per guest block (about 44 000 at boot by frame ~230, 60 - 80 000 later), which fits the "heavy scene is slow only in WebKit" picture. ✅ measured; the mechanism (GC work after each frame) is 🔍 a guess.

First attempt at a fix, `JIT_MAX_BLOCKS=N` (flush the whole block cache at N blocks, opt-in env, default off): N=12000 thrashes the heavy scene (2 Hz, 207 - 215 ms per frame), N=25000 gave 11.5 Hz in one Safari run against 28 - 31 Hz uncapped (13 - 16 earlier in the evening, the baseline moves with load). The 40 000 runs and repeats did not finish. A flush-all cap is therefore NOT a fix. Next candidates: put many blocks into one module/instance (compile in batches), or evict only the start-up blocks once.

### Update (17 Oct, late night): what the instance cost led to
The flush-all cap is not a fix, but two things were found and fixed after this note: JavaScriptCore creates an executable wasm-to-JS stub per *imported function* per module (the Safari WebContent crashes were `ExecutableAllocator::allocate` failing, not RAM), and the cost above is per live Instance. The JIT now imports no functions (helpers go through `call_indirect` on the host table) and packs 256 blocks into one instance (`JIT_BATCH`). Same-scene Safari A/B (`ff=7500&fscriptfile=route-title.txt`): no packing 18.9 Hz, packing 43.6 Hz in the first pair of runs. Full write-up, design, numbers and the route: [webkit-instances-and-batching.md](webkit-instances-and-batching.md).
