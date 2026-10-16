// SPDX-License-Identifier: GPL-3.0-or-later
// Shared test environment: guest memory + callbacks for Dynarmic's A32 interface.
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"
#include "dynarmic/interface/exclusive_monitor.h"

using u32 = std::uint32_t;
using u8 = std::uint8_t;
using u64 = std::uint64_t;
using namespace Dynarmic;

constexpr u32 CODE_ARM = 0x10000, CODE_THUMB = 0x20000, CODE_FP = 0x28000, SENTINEL = 0x30000, BUF = 0x40000, STACK_TOP = 0x100000;

struct Env final : Dynarmic::A32::UserCallbacks {
    std::vector<u8> mem = std::vector<u8>(0x110000);
    A32::Jit* jit = nullptr;
    std::vector<u32> read_table, write_table;  // one host pointer per 4 KiB page, like Panda3DS's readTable/writeTable
    void Configure(A32::UserConfig& conf) {
        read_table.assign(1u << 20, 0); write_table.assign(1u << 20, 0);
        for (u32 p = 0; (p << 12) + 4096 <= mem.size(); p++) read_table[p] = write_table[p] = static_cast<u32>(reinterpret_cast<uintptr_t>(&mem[p << 12]));
        conf.wasm_read_table = read_table.data();
        conf.wasm_write_table = write_table.data();
    }
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

