/* SPDX-License-Identifier: GPL-3.0-or-later
 * VFP kernels (ARM mode only). Inputs are kept in the normal range; the JIT runs them with FPSCR = 0 (round to nearest),
 * so the wasm32 build of the same source is a valid reference. */
#include "tests_fp.h"
typedef unsigned u32; typedef unsigned long long u64;

static u32 bits32(float f) { union { float f; u32 u; } x; x.f = f; return x.u; }
static u64 bits64(double f) { union { double f; u64 u; } x; x.f = f; return x.u; }
static float mkf(u32 v, float scale, float off) { return (float)(int)(v & 0xfffff) * scale + off; }
static double mkd(u32 v, double scale, double off) { return (double)(int)(v & 0xfffff) * scale + off; }

u32 f_basic32(u32 a, u32 b, u32* m) {
    float x = mkf(a, 0.37f, -1000.0f), y = mkf(b, 0.0013f, 1.0f);
    float s = x + y, d = x - y, p = x * y, q = x / y, r = __builtin_sqrtf(y * y + 1.0f);
    float n = -x, ab = __builtin_fabsf(d);
    m[0] = bits32(s); m[1] = bits32(d); m[2] = bits32(p); m[3] = bits32(q); m[4] = bits32(r); m[5] = bits32(n); m[6] = bits32(ab);
    return bits32(s) ^ bits32(d) ^ bits32(p) ^ bits32(q) ^ bits32(r) ^ bits32(n) ^ bits32(ab);
}

u32 f_basic64(u32 a, u32 b, u32* m) {
    double x = mkd(a, 0.37, -1000.0), y = mkd(b, 0.0013, 1.0);
    double s = x + y, d = x - y, p = x * y, q = x / y, r = __builtin_sqrt(y * y + 1.0);
    u64 u = bits64(s) ^ bits64(d) ^ bits64(p) ^ bits64(q) ^ bits64(r) ^ bits64(-x) ^ bits64(__builtin_fabs(d));
    m[0] = (u32)u; m[1] = (u32)(u >> 32); m[2] = (u32)bits64(q); m[3] = (u32)(bits64(q) >> 32);
    return (u32)u ^ (u32)(u >> 32);
}

u32 f_loops(u32 a, u32 b, u32* m) {  /* typical vector math: dot products, accumulations */
    float v[8], w[8], acc = 0.0f, mn = 1e30f, mx = -1e30f;
    int i, j;
    for (i = 0; i < 8; i++) { v[i] = mkf(a + i * 977, 0.01f, -50.0f); w[i] = mkf(b + i * 1301, 0.02f, -20.0f); }
    for (j = 0; j < 8; j++) {
        float dot = 0.0f;
        for (i = 0; i < 8; i++) dot += v[i] * w[(i + j) & 7];
        acc = acc * 0.5f + dot;
        if (dot < mn) mn = dot;
        if (dot > mx) mx = dot;
        m[j] = bits32(dot);
    }
    float len = __builtin_sqrtf(acc * acc + 1.0f);
    return bits32(acc) ^ bits32(mn) ^ bits32(mx) ^ bits32(len);
}

u32 f_convert(u32 a, u32 b, u32* m) {
    float x = mkf(a, 0.37f, -50000.0f);
    double y = mkd(b, 0.01, -3000.0);
    int xi = (int)x, yi = (int)y;
    u32 xu = (u32)(x < 0 ? -x : x), yu = (u32)(y < 0 ? -y : y);
    float fx = (float)(int)(a & 0xffffff), fu = (float)(a & 0xfffffff);
    double dd = (double)x, ff = (double)(float)y;
    float back = (float)y;
    m[0] = (u32)xi; m[1] = (u32)yi; m[2] = xu; m[3] = yu; m[4] = bits32(fx); m[5] = bits32(fu); m[6] = (u32)bits64(dd); m[7] = bits32(back);
    return (u32)xi ^ (u32)yi ^ xu ^ yu ^ bits32(fx) ^ bits32(fu) ^ (u32)bits64(dd) ^ (u32)bits64(ff) ^ bits32(back);
}

u32 f_compare(u32 a, u32 b, u32* m) {
    float x = mkf(a, 0.5f, -1000.0f), y = mkf(b, 0.5f, -1000.0f);
    double dx = x, dy = y;
    u32 r = 0;
    r |= (x < y) << 0; r |= (x <= y) << 1; r |= (x > y) << 2; r |= (x >= y) << 3; r |= (x == y) << 4; r |= (x != y) << 5;
    r |= (dx < dy) << 6; r |= (dx > dy) << 7; r |= (dx == dy) << 8;
    float mn = x < y ? x : y, mx = x > y ? x : y;
    m[0] = bits32(mn); m[1] = bits32(mx);
    return r ^ bits32(mn) ^ bits32(mx);
}

u32 f_mixed(u32 a, u32 b, u32* m) {  /* int/float mix, 2D rotation and normalisation */
    float ang = mkf(a, 0.0001f, 0.0f), px = mkf(b, 0.01f, -100.0f), py = mkf(a ^ b, 0.01f, -100.0f);
    /* poor man's sin/cos via Taylor series (no libm) */
    float a2 = ang * ang;
    float c = 1.0f - a2 / 2.0f + a2 * a2 / 24.0f - a2 * a2 * a2 / 720.0f;
    float s = ang - ang * a2 / 6.0f + ang * a2 * a2 / 120.0f - ang * a2 * a2 * a2 / 5040.0f;
    float rx = px * c - py * s, ry = px * s + py * c;
    float len = __builtin_sqrtf(rx * rx + ry * ry);
    float inv = len > 0.0f ? 1.0f / len : 0.0f;
    m[0] = bits32(rx); m[1] = bits32(ry); m[2] = bits32(len); m[3] = bits32(rx * inv); m[4] = bits32(ry * inv);
    return bits32(rx) ^ bits32(ry) ^ bits32(len) ^ bits32(rx * inv) ^ bits32(ry * inv) ^ (u32)(int)(len * 3.0f);
}
