/* SPDX-License-Identifier: GPL-3.0-or-later
 * Throughput kernels for the WASM JIT (ARM mode + VFP). Each takes an iteration count in `a`. */
typedef unsigned u32;

u32 b_hash(u32 n, u32 b, u32* m) {  /* tight loop: one block per iteration, dispatch-bound */
    u32 h = 0x811c9dc5u, x = b;
    for (u32 i = 0; i < n; i++) {
        x = x * 1103515245u + 12345u;
        h = (h ^ (x >> 16)) * 16777619u;
        h = (h << 5) | (h >> 27);
    }
    return h;
}

u32 b_memsum(u32 n, u32 passes, u32* m) {  /* sequential loads/stores over a 4096-word buffer */
    u32 sum = 0, i, p;
    for (i = 0; i < 4096; i++) m[i] = i * 2654435761u;
    for (p = 0; p < n; p++) {
        for (i = 0; i < 4096; i++) { sum += m[i] ^ p; m[i] = sum; }
    }
    return sum;
}

static u32 fib(u32 x) { return x < 2 ? x : fib(x - 1) + fib(x - 2); }
u32 b_calls(u32 n, u32 b, u32* m) {  /* recursion: calls and returns through bx lr / ldm pc */
    return fib(n);
}

u32 b_fdot(u32 n, u32 b, u32* m) {  /* VFP: dot products and accumulation */
    float* v = (float*)m;
    float acc = 0.0f;
    u32 i, p;
    for (i = 0; i < 64; i++) v[i] = (float)(int)(i + 1) * 0.25f;
    for (p = 0; p < n; p++) {
        float dot = 0.0f;
        for (i = 0; i < 64; i++) dot += v[i] * v[63 - i];
        acc += dot * 0.001f;
    }
    union { float f; u32 u; } x; x.f = acc; return x.u;
}
