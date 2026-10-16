// SPDX-License-Identifier: GPL-3.0-or-later
// Decodes raw ARMv6 code with Dynarmic's A32 front end (compiled to WASM) and prints the IR before/after optimisation.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "dynarmic/frontend/A32/PSR.h"
#include "dynarmic/frontend/A32/FPSCR.h"
#include "dynarmic/frontend/A32/a32_location_descriptor.h"
#include "dynarmic/frontend/A32/translate/a32_translate.h"
#include "dynarmic/frontend/A32/translate/translate_callbacks.h"
#include "dynarmic/ir/basic_block.h"
#include "dynarmic/ir/opt/passes.h"

using namespace Dynarmic;

struct Callbacks final : A32::TranslateCallbacks {
    std::vector<uint8_t> mem;
    std::optional<uint32_t> MemoryReadCode(A32::VAddr a) override {
        if (a + 4 > mem.size()) return std::nullopt;
        return uint32_t(mem[a]) | uint32_t(mem[a + 1]) << 8 | uint32_t(mem[a + 2]) << 16 | uint32_t(mem[a + 3]) << 24;
    }
    bool PreCodeReadHook(bool, A32::VAddr, A32::IREmitter&) override { return true; }
    void PreCodeTranslationHook(bool, A32::VAddr, A32::IREmitter&) override {}
    uint64_t GetTicksForCode(bool, A32::VAddr, uint32_t) override { return 1; }
};

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: ir_dump code.bin addr...\n"); return 1; }
    Callbacks cb;
    std::ifstream f(argv[1], std::ios::binary);
    cb.mem.assign(std::istreambuf_iterator<char>(f), {});
    std::printf("loaded %zu bytes\n", cb.mem.size());
    for (int i = 2; i < argc; i++) {
        uint32_t addr = std::strtoul(argv[i], nullptr, 0);
        A32::LocationDescriptor loc{addr, A32::PSR{0x10}, A32::FPSCR{}};
        A32::TranslationOptions opt{A32::ArchVersion::v6K, true, true};
        IR::Block b = A32::Translate(loc, &cb, opt);
        std::printf("\n===== block @0x%x : raw IR (%zu instructions) =====\n%s\n", addr, b.size(), IR::DumpBlock(b).c_str());
        Optimization::A32GetSetElimination(b, {});
        Optimization::DeadCodeElimination(b);
        Optimization::ConstantPropagation(b);
        Optimization::DeadCodeElimination(b);
        Optimization::IdentityRemovalPass(b);
        Optimization::VerificationPass(b);
        std::printf("===== block @0x%x : optimised IR (%zu instructions) =====\n%s\n", addr, b.size(), IR::DumpBlock(b).c_str());
    }
    return 0;
}
