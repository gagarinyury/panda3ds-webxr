// SPDX-License-Identifier: GPL-3.0-or-later
// Browser driver for the Panda3DS core: JS calls web_init / web_load / web_frame from requestAnimationFrame and reads the two screens as RGBA.
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>

#include <cstdio>
#include <memory>
#include <vector>

#include "PICA/gpu.hpp"
#include "PICA/regs.hpp"
#include "emulator.hpp"
#include "services/hid.hpp"
#include "opengl.hpp"

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
// gl != 0: use the OpenGL ES renderer on a WebGL2 canvas (#gl) of w x h pixels; the renderer draws both screens into the canvas itself.
EMSCRIPTEN_KEEPALIVE int web_init(int gl, int w, int h) {
    emu = std::make_unique<Emulator>();
    if (gl) {
        EmscriptenWebGLContextAttributes a;
        emscripten_webgl_init_context_attributes(&a);
        a.majorVersion = 2;
        a.minorVersion = 0;
        a.alpha = false;
        a.depth = true;
        a.stencil = true;
        a.antialias = false;
        a.preserveDrawingBuffer = std::getenv("GL_NOPRESERVE") == nullptr;   // true: lets the page read the picture back after the frame (tests, screenshots); env GL_NOPRESERVE:1 saves the copy the browser makes at every present
        a.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
        a.enableExtensionsByDefault = true;
        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#gl", &a);
        if (ctx <= 0) { std::printf("[FATAL] WebGL2 context creation failed (%d)\n", static_cast<int>(ctx)); return 0; }
        emscripten_webgl_make_context_current(ctx);
        if (!gladLoadGLES2Loader(reinterpret_cast<GLADloadproc>(emscripten_webgl_get_proc_address))) { std::printf("[FATAL] glad failed to load GLES2\n"); return 0; }
        emu->getRenderer()->setupGLES();
        emu->setOutputSize(static_cast<u32>(w), static_cast<u32>(h));
    }
    emu->initGraphicsContext(nullptr);
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_load(const char* path) {
    if (!emu->loadROM(path)) return 0;
    emu->resume();
    return 1;
}

EMSCRIPTEN_KEEPALIVE int web_frame() {
    emu->getServiceManager().getHID().updateInputs(emu->getTicks());   // publish the button / touch state to the guest, once per frame like the SDL frontend
    emu->runFrame();
    return emu->running ? 1 : 0;
}

// Input: mask = HID::Keys bit(s); touch x/y are bottom-screen pixels (0..319, 0..239); circle pad x/y are -156..156
EMSCRIPTEN_KEEPALIVE void web_key(unsigned mask, int down) {
    auto& hid = emu->getServiceManager().getHID();
    if (down) hid.pressKey(mask); else hid.releaseKey(mask);
}
EMSCRIPTEN_KEEPALIVE void web_touch(int x, int y, int down) {
    auto& hid = emu->getServiceManager().getHID();
    if (down) hid.setTouchScreenPress(static_cast<u16>(x), static_cast<u16>(y)); else hid.releaseTouchScreen();
}
EMSCRIPTEN_KEEPALIVE void web_circle(int x, int y) {
    auto& hid = emu->getServiceManager().getHID();
    hid.setCirclepadX(static_cast<s16>(x));
    hid.setCirclepadY(static_cast<s16>(y));
}

// Fills the RGBA buffer of one screen and returns a pointer to it (or 0 if the guest has not presented anything yet)
EMSCRIPTEN_KEEPALIVE const u8* web_screen(int top) { return convertScreen(top != 0) ? screens[top ? 0 : 1].data() : nullptr; }
}

int main() { return 0; }
