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
#include "screen_layout.hpp"
#include "services/hid.hpp"
#include "opengl.hpp"

static std::unique_ptr<Emulator> emu;
static std::vector<u8> screens[3];  // [0] top-left 400x240, [1] top-right 400x240, [2] bottom 320x240, RGBA8

// Converts the screen the guest last presented into RGBA (framebuffers are stored rotated: column major, y flipped).
// screenIndex: 0 = Top Left, 1 = Top Right, 2 = Bottom
static int convertScreen(int screenIndex) {
    using namespace PICA::ExternalRegs;
    if (screenIndex < 0 || screenIndex > 2) return 0;
    auto& out = screens[screenIndex];
    const bool isTop = (screenIndex != 2);
    const u32 w = isTop ? 400 : 320, h = 240;
    out.resize(w * h * 4);
    GPU& gpu = emu->getGPU();
    auto& r = gpu.getExtRegisters();

    u32 addr = 0;
    u32 fmt = 0;
    if (screenIndex == 0) {
        const u32 sel = r[Framebuffer0Select] & 1;
        addr = r[sel ? Framebuffer0ASecondAddr : Framebuffer0AFirstAddr];
        fmt = r[Framebuffer0Config] & 7;
    } else if (screenIndex == 1) {
        const u32 sel = r[Framebuffer0Select] & 1;
        addr = r[sel ? Framebuffer0BSecondAddr : Framebuffer0BFirstAddr];
        fmt = r[Framebuffer0Config] & 7;
    } else {
        const u32 sel = r[Framebuffer1Select] & 1;
        addr = r[sel ? Framebuffer1ASecondAddr : Framebuffer1AFirstAddr];
        fmt = r[Framebuffer1Config] & 7;
    }

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
        a.preserveDrawingBuffer = std::getenv("GL_PRESERVE") != nullptr;   // true: lets the page read the picture back after the frame (tests, screenshots)
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
// Screen layout (call after the canvas has its final pixel size): layout 0 = one above the other, 2 = side by side; topPct = share of the window given to the top screen.
// Sets the output size; the rectangle of the bottom screen inside the window (0 = x, 1 = y, 2 = width, 3 = height) is read back with web_layout_rect, so the page can map touches.
static int layoutRect[4] = {0, 0, 320, 240};
EMSCRIPTEN_KEEPALIVE void web_layout(int w, int h, int layout, float topPct) {
    auto& cfg = emu->getConfig();
    cfg.screenLayout = static_cast<ScreenLayout::Layout>(layout);
    cfg.topScreenSize = topPct;
    emu->setOutputSize(static_cast<u32>(w), static_cast<u32>(h));
    ScreenLayout::WindowCoordinates c;
    ScreenLayout::calculateCoordinates(c, static_cast<u32>(w), static_cast<u32>(h), topPct, cfg.screenLayout);
    layoutRect[0] = static_cast<int>(c.bottomScreenX);
    layoutRect[1] = static_cast<int>(c.bottomScreenY);
    layoutRect[2] = static_cast<int>(c.bottomScreenWidth);
    layoutRect[3] = static_cast<int>(c.bottomScreenHeight);
}
EMSCRIPTEN_KEEPALIVE int web_layout_rect(int i) { return layoutRect[i & 3]; }

EMSCRIPTEN_KEEPALIVE const u8* web_screen(int top) { return convertScreen(top ? 0 : 2) ? screens[top ? 0 : 2].data() : nullptr; }
EMSCRIPTEN_KEEPALIVE const u8* web_screen_stereo(int eye) { return convertScreen(eye) ? screens[eye].data() : nullptr; }
EMSCRIPTEN_KEEPALIVE void web_set_3d_slider(float val) { if (emu) emu->getConfig().slider3D = val; }
EMSCRIPTEN_KEEPALIVE float web_get_3d_slider() { return emu ? emu->getConfig().slider3D : 0.0f; }
}

int main() { return 0; }
