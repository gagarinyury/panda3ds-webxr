// SPDX-License-Identifier: GPL-3.0-or-later
// Browser driver for the Panda3DS core: JS calls web_init / web_load / web_frame from requestAnimationFrame and reads the two screens as RGBA.
#include <emscripten.h>

#include <cstdio>
#include <memory>
#include <vector>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include "emulator.hpp"

static std::unique_ptr<Emulator> emu;
static std::vector<u8> screens[2];  // [0] top 400x240, [1] bottom 320x240, RGBA8

// Converts the screen the guest last presented into RGBA (framebuffers are stored rotated: column major, y flipped).
static int convertScreen(bool top) {
    using namespace PICA::ExternalRegs;
    auto& out = screens[top ? 0 : 1];
    const u32 w = top ? 400 : 320, h = 240;
    out.resize(w * h * 4);
    GPU& gpu = emu->getGPU();
    auto& r = gpu.getExtRegisters();
    const u32 sel = r[top ? Framebuffer0Select : Framebuffer1Select] & 1;
    const u32 addr = r[top ? (sel ? Framebuffer0ASecondAddr : Framebuffer0AFirstAddr) : (sel ? Framebuffer1ASecondAddr : Framebuffer1AFirstAddr)];
    const u32 fmt = r[top ? Framebuffer0Config : Framebuffer1Config] & 7;
    static const int bpp[] = {4, 3, 2, 2, 2, 2, 2, 2};
    const u8* fb = gpu.getPointerPhys<u8>(addr, w * h * bpp[fmt]);
    if (!fb) { std::fill(out.begin(), out.end(), 0); return 0; }
    for (u32 x = 0; x < w; x++) {
        for (u32 y = 0; y < h; y++) {
            const u8* p = fb + (x * h + (h - 1 - y)) * bpp[fmt];
            u8 R = 0, G = 0, B = 0;
            switch (fmt) {
                case 0: R = p[3]; G = p[2]; B = p[1]; break;
                case 1: B = p[0]; G = p[1]; R = p[2]; break;
                case 2: { u16 v = p[0] | p[1] << 8; R = (v >> 11) * 255 / 31; G = ((v >> 5) & 63) * 255 / 63; B = (v & 31) * 255 / 31; break; }
                default: break;
            }
            u8* o = &out[(y * w + x) * 4];
            o[0] = R; o[1] = G; o[2] = B; o[3] = 255;
        }
    }
    return 1;
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int web_init() {
    emu = std::make_unique<Emulator>();
    emu->initGraphicsContext(nullptr);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_load(const char* path) {
    if (!emu->loadROM(path)) return 0;
    emu->resume();
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_frame() {
    emu->runFrame();
    return emu->running ? 1 : 0;
}

// Fills the RGBA buffer of one screen and returns a pointer to it (or 0 if the guest has not presented anything yet)
EMSCRIPTEN_KEEPALIVE const u8* web_screen(int top) { return convertScreen(top != 0) ? screens[top ? 0 : 1].data() : nullptr; }
}

int main() { return 0; }
