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
it for this. The buffer pool (`BufferPoolStreamBuffer` in `third_party/duckstation/gl/stream_buffer.cpp`, default in the web build; env `GL_STREAM_BUFFERDATA:1` / `GL_STREAM_SUBDATA:1` select the old paths)
is the cheap step: 2.7x cheaper per draw in the benchmark. Not yet measured in the game on the phone.
