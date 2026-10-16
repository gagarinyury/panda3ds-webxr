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
#include "tests.h"

extern "C" {
#define X(n) unsigned n(unsigned, unsigned, unsigned*);
TEST_LIST(X)
#undef X
}

using namespace Dynarmic;
using u32 = std::uint32_t;
using u8 = std::uint8_t;
using u64 = std::uint64_t;

constexpr u32 CODE_ARM = 0x10000, CODE_THUMB = 0x20000, SENTINEL = 0x30000, BUF = 0x40000, STACK_TOP = 0x100000;

struct Env final : Dynarmic::A32::UserCallbacks {
    std::vector<u8> mem = std::vector<u8>(0x110000);
    A32::Jit* jit = nullptr;
    u64 budget = 0, used = 0;
    bool failed = false;
    std::string why;

    template<typename T> T Rd(u32 a) {
        if (a + sizeof(T) > mem.size()) { failed = true; why = "bad read " + std::to_string(a); return 0; }
        T v; std::memcpy(&v, &mem[a], sizeof v); return v;
    }
    template<typename T> void Wr(u32 a, T v) {
        if (a + sizeof(T) > mem.size()) { failed = true; why = "bad write " + std::to_string(a); return; }
        std::memcpy(&mem[a], &v, sizeof v);
    }
    u8 MemoryRead8(u32 a) override { return Rd<u8>(a); }
    std::uint16_t MemoryRead16(u32 a) override { return Rd<std::uint16_t>(a); }
    u32 MemoryRead32(u32 a) override { return Rd<u32>(a); }
    std::uint64_t MemoryRead64(u32 a) override { return Rd<std::uint64_t>(a); }
    void MemoryWrite8(u32 a, u8 v) override { Wr(a, v); }
    void MemoryWrite16(u32 a, std::uint16_t v) override { Wr(a, v); }
    void MemoryWrite32(u32 a, u32 v) override { Wr(a, v); }
    void MemoryWrite64(u32 a, std::uint64_t v) override { Wr(a, v); }
    void InterpreterFallback(u32 pc, size_t n) override { failed = true; why = "interpreter fallback at " + std::to_string(pc); jit->HaltExecution(); }
    void CallSVC(u32 swi) override { if (swi == 0x123) jit->HaltExecution(); else { failed = true; why = "unexpected svc"; jit->HaltExecution(); } }
    void ExceptionRaised(u32 pc, A32::Exception e) override { failed = true; why = "exception at " + std::to_string(pc); jit->HaltExecution(); }
    void AddTicks(u64 t) override { used += t; }
    u64 GetTicksRemaining() override { return used >= budget ? 0 : budget - used; }
};

struct Sym { std::string name; u32 off; };
static std::vector<Sym> LoadSyms(const char* path) {
    std::vector<Sym> v; std::ifstream f(path); std::string n; std::string h;
    while (f >> n >> h) v.push_back({n, static_cast<u32>(std::strtoul(h.c_str(), nullptr, 16))});
    return v;
}
static std::vector<u8> LoadBin(const char* path) {
    std::ifstream f(path, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}};
}

struct Ref { const char* name; unsigned (*fn)(unsigned, unsigned, unsigned*); };
static const Ref refs[] = {
#define X(n) {#n, n},
    TEST_LIST(X)
#undef X
};

static u32 rng = 0x12345678;
static u32 Rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    int total_fail = 0, total_pass = 0;
    for (const char* mode : {"arm", "thumb"}) {
        const bool thumb = std::string(mode) == "thumb";
        Env env;
        std::string base = std::string("out/tests_") + mode;
        auto code = LoadBin((base + ".bin").c_str());
        auto syms = LoadSyms((base + ".syms").c_str());
        const u32 code_base = thumb ? CODE_THUMB : CODE_ARM;
        std::memcpy(&env.mem[code_base], code.data(), code.size());
        env.Wr<u32>(SENTINEL, 0xEF000123);  // svc 0x123

        A32::UserConfig conf;
        conf.callbacks = &env;
        conf.arch_version = A32::ArchVersion::v6K;
        conf.define_unpredictable_behaviour = true;
        ExclusiveMonitor mon{1};
        conf.global_monitor = &mon;
        A32::Jit jit{conf};
        env.jit = &jit;

        std::vector<std::pair<u32, u32>> inputs = {{0, 0}, {1, 1}, {0xffffffff, 1}, {0x80000000, 0x7fffffff}, {0x7fffffff, 0x80000000}, {5, 3}, {3, 5}, {0xffffffff, 0xffffffff},
                                                    {100, 0x40}, {0x12345678, 31}, {0xdeadbeef, 0x1234}, {1000, 7}};
        for (int i = 0; i < 20; i++) inputs.push_back({Rnd(), Rnd()});

        for (const auto& r : refs) {
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
