# Why heavy 3D scenes were slow in WebKit, and block batching (17 Oct 2026)

Question: Super Mario 3D Land runs 54-60 Hz in Chrome in a heavy scene, but 20-35 Hz in WebKit (Safari on the Mac, WKWebView on the iPhone), while light scenes run 60 Hz everywhere. ✅ = measured, 🔍 = guess.

## 1. The cost is per live `WebAssembly.Instance` ✅

The JIT used to make one Module + one Instance per guest block (44 000 by frame ~230, 60-80 000 later in a level). Bare replay page (`web/probe-glreplay.html`: one recorded heavy frame of 345 draws + 9 ms of busy work), Safari on the Mac, with ballast kept alive:

| ballast | loop |
|---|---|
| 0 - 10 000 instances | 59 Hz |
| 20 000 | 57.8 Hz |
| 40 000 | 43 Hz |
| 70 000 | 25 - 33 Hz |
| 256 MB of memory only | no effect |
| 70 000 Modules, no instances | 59 Hz |
| 70 000 instances of one module | 24.8 Hz |
| 70 000 functions in 700 instances (batches of 100; one export, or every function exported) | 59.2 Hz |

So the price is per *instance*: not per module, per byte, per function or per export. The mechanism (GC work after each frame?) is 🔍. (A first cold replay read 2.6 Hz; a rerun gave 59.2: cold start, discarded.)

Not a fix: flushing the whole block cache at a limit (`JIT_MAX_BLOCKS=N`, opt-in, off by default). N=12 000 thrashes (2 Hz), N=25 000 gave 11.5 Hz in one run against 28-31 Hz uncapped.

## 2. The Safari crashes were executable memory, not RAM ✅

Safari's "A problem repeatedly occurred" for the page was first blamed on memory. The WebContent crash logs (`~/Library/Logs/DiagnosticReports/com.apple.WebKit.WebContent-*.ips`) show `EXC_BREAKPOINT` in `JSC::ExecutableAllocator::allocate` called from `JSC::Wasm::wasmToJS` <- `EntryPlan::generateWasmToJSStubs`. JavaScriptCore makes a wasm-to-JS stub **for every imported function of every module** in the executable-memory pool. Our blocks imported ~50 helper functions each, so the pool ran out (also behind the crashes of the unbatched runs). The same pool is smaller on the iPhone: 🔍 it may be behind some WebContent kills there; not verified.

## 3. The fix: no function imports, helpers through a table, blocks packed into batches

Layout of a block module (`emit_a32.cpp`, `Assemble()`):
- imports: only `e.m` (memory), `e.t` (the block table, table 0) and `e.h` (the host function table = Emscripten's `wasmTable`, table 1). No imported functions, so no wasm-to-JS stubs.
- type section is fixed: type 0 = block function `(i32)->i32`, type `i+1` = signature of helper `kHelpers[i]`.
- a helper is called with `i32.const <host table index>; call_indirect (type i+1) table 1`. The host fills `helper_fn[]` (`EmitOptions::helper_fn`) when the helpers are registered.
- the block's own function has index 0, so its code is **relocatable**: `EmitResult` reports `prefix_len` (header+types+imports), `code_off`, `code_len`.
- `JIT_BATCH=-1` keeps the old per-import layout (`AssembleLegacy`) for comparisons.

Batching (`a32_interface.cpp`, `jit_js_compile`): each block is still instantiated alone (so it runs at once); its `{slot, entry}` is queued. At `JIT_BATCH` blocks (default **256**, `0` = off) JS builds one module from the shared prefix, the function section, exports `b0..b{K-1}` and the concatenated code, instantiates it once, re-points the table slots and `fnCache` to the batch exports; the single-block instances become garbage. Cost of packing: ~0.5 ms per 256-block batch; 114-153 ms in total for 58 000-80 000 blocks (✅ Safari). `jit_js_clear` resets the cached imports and the queue. Telemetry: `packMs`, `batches`, `compMs` in the page's `mem()`.

## 4. Result: same scene, same route, Safari on the Mac

Route: `ff=7500&fscriptfile=route-title.txt` (see §5), 190 s per variant, "heavy" = at least 400 k guest ticks per frame and speed < 80 (`scripts/safari-ab.py`). Do not touch Safari while it runs.

| run | variant | loop Hz (heavy) | speed | emu ms/frame | ticks/frame | samples |
|---|---|---|---|---|---|---|
| rn1 | `JIT_BATCH=0` | 18.9 | 37.4 | 7.1 | 867 k | 92 |
| rb1 | `JIT_BATCH=256` | 43.6 | 58.8 | 5.9 | 784 k | 137 |
| rn2 | `JIT_BATCH=0` | 15.1 | 47.3 | 8.0 | 1180 k | 10 (weak) |
| rb2 | `JIT_BATCH=256` | 43.5 | 59.3 | 6.2 | 782 k | 148 |

Reading: with packing the two runs agree (43.6 and 43.5 Hz, 137 and 148 samples, the same load ~780 k ticks per frame). Without packing: 18.9 and 15.1 Hz. rn2 is the weak one: only 10 heavy samples, a heavier scene (1180 k ticks) and a slow light phase (12 Hz against 41), because the Mac was in use and hot (CPU ~80 C, load average ~4) during that run; treat it as noisy. So: one clean pair plus two consistent packed runs, and two unpacked runs that both sit at 15-19 Hz. Strong evidence, not yet a long series; a clean repeat with the Mac left alone is still worth doing. ✅ measured, mechanism 🔍.

"speed" is the emulated rate including catch-up frames; **loop/paint Hz is what is shown**. Run-to-run noise is large (the same variant gave 31-46 Hz on an earlier day), the scene load differs between runs (636 k-949 k ticks per frame), so judge by repeats on the same scene. Earlier single, dirty pair: batch 34.5 Hz vs no batch 20.4 Hz (the player touched the page).

### iPhone Air (17 Oct, 22:30, thermal nominal, build with `JIT_BATCH=256`, played by hand to W1-1)

Light scenes (title, menus, cutscenes, frames 1000-5500): loop 47-63 Hz, 4.2-5.5 ms emulator time per frame, nothing killed. **World 1-1 (frames 5660-8170): loop 16-22 Hz, speed 32-44, emulator 7.8-10.6 ms per frame**, guest 5.4 ms, 3545 dispatcher returns per frame, ~74 700 blocks. The same scene on the same phone before packing was 21.4-22 Hz (notes of 17 Oct). **So the packing did not move the phone: the Safari-on-Mac gain (19 -> 44 Hz) does not carry over to the iPhone.** The loop period (~47 ms) is far larger than the emulator time (~8.4 ms), so most of the frame is spent outside our code (present / GPU / compositor / GC); the replay of a recorded W1-1 frame in a bare page on the same phone gave 58.9 Hz, so the stream of GL calls alone does not explain it either. Whether the per-instance cost exists on the phone: no, see the next section.

### Chrome (Comet 154, Mac, packed blocks, same route; one short run, 17 Oct 22:37)

Right after the fast-forward ended (frame 7518-7638, W1-1, 740-800 k ticks per frame): speed 59-60, emulator 5.4-5.8 ms per frame, **paint 31-33 Hz**, only three samples (the loop-rate field reads 58-84 there because it still contains the fast-forward bursts). Not enough for a verdict; the run was stopped on request. Do not read it as "Chrome 54-60 Hz with packing": the paint rate of this run is lower than the earlier Chrome numbers.

### iPhone Air: no per-instance cost up to 40 000 instances (17 Oct, 22:50) ✅

Bare replay page on the phone (`probe-glreplay.html`, the recorded W1-1 frame: 345 draws, 8711 calls, 9 ms busy work, flags `DSPCF`, rAF), with ballast of live instances (`growBallast`: one module + instance each) kept alive:

| live instances | loop | repaints | JS per frame |
|---|---|---|---|
| 0 | 59.0 Hz | 59.0 | 9.34 ms |
| 10 000 | 59.0 | 59.0 | 9.28 |
| 20 000 | 59.1 | 59.2 | 9.27 |
| 40 000 | 58.8 | 58.9 | 9.35 |

On the Mac the same test gave 43 Hz at 40 000. So the phone does **not** have the per-instance price (at least up to 40 000; the game keeps ~74 000 blocks, 70 000 was not run on the phone), which is why packing the blocks did not change W1-1 there. The 16-22 Hz of the phone in W1-1 comes from something else; the replay of the same GL stream with the same busy time holds 59 Hz, so the difference must be in what the real emulator does besides that (candidates: audio thread, uploads from the 256 MB heap, real JIT code running between the calls, memory pressure of the page). (The user agent string of the page says iOS 18_7: WebKit freezes it, ignore.)

### iPhone Air, the real game in W1-1: the cost is the draw calls (17 Oct, night) ✅

The route now runs on the phone without hands: `ios/prepare-site.sh` copies `web/*.txt` into the app, and `ios-watch.py` is started with `-page index.html -q "rom=/docs/sm3dl.3ds&frames=0&turbo=0&name=rp1&report=1&dbg=0&env=&gl=1&sd=&script=&shots=0&pace=&persist=1&ff=7500&fscriptfile=route-title.txt"` (the whole query is replaced by `-q`). Fast-forward to frame 7500 takes about a minute and a half on the phone (64-80 fps) and the WebContent process was not killed (78 700 blocks, packed).

| run | variant | W1-1 loop | speed | emulator ms/frame |
|---|---|---|---|---|
| rp1 | normal | 21-22 Hz | 42.4-44 | 8.5-8.7 |
| rp2 | `env=GL_NODRAW:1` (no draw calls at all, uploads kept) | **59-63 Hz** | 58.8-63.5 | 6.1 |

So on the phone the JIT, the guest code, the sound, the 256 MB memory, the heap-view uploads (all tested in the replay page: 59 Hz with each) are not what costs the time: with the draws removed the same scene runs in real time. ~40 ms of every 50 ms frame goes into drawing (GPU / ANGLE-Metal in the WebContent and GPU processes). Which part of the drawing is not known yet (candidates 🔍: texture uploads of the real game, the many framebuffer switches with render targets, shader variety, fragment cost of the real textures). Careful with the note about `GL_NODRAW` in performance-notes (a no-draw run is not a clean test of the uploads), but here the comparison is of whole scenes. Next: take the draws away in parts (by framebuffer / by shader / small viewport / smaller textures) on the same route.

## 5. Reproducible route to World 1-1 without hands

Game facts (✅ user + screenshots): StreetPass dialog #1 "OK" takes A (~frame 1600); dialog #2 "Activate StreetPass?" needs a touch on [Cancel]; "A NEW GAME" takes A; intro cutscene; the Bowser story pages need a touch on the bottom-right corner (A and Start do not work reliably); on the world map the red "!" button on the bottom screen must be tapped (twice); then World 1-1 loads (~frame 6400) and gameplay starts (~6600). `ff=7500` is enough.

`web/index.html` additions:
- `fscript=T:<frame>:<hold>:<x>:<y>` touches the bottom screen (0..319 x 0..239) for `hold` frames (`Module._web_touch(x,y,1)`, then `(0,0,0)`).
- `fscriptfile=route-title.txt` loads a script from a file (comma-separated entries, sorted by frame after loading). `web/route-title.txt` has 295 entries: for f in 1700..7000 step 90: `A:f:10`, `T:f+25:8:56:195` (Cancel), `T:f+45:8:310:228` (corner), `T:f+60:8:66:147` and `T:f+75:8:66:147` (red button). A is pressed constantly; the extra taps do no harm in the level.
- `ROUTE="..." SECS=190 python3 scripts/safari-ab.py name=query ...`, e.g. `rn1=env=JIT_BATCH:0 rb1=env=JIT_BATCH:256`; telemetry lands in `run/web-reports/live-<name>.jsonl`.

## 6. Not done / open

- iPhone: batching measured (above): no change in W1-1 (16-22 Hz). The ballast replay on the phone is done (no per-instance cost up to 40 000). Found: the draw calls (see the section above, no-draw = 59-63 Hz). Next: split the drawing cost into parts.
- Compile hitches in new areas (130-250 blocks, 30-47 ms), shader compile freezes (Mario Kart: ~1 s per program), missing shadows under characters in W1-1 (`docs/related-work-and-ios-hang.md`), Smash black screen.
- Not tried: `persist=1` (IndexedDB game data) to shorten the route; evicting only start-up blocks.
