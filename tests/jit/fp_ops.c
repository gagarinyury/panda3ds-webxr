/* SPDX-License-Identifier: GPL-3.0-or-later
 * One VFP kernel for the FP differential test: for each operand pair compute add, sub, mul, div, sqrt and the VCMP flags. */
typedef unsigned u32;

void fp_ops(u32 n, const u32* in, u32* out) {
    for (u32 i = 0; i < n; i++) {
        union { u32 u; float f; } a, b, r;
        a.u = in[2 * i]; b.u = in[2 * i + 1];
        r.f = a.f + b.f; out[6 * i + 0] = r.u;
        r.f = a.f - b.f; out[6 * i + 1] = r.u;
        r.f = a.f * b.f; out[6 * i + 2] = r.u;
        r.f = a.f / b.f; out[6 * i + 3] = r.u;
        r.f = __builtin_sqrtf(a.f); out[6 * i + 4] = r.u;
        u32 fpscr;
        asm volatile("vcmp.f32 %[a], %[b]\n\tvmrs %[r], fpscr" : [r] "=r"(fpscr) : [a] "t"(a.f), [b] "t"(b.f) : "cc");
        out[6 * i + 5] = fpscr & 0xf0000000u;
    }
}
