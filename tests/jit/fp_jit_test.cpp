// SPDX-License-Identifier: GPL-3.0-or-later
// VFP differential test: ARM code run through the WASM JIT (inline fast paths and helper fallbacks) must match the soft-float
// reference (which itself matches Apple Silicon bit for bit) for every operand pair in every FPSCR mode.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "dynarmic/backend/wasm/fp_soft.h"
#include "env.h"

namespace S = Dynarmic::Backend::Wasm::FPSoft;

static u64 rs = 0x9E3779B97F4A7C15ull;
static u64 Rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static u32 Rnd32() {
    const u64 r = Rnd();
    switch (r & 7) {
    case 0: return (u32)(r >> 8);
    case 1: return (u32)(r >> 8) & 0x807fffff;
    case 2: return ((u32)(r >> 8) & 0x807fffff) | (((u32)(r >> 40) % 8) << 23);
    case 3: return ((u32)(r >> 8) & 0x807fffff) | ((254 - ((u32)(r >> 40) % 6)) << 23);
    case 4: return ((u32)(r >> 8) & 0x807fffff) | ((120 + ((u32)(r >> 40) % 16)) << 23);
    default: return ((u32)(r >> 8) & 0x80000000u) | ((u32)(r >> 20) & 0x7fffff) | ((((u32)(r >> 44)) % 255) << 23);
    }
}

int main(int argc, char** argv) {
    const int random_pairs = argc > 1 ? std::atoi(argv[1]) : 6000;
    std::vector<u32> special = {0, 1, 2, 0x7fffff, 0x400000, 0x00800000, 0x00800001, 0x3f800000, 0x40400000, 0x3dcccccd, 0x3f7fffff, 0x3f800001, 0x7f7fffff, 0x7f000000, 0x7f800000,
                                0x7fc00000, 0x7fc12345, 0x7f800001, 0x7fa00000, 0x4b000000, 0x4affffff, 0x4f000000, 0x2f800000, 0x34000000, 0x33800000, 0x1e3ce508, 0x00ffffff, 0x01000000};
    const size_t ns = special.size();
    for (size_t i = 0; i < ns; i++) special.push_back(special[i] | 0x80000000u);
    std::vector<u32> pairs;
    for (u32 a : special) for (u32 b : special) { pairs.push_back(a); pairs.push_back(b); }
    for (int i = 0; i < random_pairs; i++) { pairs.push_back(Rnd32()); pairs.push_back(Rnd32()); }
    const u32 n = pairs.size() / 2;

    Env env;
    env.mem.resize(0x1000000);
    constexpr u32 CODE = 0x10000, SENTINEL = 0x30000, IN = 0x100000, OUT = 0x600000, STACK = 0xff0000;
    auto code = LoadBin("out/fp_ops.bin");
    auto syms = LoadSyms("out/fp_ops.syms");
    u32 fn = 0; for (auto& s : syms) if (s.name == "fp_ops") fn = s.off;
    std::memcpy(&env.mem[CODE], code.data(), code.size());
    env.Wr<u32>(SENTINEL, 0xEF000123);
    std::memcpy(&env.mem[IN], pairs.data(), pairs.size() * 4);
    A32::UserConfig conf;
    conf.callbacks = &env;
    conf.arch_version = A32::ArchVersion::v6K;
    conf.define_unpredictable_behaviour = true;
    ExclusiveMonitor mon{1};
    conf.global_monitor = &mon;
    env.Configure(conf);
    A32::Jit jit{conf};
    env.jit = &jit;

    struct Mode { u32 fpscr; const char* name; };
    const Mode modes[] = {{0x03C00000, "RZ+FZ+DN (3DS default)"}, {0x03000000, "RN+FZ+DN"}, {0x00C00000, "RZ"}, {0x00000000, "RN"}, {0x01000000, "RN+FZ"},
                          {0x02000000, "RN+DN"}, {0x01C00000, "RZ+FZ"}, {0x02C00000, "RZ+DN"}, {0x00400000, "RP (helper path)"}, {0x00800000, "RM (helper path)"}};
    static const char* opname[] = {"add", "sub", "mul", "div", "sqrt", "vcmp"};
    int bad_total = 0;
    for (const auto& m : modes) {
        jit.Reset();
        auto& r = jit.Regs(); r.fill(0);
        r[0] = n; r[1] = IN; r[2] = OUT; r[13] = STACK; r[14] = SENTINEL; r[15] = CODE + fn;
        jit.SetCpsr(0x10); jit.SetFpscr(m.fpscr);
        env.failed = false; env.used = 0; env.budget = ~u64(0) >> 2;
        HaltReason hr{};
        while (!(static_cast<u32>(hr) & static_cast<u32>(HaltReason::UserDefined1)) && !env.failed) hr = jit.Run();
        jit.ClearHalt(HaltReason::UserDefined1);
        if (env.failed) { std::printf("FAIL %-24s JIT error: %s\n", m.name, env.why.c_str()); bad_total++; continue; }

        const Dynarmic::FP::FPCR cr{m.fpscr};
        long mism[6] = {0}; std::string first[6];
        for (u32 i = 0; i < n; i++) {
            const u32 a = pairs[2 * i], b = pairs[2 * i + 1];
            Dynarmic::FP::FPSR sr{0};
            const u32 exp[6] = {S::Add<u32>(a, b, cr, sr), S::Sub<u32>(a, b, cr, sr), S::Mul<u32>(a, b, cr, sr), S::Div<u32>(a, b, cr, sr), S::Sqrt<u32>(a, cr, sr),
                                S::Compare<u32>(a, b, false, cr, sr)};
            for (int k = 0; k < 6; k++) {
                const u32 got = env.Rd<u32>(OUT + 4 * (6 * i + k));
                if (got != exp[k]) {
                    mism[k]++;
                    if (first[k].empty()) { char buf[160]; std::snprintf(buf, sizeof buf, "a=%08x b=%08x jit=%08x ref=%08x", a, b, got, exp[k]); first[k] = buf; }
                }
            }
        }
        long tot = 0; for (int k = 0; k < 6; k++) tot += mism[k];
        if (tot == 0) std::printf("PASS %-24s %u pairs x 6 ops\n", m.name, n);
        else {
            bad_total++;
            for (int k = 0; k < 6; k++) if (mism[k]) std::printf("FAIL %-24s %-5s %ld/%u differ; first: %s\n", m.name, opname[k], mism[k], n, first[k].c_str());
        }
    }
    std::printf("\n%d mode(s) failed\n", bad_total);
    return bad_total ? 1 : 0;
}
