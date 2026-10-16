/* SPDX-License-Identifier: GPL-3.0-or-later
 * Freestanding 3DS homebrew (no libctru): prints through svcOutputDebugString, runs a hash loop and a VFP computation,
 * prints the results and exits. Built for ARMv6K + VFPv2 and wrapped into a .3dsx by build.sh. */
typedef unsigned u32;

static void out(const char* s, int n);
static u32 hash_loop(u32 iters);
static float vfp_work(u32 n);
static int hex(char* dst, u32 v);

__attribute__((noreturn)) void _start(void) {
    /* the greeting lives inside the code (no .rodata in our bare 3DSX): adr + inline .ascii */
    asm volatile(
        "adr r0, 1f\n"
        "mov r1, #23\n"
        "svc 0x3D\n"
        "b 2f\n"
        "1: .ascii \"hello from the WASM JIT\"\n"
        ".balign 4\n"
        "2:\n" ::: "r0", "r1", "r2", "r3", "r12", "lr", "memory");

    char buf[32];
    u32 h = hash_loop(2000000);
    buf[0] = 'h'; buf[1] = 'a'; buf[2] = 's'; buf[3] = 'h'; buf[4] = '=';
    int n = 5 + hex(buf + 5, h);
    out(buf, n);

    float f = vfp_work(1000);
    union { float f; u32 u; } x; x.f = f;
    buf[0] = 'f'; buf[1] = 'p'; buf[2] = 'u'; buf[3] = '=';
    n = 4 + hex(buf + 4, x.u);
    out(buf, n);

    /* svcExitProcess */
    register u32 r0 asm("r0");
    (void)r0;
    asm volatile("svc 0x03");
    for (;;) {}
}

static void out(const char* s, int n) {
    register const char* r0 asm("r0") = s;
    register int r1 asm("r1") = n;
    asm volatile("svc 0x3D" : "+r"(r0), "+r"(r1) : : "memory", "r2", "r3", "r12", "lr");
}

static u32 hash_loop(u32 iters) {
    u32 h = 0x811c9dc5u, x = 12345;
    for (u32 i = 0; i < iters; i++) {
        x = x * 1103515245u + 12345u;
        h = (h ^ (x >> 16)) * 16777619u;
        h = (h << 5) | (h >> 27);
    }
    return h;
}

static float vfp_work(u32 n) {
    float acc = 0.0f, s = 1.0f;
    for (u32 i = 1; i <= n; i++) {
        float t = (float)(int)i;
        acc += s / (2.0f * t - 1.0f);   /* Leibniz series for pi/4 */
        s = -s;
    }
    return __builtin_sqrtf(acc * 16.0f);
}

static int hex(char* dst, u32 v) {
    for (int i = 0; i < 8; i++) {
        u32 d = (v >> (28 - 4 * i)) & 15;
        dst[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    }
    return 8;
}
