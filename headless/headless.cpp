// SPDX-License-Identifier: GPL-3.0-or-later
// Headless driver for the Panda3DS core under Node: loads a ROM/homebrew, runs N frames, prints what happened.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>

#include "emulator.hpp"

static int run(const char* path, int frames) {
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
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: AlberHeadless <rom.3dsx|elf|3ds> [frames]\n");
        return 2;
    }
    const int frames = argc > 2 ? std::atoi(argv[2]) : 60;
    try {
        return run(argv[1], frames);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[headless] C++ exception: %s\n", e.what());
        return 3;
    }
}
