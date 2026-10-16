// SPDX-License-Identifier: GPL-3.0-or-later
// Differential test: the same kernels run (a) as ARMv6/Thumb code through Dynarmic's WASM backend and (b) as native wasm32 code.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include "dynarmic/interface/exclusive_monitor.h"
#include "env.h"
#include "tests.h"
#include "tests_fp.h"

extern "C" {
#define X(n) unsigned n(unsigned, unsigned, unsigned*);
TEST_LIST(X)
TEST_LIST_FP(X)
#undef X
}

using namespace Dynarmic;
using u32 = std::uint32_t;
using u8 = std::uint8_t;
using u64 = std::uint64_t;

struct Ref { const char* name; unsigned (*fn)(unsigned, unsigned, unsigned*); };
static const Ref refs[] = {
#define X(n) {#n, n},
    TEST_LIST(X)
#undef X
};
static const Ref refs_fp[] = {
#define X(n) {#n, n},
    TEST_LIST_FP(X)
#undef X
};

static u32 rng = 0x12345678;
static u32 Rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    int total_fail = 0, total_pass = 0;
    for (const char* mode : {"arm", "thumb", "fp"}) {
        const bool thumb = std::string(mode) == "thumb";
        const bool fpgroup = std::string(mode) == "fp";
        Env env;
        std::string base = std::string("out/tests_") + mode;
        auto code = LoadBin((base + ".bin").c_str());
        auto syms = LoadSyms((base + ".syms").c_str());
        const u32 code_base = fpgroup ? CODE_FP : (thumb ? CODE_THUMB : CODE_ARM);
        std::memcpy(&env.mem[code_base], code.data(), code.size());
        env.Wr<u32>(SENTINEL, 0xEF000123);  // svc 0x123

        A32::UserConfig conf;
        conf.callbacks = &env;
        env.Configure(conf);
        conf.arch_version = A32::ArchVersion::v6K;
        conf.define_unpredictable_behaviour = true;
        ExclusiveMonitor mon{1};
        conf.global_monitor = &mon;
        A32::Jit jit{conf};
        env.jit = &jit;

        std::vector<std::pair<u32, u32>> inputs = {{0, 0}, {1, 1}, {0xffffffff, 1}, {0x80000000, 0x7fffffff}, {0x7fffffff, 0x80000000}, {5, 3}, {3, 5}, {0xffffffff, 0xffffffff},
                                                    {100, 0x40}, {0x12345678, 31}, {0xdeadbeef, 0x1234}, {1000, 7}};
        for (int i = 0; i < 20; i++) inputs.push_back({Rnd(), Rnd()});

        for (const auto& r : (fpgroup ? std::vector<Ref>(std::begin(refs_fp), std::end(refs_fp)) : std::vector<Ref>(std::begin(refs), std::end(refs)))) {
            if (only && std::string(only) != r.name) continue;
            u32 fn = 0; for (auto& s : syms) if (s.name == r.name) fn = s.off;
            int fails = 0; std::string first;
            for (auto [a, b] : inputs) {
                // identical scratch contents for both runs
                u32 seed = a * 2654435761u ^ b; std::vector<u32> ref_buf(256), jit_buf(256);
                for (int i = 0; i < 256; i++) { seed = seed * 1664525u + 1013904223u; ref_buf[i] = jit_buf[i] = seed; }
                const u32 expect = r.fn(a, b, ref_buf.data());

                std::memcpy(&env.mem[BUF], jit_buf.data(), 1024);
                jit.Reset();
                auto& regs = jit.Regs(); regs.fill(0);
                regs[0] = a; regs[1] = b; regs[2] = BUF; regs[13] = STACK_TOP; regs[14] = SENTINEL; regs[15] = code_base + (fn & ~1u);
                jit.SetCpsr(0x10 | (thumb ? 0x20 : 0));
                jit.SetFpscr(0);
                env.failed = false; env.why.clear(); env.used = 0; env.budget = 400000000;
                HaltReason hr{};
                for (int guard = 0; guard < 4 && !(static_cast<u32>(hr) & static_cast<u32>(HaltReason::UserDefined1)) && !env.failed; guard++) hr = jit.Run();
                jit.ClearHalt(HaltReason::UserDefined1);
                const u32 got = regs[0];
                std::vector<u32> after(256); std::memcpy(after.data(), &env.mem[BUF], 1024);
                bool bad = env.failed || got != expect || after != ref_buf;
                if (static_cast<u32>(hr) & static_cast<u32>(HaltReason::UserDefined8)) bad = true;
                if (bad) {
                    fails++;
                    if (first.empty()) {
                        char buf[256];
                        std::snprintf(buf, sizeof buf, "a=%08x b=%08x jit=%08x ref=%08x%s%s%s", a, b, got, expect, after != ref_buf ? " [memory differs]" : "", env.failed ? " " : "", env.why.c_str());
                        first = buf;
                    }
                }
            }
            if (fails == 0) { total_pass++; std::printf("PASS %-8s %-9s (%zu cases)\n", mode, r.name, inputs.size()); }
            else { total_fail++; std::printf("FAIL %-8s %-9s %d/%zu cases differ; first: %s\n", mode, r.name, fails, inputs.size(), first.c_str()); }
        }
    }
    std::printf("\n%d passed, %d failed\n", total_pass, total_fail);
    return total_fail ? 1 : 0;
}
