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

Light scenes (title, menus, cutscenes, frames 1000-5500): loop 47-63 Hz, 4.2-5.5 ms emulator time per frame, nothing killed. **World 1-1 (frames 5660-8170): loop 16-22 Hz, speed 32-44, emulator 7.8-10.6 ms per frame**, guest 5.4 ms, 3545 dispatcher returns per frame, ~74 700 blocks. The same scene on the same phone before packing was 21.4-22 Hz (notes of 17 Oct). **So the packing did not move the phone: the Safari-on-Mac gain (19 -> 44 Hz) does not carry over to the iPhone.** The loop period (~47 ms) is far larger than the emulator time (~8.4 ms), so most of the frame is spent outside our code (present / GPU / compositor / GC); the replay of a recorded W1-1 frame in a bare page on the same phone gave 58.9 Hz, so the stream of GL calls alone does not explain it either. Open: whether the per-instance cost also exists on the phone (run `probe-glreplay.html` with ballast on the phone), and what else differs between the replay page and the emulator.

### Chrome (Comet 154, Mac, packed blocks, same route; one short run, 17 Oct 22:37)

Right after the fast-forward ended (frame 7518-7638, W1-1, 740-800 k ticks per frame): speed 59-60, emulator 5.4-5.8 ms per frame, **paint 31-33 Hz**, only three samples (the loop-rate field reads 58-84 there because it still contains the fast-forward bursts). Not enough for a verdict; the run was stopped on request. Do not read it as "Chrome 54-60 Hz with packing": the paint rate of this run is lower than the earlier Chrome numbers.

## 5. Reproducible route to World 1-1 without hands

Game facts (✅ user + screenshots): StreetPass dialog #1 "OK" takes A (~frame 1600); dialog #2 "Activate StreetPass?" needs a touch on [Cancel]; "A NEW GAME" takes A; intro cutscene; the Bowser story pages need a touch on the bottom-right corner (A and Start do not work reliably); on the world map the red "!" button on the bottom screen must be tapped (twice); then World 1-1 loads (~frame 6400) and gameplay starts (~6600). `ff=7500` is enough.

`web/index.html` additions:
- `fscript=T:<frame>:<hold>:<x>:<y>` touches the bottom screen (0..319 x 0..239) for `hold` frames (`Module._web_touch(x,y,1)`, then `(0,0,0)`).
- `fscriptfile=route-title.txt` loads a script from a file (comma-separated entries, sorted by frame after loading). `web/route-title.txt` has 295 entries: for f in 1700..7000 step 90: `A:f:10`, `T:f+25:8:56:195` (Cancel), `T:f+45:8:310:228` (corner), `T:f+60:8:66:147` and `T:f+75:8:66:147` (red button). A is pressed constantly; the extra taps do no harm in the level.
- `ROUTE="..." SECS=190 python3 scripts/safari-ab.py name=query ...`, e.g. `rn1=env=JIT_BATCH:0 rb1=env=JIT_BATCH:256`; telemetry lands in `run/web-reports/live-<name>.jsonl`.

## 6. Not done / open

- iPhone: batching measured (above): no change in W1-1 (16-22 Hz). Next: ballast replay on the phone, and what else separates the phone's emulator loop from the bare replay.
- Compile hitches in new areas (130-250 blocks, 30-47 ms), shader compile freezes (Mario Kart: ~1 s per program), missing shadows under characters in W1-1 (`docs/related-work-and-ios-hang.md`), Smash black screen.
- Not tried: `persist=1` (IndexedDB game data) to shorten the route; evicting only start-up blocks.
