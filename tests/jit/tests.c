/* SPDX-License-Identifier: GPL-3.0-or-later
 * Differential test kernels. Built twice: for ARMv6 (executed by the WASM JIT) and for wasm32 (reference). */
#include <stdint.h>
#include "tests.h"

#if defined(__thumb__)  /* Thumb-1 has no CLZ; ARMv6K cores have no Thumb-2 */
static unsigned clz32(unsigned x) { unsigned n = 0; if (!x) return 32; while (!(x & 0x80000000u)) { x <<= 1; n++; } return n; }
#else
#define clz32(x) ((unsigned)__builtin_clz(x))
#endif
typedef unsigned u32; typedef unsigned long long u64; typedef long long s64; typedef int s32;
typedef uint8_t u8; typedef int8_t s8;
#if defined(__thumb__)  /* Thumb-1 has no UMULL/SMULL: spell the widening multiply out so no libcall is needed */
static u64 umul64(u32 a, u32 b) {
    u32 al = a & 0xffff, ah = a >> 16, bl = b & 0xffff, bh = b >> 16;
    u32 ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    u32 mid = (ll >> 16) + (lh & 0xffff) + (hl & 0xffff);
    u32 lo = (ll & 0xffff) | (mid << 16);
    u32 hi = hh + (lh >> 16) + (hl >> 16) + (mid >> 16);
    return ((u64)hi << 32) | lo;
}
static s64 smul64(s32 a, s32 b) {
    u64 p = umul64((u32)a, (u32)b); u32 hi = (u32)(p >> 32);
    if (a < 0) hi -= (u32)b;
    if (b < 0) hi -= (u32)a;
    return (s64)(((u64)hi << 32) | (u32)p);
}
#else
#define umul64(a, b) ((u64)(a) * (b))
#define smul64(a, b) ((s64)(s32)(a) * (s32)(b))
#endif

u32 t_arith(u32 a, u32 b, u32* m) {
    u32 s = a + b, d = a - b, p = a * b;
    u32 carry = (s < a), borrow = (a < b);
    u32 x = (a ^ b) + (a & b) - (a | b) + ~a;
    m[0] = s; m[1] = d; m[2] = p; m[3] = x;
    return s ^ (d << 1) ^ (p >> 3) ^ (carry << 7) ^ (borrow << 9) ^ x;
}

u32 t_flags(u32 a, u32 b, u32* m) {
    s32 sa = (s32)a, sb = (s32)b;
    u32 r = 0;
    r |= (a < b) << 0;  r |= (a <= b) << 1; r |= (a > b) << 2;  r |= (a >= b) << 3;
    r |= (sa < sb) << 4; r |= (sa <= sb) << 5; r |= (sa > sb) << 6; r |= (sa >= sb) << 7;
    r |= (a == b) << 8; r |= (a != b) << 9; r |= (a == 0) << 10; r |= (sa < 0) << 11;
    s32 ov = ((sa >= 0) == (sb < 0)) && (((sa - sb) >= 0) != (sa >= 0)); /* signed-overflow-ish via wrap */
    r |= (u32)(((a + b) < a)) << 12; r |= (u32)ov << 13;
    m[0] = r;
    return r;
}

u32 t_shifts(u32 a, u32 b, u32* m) {
    u32 n = b & 31, k = (b >> 5) & 63;
    u32 r = 0;
    r ^= a << n;  r ^= a >> n;  r ^= (u32)((s32)a >> n);
    r ^= n ? ((a >> n) | (a << (32 - n))) : a;      /* ror */
    r ^= (k < 32) ? (a << k) : 0;                   /* register shift >= 32 gives 0 */
    r ^= (k < 32) ? (a >> k) : 0;
    r ^= (u32)((s32)a >> (k < 31 ? k : 31));
    r ^= (a << 3) ^ (a >> 5) ^ ((u32)((s32)a >> 7)) ^ ((a >> 9) | (a << 23));
    m[0] = r;
    return r;
}

u32 t_loops(u32 a, u32 b, u32* m) {
    u32 acc = a, i, j;
    for (i = 0; i < (b & 15) + 1; i++) {
        for (j = 0; j < 7; j++) {
            acc = acc * 33 + i;
            if (acc & 0x100) acc ^= j << 4; else acc += j;
            if ((acc & 0xff) == 0x42) break;
        }
        m[i & 15] = acc;
    }
    u32 k = 0;
    while (acc != 0 && k < 100) { acc >>= 1; k++; }
    return acc + k;
}

u32 t_mem(u32 a, u32 b, u32* m) {
    uint8_t* bytes = (uint8_t*)(m + 64);
    int16_t* halves = (int16_t*)(m + 96);
    u32 i, sum = 0;
    for (i = 0; i < 64; i++) bytes[i] = (uint8_t)(a + i * 7);
    for (i = 0; i < 32; i++) halves[i] = (int16_t)(b - i * 1111);
    for (i = 0; i < 64; i++) sum += bytes[i] * (i + 1);
    for (i = 0; i < 32; i++) sum ^= (u32)(int32_t)halves[i] + i;
    for (i = 0; i < 16; i++) m[128 + i] = m[64 + i] ^ m[96 + i];
    return sum;
}

u32 t_sort(u32 a, u32 b, u32* m) {
    u32 i, j, n = 24, seed = a ^ b;
    for (i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; m[i] = seed >> 8; }
    for (i = 0; i < n; i++)
        for (j = 0; j + 1 < n - i; j++)
            if (m[j] > m[j + 1]) { u32 t = m[j]; m[j] = m[j + 1]; m[j + 1] = t; }
    u32 chk = 0;
    for (i = 0; i < n; i++) chk = chk * 31 + m[i];
    return chk;
}

u32 t_bits(u32 a, u32 b, u32* m) {
    u32 r = 0;
    r ^= a ? clz32(a) : 32;
    r ^= __builtin_bswap32(b);
    r ^= (u32)(int32_t)(int8_t)a ^ (u32)(uint8_t)(a >> 8) ^ (u32)(int32_t)(int16_t)b ^ (u32)(uint16_t)(b >> 4);
    u32 pc = 0, x = a ^ b; while (x) { pc += x & 1; x >>= 1; }
    r ^= pc << 8;
    r ^= ((a & 0xff00ff00u) >> 8) | ((a & 0x00ff00ffu) << 8);   /* rev16 */
    m[0] = r;
    return r;
}

u32 t_mul64(u32 a, u32 b, u32* m) {
    u64 ua = umul64(a, b);
    s64 sa = smul64((s32)a, (s32)b);
    u64 acc = ((u64)b << 32 | a) + ua;
    s64 sacc = sa + (s64)acc;
    m[0] = (u32)ua; m[1] = (u32)(ua >> 32); m[2] = (u32)sa; m[3] = (u32)((u64)sa >> 32);
    m[4] = (u32)sacc; m[5] = (u32)((u64)sacc >> 32);
    return (u32)ua ^ (u32)(ua >> 32) ^ (u32)(sa >> 32) ^ (u32)acc ^ (u32)(sacc >> 40);
}

u32 t_div(u32 n, u32 d, u32* m) {  /* shift-subtract division, no libcall */
    u32 q = 0, r = 0, i;
    if (d == 0) return 0xdead;
    for (i = 0; i < 32; i++) {
        r = (r << 1) | (n >> 31);
        n <<= 1;
        q <<= 1;
        if (r >= d) { r -= d; q |= 1; }
    }
    m[0] = q; m[1] = r;
    return q ^ (r << 16);
}

__attribute__((noinline)) static u32 leaf(u32 x, u32 y) { return (x ^ y) * 3 + (x >> 2); }
__attribute__((noinline)) static u32 mid(u32 x, u32 y, u32* m) { u32 s = 0, i; for (i = 0; i < 4; i++) { s += leaf(x + i, y); m[i] = s; } return s; }
u32 t_calls(u32 a, u32 b, u32* m) {
    u32 r = mid(a, b, m);
    r += mid(b, a, m + 8);
    r ^= leaf(r, a);
    return r;
}

u32 t_bytes(u32 a, u32 b, u32* m) {  /* string-like byte processing with pre/post-indexed addressing */
    uint8_t* s = (uint8_t*)m;
    u32 i, h = 5381;
    for (i = 0; i < 100; i++) s[i] = (uint8_t)(a * (i + 1) + b);
    s[99] = 0;
    for (i = 0; s[i] && i < 99; i++) h = h * 33 + s[i];
    for (i = 0; i < 49; i++) { uint8_t t = s[i]; s[i] = s[98 - i]; s[98 - i] = t; }
    for (i = 0; i < 99; i++) h ^= (u32)s[i] << ((i & 3) * 8);
    return h;
}

u32 t_cond(u32 a, u32 b, u32* m) {  /* predicated instructions, min/max/abs/clamp */
    s32 x = (s32)a, y = (s32)b;
    s32 mn = x < y ? x : y, mx = x > y ? x : y;
    s32 ab = x < 0 ? -x : x;
    s32 cl = x < -1000 ? -1000 : (x > 1000 ? 1000 : x);
    u32 umin = a < b ? a : b, umax = a > b ? a : b;
    u32 sel = (a & 1) ? (b & 2 ? 11 : 22) : (b & 4 ? 33 : 44);
    m[0] = (u32)mn; m[1] = (u32)mx; m[2] = (u32)ab; m[3] = (u32)cl; m[4] = umin; m[5] = umax; m[6] = sel;
    return (u32)mn ^ (u32)mx ^ (u32)ab ^ (u32)cl ^ umin ^ umax ^ sel;
}

#if defined(__arm__) && !defined(__thumb__)
#define sat_ssat16(x) __builtin_arm_ssat(x, 16)
#define sat_ssat8(x)  __builtin_arm_ssat(x, 8)
#define sat_usat16(x) __builtin_arm_usat(x, 16)
#define sat_usat8(x)  __builtin_arm_usat(x, 8)
#define sat_qadd(a, b) __builtin_arm_qadd(a, b)
#define sat_qsub(a, b) __builtin_arm_qsub(a, b)
#else
static inline s32 sat_ssat16(s32 x) { return x > 32767 ? 32767 : (x < -32768 ? -32768 : x); }
static inline s32 sat_ssat8(s32 x) { return x > 127 ? 127 : (x < -128 ? -128 : x); }
static inline u32 sat_usat16(s32 x) { return x < 0 ? 0 : (x > 65535 ? 65535 : (u32)x); }
static inline u32 sat_usat8(s32 x) { return x < 0 ? 0 : (x > 255 ? 255 : (u32)x); }
static inline s32 sat_qadd(s32 a, s32 b) {
    s64 s = (s64)a + b;
    return s > 0x7fffffff ? 0x7fffffff : (s < -0x80000000LL ? -0x80000000LL : (s32)s);
}
static inline s32 sat_qsub(s32 a, s32 b) {
    s64 s = (s64)a - b;
    return s > 0x7fffffff ? 0x7fffffff : (s < -0x80000000LL ? -0x80000000LL : (s32)s);
}
#endif

u32 t_sat(u32 a, u32 b, u32* m) {
    s32 sa = (s32)a, sb = (s32)b;
    s32 s16 = sat_ssat16(sa);
    s32 s8 = sat_ssat8(sb);
    u32 u16 = sat_usat16(sa);
    u32 u8 = sat_usat8(sb);
    s32 qa = sat_qadd(sa, sb);
    s32 qs = sat_qsub(sa, sb);
    m[0] = (u32)s16; m[1] = (u32)s8; m[2] = u16; m[3] = u8; m[4] = (u32)qa; m[5] = (u32)qs;
    return (u32)s16 ^ ((u32)s8 << 5) ^ (u16 << 3) ^ (u8 << 11) ^ (u32)qa ^ (u32)qs;
}

#if defined(__arm__) && !defined(__thumb__)
#define p_uadd8(a, b) __builtin_arm_uadd8(a, b)
#define p_usub8(a, b) __builtin_arm_usub8(a, b)
#define p_sadd8(a, b) __builtin_arm_sadd8(a, b)
#define p_ssub8(a, b) __builtin_arm_ssub8(a, b)
#define p_uadd16(a, b) __builtin_arm_uadd16(a, b)
#define p_usub16(a, b) __builtin_arm_usub16(a, b)
#define p_sadd16(a, b) __builtin_arm_sadd16(a, b)
#define p_ssub16(a, b) __builtin_arm_ssub16(a, b)
#define p_usada8(a, b, c) __builtin_arm_usada8(a, b, c)
static inline u32 p_sel_after_uadd8(u32 a, u32 b, u32 x, u32 y) {
    (void)__builtin_arm_uadd8(a, b);
    return __builtin_arm_sel(x, y);
}
#else
static inline u32 p_uadd8(u32 a, u32 b) {
    u32 r = 0;
    for (int i = 0; i < 4; i++) {
        u8 da = (u8)(a >> (i * 8)), db = (u8)(b >> (i * 8));
        r |= (u32)(u8)(da + db) << (i * 8);
    }
    return r;
}
static inline u32 p_usub8(u32 a, u32 b) {
    u32 r = 0;
    for (int i = 0; i < 4; i++) {
        u8 da = (u8)(a >> (i * 8)), db = (u8)(b >> (i * 8));
        r |= (u32)(u8)(da - db) << (i * 8);
    }
    return r;
}
static inline u32 p_sadd8(u32 a, u32 b) {
    u32 r = 0;
    for (int i = 0; i < 4; i++) {
        s8 da = (s8)(a >> (i * 8)), db = (s8)(b >> (i * 8));
        r |= (u32)(u8)(da + db) << (i * 8);
    }
    return r;
}
static inline u32 p_ssub8(u32 a, u32 b) {
    u32 r = 0;
    for (int i = 0; i < 4; i++) {
        s8 da = (s8)(a >> (i * 8)), db = (s8)(b >> (i * 8));
        r |= (u32)(u8)(da - db) << (i * 8);
    }
    return r;
}
static inline u32 p_uadd16(u32 a, u32 b) {
    return ((u32)(uint16_t)((a & 0xffff) + (b & 0xffff))) |
           ((u32)(uint16_t)((a >> 16) + (b >> 16)) << 16);
}
static inline u32 p_usub16(u32 a, u32 b) {
    return ((u32)(uint16_t)((a & 0xffff) - (b & 0xffff))) |
           ((u32)(uint16_t)((a >> 16) - (b >> 16)) << 16);
}
static inline u32 p_sadd16(u32 a, u32 b) {
    return ((u32)(uint16_t)((int16_t)(a & 0xffff) + (int16_t)(b & 0xffff))) |
           ((u32)(uint16_t)((int16_t)(a >> 16) + (int16_t)(b >> 16)) << 16);
}
static inline u32 p_ssub16(u32 a, u32 b) {
    return ((u32)(uint16_t)((int16_t)(a & 0xffff) - (int16_t)(b & 0xffff))) |
           ((u32)(uint16_t)((int16_t)(a >> 16) - (int16_t)(b >> 16)) << 16);
}
static inline u32 p_sel_after_uadd8(u32 a, u32 b, u32 x, u32 y) {
    u32 r = 0;
    for (int i = 0; i < 4; i++) {
        u32 da = (a >> (i * 8)) & 0xff, db = (b >> (i * 8)) & 0xff;
        u32 ge = (da + db >= 256);
        u8 rx = (x >> (i * 8)) & 0xff, ry = (y >> (i * 8)) & 0xff;
        r |= (u32)(ge ? rx : ry) << (i * 8);
    }
    return r;
}
static inline u32 p_usada8(u32 a, u32 b, u32 c) {
    u32 sum = c;
    for (int i = 0; i < 4; i++) {
        u8 da = (a >> (i * 8)) & 0xff, db = (b >> (i * 8)) & 0xff;
        sum += (da > db) ? (da - db) : (db - da);
    }
    return sum;
}
#endif

u32 t_packed(u32 a, u32 b, u32* m) {
    u32 ua8 = p_uadd8(a, b);
    u32 us8 = p_usub8(a, b);
    u32 sa8 = p_sadd8(a, b);
    u32 ss8 = p_ssub8(a, b);
    u32 ua16 = p_uadd16(a, b);
    u32 us16 = p_usub16(a, b);
    u32 sa16 = p_sadd16(a, b);
    u32 ss16 = p_ssub16(a, b);
    u32 sel = p_sel_after_uadd8(a, b, a ^ 0x12345678, b ^ 0x9abcdef0);
    u32 sad = p_usada8(a, b, 42);
    m[0] = ua8; m[1] = us8; m[2] = sa8; m[3] = ss8;
    m[4] = ua16; m[5] = us16; m[6] = sa16; m[7] = ss16;
    m[8] = sel; m[9] = sad;
    return ua8 ^ us8 ^ sa8 ^ ss8 ^ ua16 ^ us16 ^ sa16 ^ ss16 ^ sel ^ sad;
}


