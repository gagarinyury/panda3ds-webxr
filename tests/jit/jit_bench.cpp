// SPDX-License-Identifier: GPL-3.0-or-later
// Throughput benchmark for the WASM JIT: guest MIPS per kernel, result checked against the same C compiled to wasm32.
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "env.h"

extern "C" {
unsigned b_hash(unsigned, unsigned, unsigned*);
unsigned b_memsum(unsigned, unsigned, unsigned*);
unsigned b_calls(unsigned, unsigned, unsigned*);
unsigned b_fdot(unsigned, unsigned, unsigned*);
}
struct K { const char* name; unsigned (*ref)(unsigned, unsigned, unsigned*); u32 n; u32 b; };

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    Env env;
    env.mem.resize(0x300000);
    auto code = LoadBin("out/bench.bin");
    auto syms = LoadSyms("out/bench.syms");
    constexpr u32 CODE = 0x10000, SENTINEL = 0x30000, DATA = 0x100000, STACK = 0x2f0000;
    std::memcpy(&env.mem[CODE], code.data(), code.size());
    env.Wr<u32>(SENTINEL, 0xEF000123);
    A32::UserConfig conf;
    conf.callbacks = &env;
    conf.arch_version = A32::ArchVersion::v6K;
    conf.define_unpredictable_behaviour = true;
    ExclusiveMonitor mon{1};
    conf.global_monitor = &mon;
    env.Configure(conf);
    A32::Jit jit{conf};
    env.jit = &jit;

    const K kernels[] = {{"b_hash", b_hash, 15000000, 12345}, {"b_memsum", b_memsum, 1000, 0}, {"b_calls", b_calls, 30, 0}, {"b_fdot", b_fdot, 30000, 0}};
    int bad = 0;
    for (const auto& k : kernels) {
        if (only && std::string(only) != k.name) continue;
        u32 fn = 0; for (auto& s : syms) if (s.name == k.name) fn = s.off;
        std::vector<u32> refbuf(8192, 0);
        const u32 expect = k.ref(k.n, k.b, refbuf.data());
        double best = 1e30; u64 instr = 0; u32 got = 0;
        for (int rep = 0; rep < 3; rep++) {
            jit.Reset();
            auto& r = jit.Regs(); r.fill(0);
            r[0] = k.n; r[1] = k.b; r[2] = DATA; r[13] = STACK; r[14] = SENTINEL; r[15] = CODE + fn;
            jit.SetCpsr(0x10); jit.SetFpscr(0);
            env.failed = false; env.used = 0; env.budget = ~u64(0) >> 2;
            const auto t0 = std::chrono::steady_clock::now();
            HaltReason hr{};
            while (!(static_cast<u32>(hr) & static_cast<u32>(HaltReason::UserDefined1)) && !env.failed) hr = jit.Run();
            const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            jit.ClearHalt(HaltReason::UserDefined1);
            if (s < best) best = s;
            instr = env.used; got = r[0];
        }
        const bool ok = got == expect && !env.failed;
        if (!ok) bad++;
        std::printf("%-9s %10.1f M instr  %7.3f s  %8.1f MIPS  %s%s\n", k.name, instr / 1e6, best, instr / best / 1e6, ok ? "ok" : "WRONG RESULT ", env.failed ? env.why.c_str() : "");
    }
    return bad;
}
