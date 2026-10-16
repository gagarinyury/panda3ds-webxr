// SPDX-License-Identifier: GPL-3.0-or-later
// Compares the soft-float VFP implementation (fp_soft.cpp) against the Apple Silicon FPU under every
// FPCR mode (rounding x flush-to-zero x default-NaN). Results and cumulative exception flags must match bit for bit.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <string>

#include "dynarmic/backend/wasm/fp_soft.h"

using namespace Dynarmic;
using u32 = std::uint32_t; using u64 = std::uint64_t;
namespace S = Dynarmic::Backend::Wasm::FPSoft;

static inline void set_fpcr(u64 v) { asm volatile("msr fpcr, %0" ::"r"(v)); }
static inline void set_fpsr(u64 v) { asm volatile("msr fpsr, %0" ::"r"(v)); }
static inline u64 get_fpsr() { u64 v; asm volatile("mrs %0, fpsr" : "=r"(v)); return v; }
static inline float F(u32 x) { float f; std::memcpy(&f, &x, 4); return f; }
static inline double D(u64 x) { double f; std::memcpy(&f, &x, 8); return f; }
static inline u32 B(float f) { u32 x; std::memcpy(&x, &f, 4); return x; }
static inline u64 B(double f) { u64 x; std::memcpy(&x, &f, 8); return x; }

#define HW2(NAME, T, REG, INS) static inline u64 NAME(u64 a, u64 b) { T x = (sizeof(T)==4) ? (T)F((u32)a) : 0; (void)x; return 0; }
// hardware primitives (inline asm so the compiler can neither fold nor reorder them)
#define HW_BIN32(NAME, INS) static u32 NAME(u32 a, u32 b) { float fa = F(a), fb = F(b), fr; asm volatile(INS " %s0, %s1, %s2" : "=w"(fr) : "w"(fa), "w"(fb)); return B(fr); }
#define HW_BIN64(NAME, INS) static u64 NAME(u64 a, u64 b) { double fa = D(a), fb = D(b), fr; asm volatile(INS " %d0, %d1, %d2" : "=w"(fr) : "w"(fa), "w"(fb)); return B(fr); }
HW_BIN32(hw_add32, "fadd") HW_BIN32(hw_sub32, "fsub") HW_BIN32(hw_mul32, "fmul") HW_BIN32(hw_div32, "fdiv")
HW_BIN64(hw_add64, "fadd") HW_BIN64(hw_sub64, "fsub") HW_BIN64(hw_mul64, "fmul") HW_BIN64(hw_div64, "fdiv")
static u32 hw_sqrt32(u32 a) { float fa = F(a), fr; asm volatile("fsqrt %s0, %s1" : "=w"(fr) : "w"(fa)); return B(fr); }
static u64 hw_sqrt64(u64 a) { double fa = D(a), fr; asm volatile("fsqrt %d0, %d1" : "=w"(fr) : "w"(fa)); return B(fr); }
static u64 hw_s2d(u32 a) { float fa = F(a); double fr; asm volatile("fcvt %d0, %s1" : "=w"(fr) : "w"(fa)); return B(fr); }
static u32 hw_d2s(u64 a) { double fa = D(a); float fr; asm volatile("fcvt %s0, %d1" : "=w"(fr) : "w"(fa)); return B(fr); }
static u32 hw_cmp32(u32 a, u32 b, bool e) { float fa = F(a), fb = F(b); u64 nzcv;
    if (e) asm volatile("fcmpe %s1, %s2\n mrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc"); else asm volatile("fcmp %s1, %s2\n mrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc");
    return (u32)nzcv; }
static u32 hw_cmp64(u64 a, u64 b, bool e) { double fa = D(a), fb = D(b); u64 nzcv;
    if (e) asm volatile("fcmpe %d1, %d2\n mrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc"); else asm volatile("fcmp %d1, %d2\n mrs %0, nzcv" : "=r"(nzcv) : "w"(fa), "w"(fb) : "cc");
    return (u32)nzcv; }
#define HW_F2I(NAME, TF, REGF, INS, FB) static u32 NAME(TF a) { auto fa = a; u32 r; asm volatile(INS " %w0, %" REGF "1, #" #FB : "=r"(r) : "w"(fa)); return r; }
HW_F2I(hw_s2s32_0, float, "s", "fcvtzs", 1) HW_F2I(hw_s2u32_0, float, "s", "fcvtzu", 1)
HW_F2I(hw_s2s32_8, float, "s", "fcvtzs", 8) HW_F2I(hw_s2u32_8, float, "s", "fcvtzu", 8)
HW_F2I(hw_d2s32_0, double, "d", "fcvtzs", 1) HW_F2I(hw_d2u32_0, double, "d", "fcvtzu", 1)
HW_F2I(hw_d2s32_8, double, "d", "fcvtzs", 8) HW_F2I(hw_d2u32_8, double, "d", "fcvtzu", 8)
#define HW_I2F(NAME, TF, REGF, INS, FB) static TF NAME(u32 a) { TF r; asm volatile(INS " %" REGF "0, %w1, #" #FB : "=w"(r) : "r"(a)); return r; }
HW_I2F(hw_s32_s_1, float, "s", "scvtf", 1) HW_I2F(hw_u32_s_1, float, "s", "ucvtf", 1) HW_I2F(hw_s32_s_8, float, "s", "scvtf", 8) HW_I2F(hw_u32_s_8, float, "s", "ucvtf", 8)
HW_I2F(hw_s32_d_1, double, "d", "scvtf", 1) HW_I2F(hw_u32_d_1, double, "d", "ucvtf", 1) HW_I2F(hw_s32_d_8, double, "d", "scvtf", 8) HW_I2F(hw_u32_d_8, double, "d", "ucvtf", 8)
// (fixed-point conversion with #1 fractional bit is deliberate: #0 is not encodable for scvtf/fcvtz* fixed forms)

static u64 rng_state = 0x9E3779B97F4A7C15ull;
static u64 Rnd() { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17; return rng_state; }

static std::vector<u32> Special32() {
    std::vector<u32> v = {0, 1, 2, 0x7fffff, 0x400000, 0x00800000, 0x00800001, 0x3f800000, 0x40400000, 0x3dcccccd, 0x3f7fffff, 0x3f800001, 0x7f7fffff, 0x7f000000, 0x7f800000,
                          0x7fc00000, 0x7fc12345, 0x7f800001, 0x7fa00000, 0x4b000000, 0x4affffff, 0x4f000000, 0x4eff0000, 0x5f000000, 0x2f800000, 0x34000000, 0x33800000, 0x1e3ce508};
    const size_t n = v.size();
    for (size_t i = 0; i < n; i++) v.push_back(v[i] | 0x80000000u);
    return v;
}
static std::vector<u64> Special64() {
    std::vector<u64> v = {0, 1, 2, 0xfffffffffffffull, 0x8000000000000ull, 0x0010000000000000ull, 0x0010000000000001ull, 0x3ff0000000000000ull, 0x4008000000000000ull, 0x3fb999999999999aull, 0x3fefffffffffffffull,
                          0x3ff0000000000001ull, 0x7fefffffffffffffull, 0x7fe0000000000000ull, 0x7ff0000000000000ull, 0x7ff8000000000000ull, 0x7ff8123456789abcull, 0x7ff0000000000001ull,
                          0x7ff4000000000000ull, 0x4330000000000000ull, 0x432fffffffffffffull, 0x41e0000000000000ull, 0x41dfffffffc00000ull, 0x3e70000000000000ull};
    const size_t n = v.size();
    for (size_t i = 0; i < n; i++) v.push_back(v[i] | 0x8000000000000000ull);
    return v;
}
static u32 Rnd32() {
    const u64 r = Rnd();
    switch (r & 7) {
    case 0: return (u32)(r >> 8);                                        // any bit pattern
    case 1: return (u32)(r >> 8) & 0x807fffff;                           // denormal / zero
    case 2: return ((u32)(r >> 8) & 0x807fffff) | (((u32)(r >> 40) % 8) << 23);   // tiny exponents
    case 3: return ((u32)(r >> 8) & 0x807fffff) | ((254 - ((u32)(r >> 40) % 6)) << 23);  // huge exponents
    case 4: return ((u32)(r >> 8) & 0x807fffff) | ((120 + ((u32)(r >> 40) % 16)) << 23);  // near 1
    default: return ((u32)(r >> 8) & 0x80000000u) | ((u32)(r >> 20) & 0x7fffff) | ((((u32)(r >> 44)) % 255) << 23);
    }
}
static u64 Rnd64() {
    const u64 r = Rnd();
    switch (r & 7) {
    case 0: return Rnd();
    case 1: return Rnd() & 0x800fffffffffffffull;
    case 2: return (Rnd() & 0x800fffffffffffffull) | ((u64)(r >> 8) % 8 << 52);
    case 3: return (Rnd() & 0x800fffffffffffffull) | ((u64)(2046 - ((r >> 8) % 6)) << 52);
    case 4: return (Rnd() & 0x800fffffffffffffull) | ((u64)(1016 + ((r >> 8) % 16)) << 52);
    default: return (Rnd() & 0x8000000000000000ull) | (Rnd() & 0xfffffffffffffull) | ((u64)((r >> 8) % 2047) << 52);
    }
}

static u32 flags_hw() { return (u32)get_fpsr() & 0x9f; }

struct Stats { long total = 0, fail = 0; std::string first; };
static int g_rm, g_fz, g_dn;
template<typename T>
static void Check(Stats& st, const char* op, T hw, T sw, u32 hwf, u32 swf, u64 a, u64 b) {
    st.total++;
    if (hw != sw || hwf != swf) {
        st.fail++;
        if (st.first.empty()) {
            char buf[300];
            std::snprintf(buf, sizeof buf, "%s rm=%d fz=%d dn=%d a=%llx b=%llx hw=%llx sw=%llx hwflags=%02x swflags=%02x", op, g_rm, g_fz, g_dn, (unsigned long long)a, (unsigned long long)b, (unsigned long long)hw, (unsigned long long)sw, hwf, swf);
            st.first = buf;
        }
    }
}

int main(int argc, char** argv) {
    const int random_pairs = argc > 1 ? std::atoi(argv[1]) : 3000;
    std::vector<u32> s32 = Special32(); std::vector<u64> s64 = Special64();
    std::map<std::string, Stats> stats;
    long grand_total = 0, grand_fail = 0;
    for (int rm = 0; rm < 4; rm++) for (int fz = 0; fz < 2; fz++) for (int dn = 0; dn < 2; dn++) {
        g_rm = rm; g_fz = fz; g_dn = dn;
        const u64 fpcr_bits = ((u64)rm << 22) | ((u64)fz << 24) | ((u64)dn << 25);
        const FP::FPCR cr{(u32)fpcr_bits};
        std::vector<std::pair<u32, u32>> p32; std::vector<std::pair<u64, u64>> p64;
        for (u32 a : s32) for (u32 b : s32) p32.push_back({a, b});
        for (int i = 0; i < random_pairs; i++) p32.push_back({Rnd32(), Rnd32()});
        for (u64 a : s64) for (u64 b : s64) p64.push_back({a, b});
        for (int i = 0; i < random_pairs; i++) p64.push_back({Rnd64(), Rnd64()});

#define RUN(NAME, T, HWEXPR, SWEXPR)                                                      \
        { Stats& st = stats[NAME];                                                        \
          set_fpcr(fpcr_bits); set_fpsr(0);                   \
          T hw = HWEXPR; u32 hwf = flags_hw();                                            \
          set_fpcr(0);                                                      \
          FP::FPSR sr{0}; T sw = SWEXPR; Check<T>(st, NAME, hw, sw, hwf, sr.Value() & 0x9f, a, b); }

        for (auto [a, b] : p32) {
            RUN("add32", u32, hw_add32(a, b), S::Add<u32>(a, b, cr, sr))
            RUN("sub32", u32, hw_sub32(a, b), S::Sub<u32>(a, b, cr, sr))
            RUN("mul32", u32, hw_mul32(a, b), S::Mul<u32>(a, b, cr, sr))
            RUN("div32", u32, hw_div32(a, b), S::Div<u32>(a, b, cr, sr))
            RUN("sqrt32", u32, hw_sqrt32(a), S::Sqrt<u32>(a, cr, sr))
            RUN("cmp32", u32, hw_cmp32(a, b, false), S::Compare<u32>(a, b, false, cr, sr))
            RUN("cmpe32", u32, hw_cmp32(a, b, true), S::Compare<u32>(a, b, true, cr, sr))
            RUN("s2d", u64, hw_s2d(a), S::SingleToDouble(a, cr, cr.RMode(), sr))
            RUN("s2s32", u32, hw_s2s32_0(F(a)), S::ToFixed<u32>(a, 32, 1, false, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("s2u32", u32, hw_s2u32_0(F(a)), S::ToFixed<u32>(a, 32, 1, true, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("s2s32f8", u32, hw_s2s32_8(F(a)), S::ToFixed<u32>(a, 32, 8, false, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("s2u32f8", u32, hw_s2u32_8(F(a)), S::ToFixed<u32>(a, 32, 8, true, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("i2s_s", u32, B(hw_s32_s_1(a)), S::FromFixed<u32>(a, 32, 1, false, cr, cr.RMode(), sr))
            RUN("i2s_u", u32, B(hw_u32_s_1(a)), S::FromFixed<u32>(a, 32, 1, true, cr, cr.RMode(), sr))
            RUN("i2s_sf8", u32, B(hw_s32_s_8(a)), S::FromFixed<u32>(a, 32, 8, false, cr, cr.RMode(), sr))
            RUN("i2s_uf8", u32, B(hw_u32_s_8(a)), S::FromFixed<u32>(a, 32, 8, true, cr, cr.RMode(), sr))
            RUN("i2d_s", u64, B(hw_s32_d_1(a)), S::FromFixed<u64>(a, 32, 1, false, cr, cr.RMode(), sr))
            RUN("i2d_u", u64, B(hw_u32_d_1(a)), S::FromFixed<u64>(a, 32, 1, true, cr, cr.RMode(), sr))
        }
        for (auto [a, b] : p64) {
            RUN("add64", u64, hw_add64(a, b), S::Add<u64>(a, b, cr, sr))
            RUN("sub64", u64, hw_sub64(a, b), S::Sub<u64>(a, b, cr, sr))
            RUN("mul64", u64, hw_mul64(a, b), S::Mul<u64>(a, b, cr, sr))
            RUN("div64", u64, hw_div64(a, b), S::Div<u64>(a, b, cr, sr))
            RUN("sqrt64", u64, hw_sqrt64(a), S::Sqrt<u64>(a, cr, sr))
            RUN("cmp64", u32, hw_cmp64(a, b, false), S::Compare<u64>(a, b, false, cr, sr))
            RUN("cmpe64", u32, hw_cmp64(a, b, true), S::Compare<u64>(a, b, true, cr, sr))
            RUN("d2s", u32, hw_d2s(a), S::DoubleToSingle(a, cr, cr.RMode(), sr))
            RUN("d2s32", u32, hw_d2s32_0(D(a)), S::ToFixed<u64>(a, 32, 1, false, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("d2u32", u32, hw_d2u32_0(D(a)), S::ToFixed<u64>(a, 32, 1, true, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("d2s32f8", u32, hw_d2s32_8(D(a)), S::ToFixed<u64>(a, 32, 8, false, cr, FP::RoundingMode::TowardsZero, sr))
            RUN("d2u32f8", u32, hw_d2u32_8(D(a)), S::ToFixed<u64>(a, 32, 8, true, cr, FP::RoundingMode::TowardsZero, sr))
        }
    }
    for (auto& [name, st] : stats) {
        grand_total += st.total; grand_fail += st.fail;
        if (st.fail) std::printf("FAIL %-9s %ld/%ld differ; first: %s\n", name.c_str(), st.fail, st.total, st.first.c_str());
        else std::printf("PASS %-9s %ld cases\n", name.c_str(), st.total);
    }
    std::printf("\n%ld comparisons, %ld mismatches\n", grand_total, grand_fail);
    return grand_fail ? 1 : 0;
}
