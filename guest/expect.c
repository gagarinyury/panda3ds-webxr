#include <stdio.h>
typedef unsigned u32;
static u32 hash_loop(u32 iters) { u32 h = 0x811c9dc5u, x = 12345; for (u32 i = 0; i < iters; i++) { x = x * 1103515245u + 12345u; h = (h ^ (x >> 16)) * 16777619u; h = (h << 5) | (h >> 27); } return h; }
static float vfp_work(u32 n) { float acc = 0.0f, s = 1.0f; for (u32 i = 1; i <= n; i++) { float t = (float)(int)i; acc += s / (2.0f * t - 1.0f); s = -s; } return __builtin_sqrtf(acc * 16.0f); }
int main(void) { union { float f; u32 u; } x; x.f = vfp_work(1000); printf("hash=%08x\nfpu=%08x\n", hash_loop(2000000), x.u); return 0; }
