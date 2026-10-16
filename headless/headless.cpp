// SPDX-License-Identifier: GPL-3.0-or-later
// Headless driver for the Panda3DS core under Node: loads a ROM/homebrew, runs N frames, prints what happened.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <cstring>
#include <fstream>
#include <vector>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include "emulator.hpp"

// Writes the screen the guest last presented as a binary PPM. top: 400x240, bottom: 320x240; framebuffers are stored rotated
// (column major, y flipped). Returns false if the screen has no valid buffer yet.
static bool DumpScreen(Emulator& emu, bool top, const char* path) {
    using namespace PICA::ExternalRegs;
    GPU& gpu = emu.getGPU();
    auto& r = gpu.getExtRegisters();
    const u32 sel = r[top ? Framebuffer0Select : Framebuffer1Select] & 1;
    const u32 addr = r[top ? (sel ? Framebuffer0ASecondAddr : Framebuffer0AFirstAddr) : (sel ? Framebuffer1ASecondAddr : Framebuffer1AFirstAddr)];
    const u32 fmt = r[top ? Framebuffer0Config : Framebuffer1Config] & 7;
    static const int bpp[] = {4, 3, 2, 2, 2, 2, 2, 2};
    const u32 w = top ? 400 : 320, h = 240;
    const u8* fb = gpu.getPointerPhys<u8>(addr, w * h * bpp[fmt]);
    if (!fb) { std::printf("[headless] %s screen: no framebuffer (addr=%08x)\n", top ? "top" : "bottom", addr); return false; }
    std::vector<u8> rgb(w * h * 3);
    for (u32 x = 0; x < w; x++) for (u32 y = 0; y < h; y++) {
        const u8* p = fb + (x * h + (h - 1 - y)) * bpp[fmt];
        u8 R = 0, G = 0, B = 0;
        switch (fmt) {
        case 0: R = p[3]; G = p[2]; B = p[1]; break;                       // RGBA8 (stored A,B,G,R)
        case 1: B = p[0]; G = p[1]; R = p[2]; break;                       // BGR8
        case 2: { u16 v = p[0] | p[1] << 8; R = (v >> 11) * 255 / 31; G = ((v >> 5) & 63) * 255 / 63; B = (v & 31) * 255 / 31; break; }  // RGB565
        default: break;
        }
        u8* o = &rgb[(y * w + x) * 3]; o[0] = R; o[1] = G; o[2] = B;
    }
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    f.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
    long lit = 0; for (size_t i = 0; i < rgb.size(); i += 3) lit += (rgb[i] | rgb[i + 1] | rgb[i + 2]) != 0;
    std::printf("[headless] %s screen: addr=%08x format=%u -> %s (%ld non-black pixels)\n", top ? "top" : "bottom", addr, fmt, path, lit);
    return true;
}

static int run(const char* path, int frames, const char* shot) {
    Emulator emu;
    emu.initGraphicsContext(nullptr);
    if (!emu.loadROM(path)) {
        std::fprintf(stderr, "[headless] failed to load %s\n", path);
        return 1;
    }
    emu.resume();
    std::printf("[headless] loaded %s, running %d frames\n", path, frames);
    std::fflush(stdout);

    const auto t0 = std::chrono::steady_clock::now();
    int done = 0;
    for (; done < frames; done++) {
        emu.runFrame();
        if (!emu.running) {
            std::printf("[headless] emulator stopped after %d frames\n", done + 1);
            break;
        }
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[headless] done: %d frames in %.1f ms (%.2f ms/frame)\n", done, ms, ms / (done ? done : 1));
    if (shot) { std::string base(shot); DumpScreen(emu, true, (base + "-top.ppm").c_str()); DumpScreen(emu, false, (base + "-bottom.ppm").c_str()); }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: AlberHeadless <rom.3dsx|elf|3ds> [frames]\n");
        return 2;
    }
    const int frames = argc > 2 ? std::atoi(argv[2]) : 60;
    const char* shot = argc > 3 ? argv[3] : nullptr;
    try {
        return run(argv[1], frames, shot);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[headless] C++ exception: %s\n", e.what());
        return 3;
    }
}
