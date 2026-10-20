// Test library for the screen mode lookup, palette fade and OS_dd_error (unit v5945; libtpw_unit_LbScreen_IsModeAvailable_2.dylib,
// tests/test_LbScreen_IsModeAvailable_2.py), bullfrog:
//   src/LbSurface_LbScreen_IsModeAvailable_2.cpp
//
// The callees outside the unit are recording fakes answering from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbSurface.h"
#include "WinScrn.h"
#include "LbSCBack.h"
#include <string>

using namespace calllog;

int32_t gWinScrnInitialised;

namespace {

uint32_t Checksum(const uint8_t* data) {
    uint32_t h = 0;
    for (int i = 0; i < 256; i++)
        for (int k = 0; k < 3; k++) h = h * 31 + data[i * 4 + k];
    return h;
}

TbResult ModeFake(const char* name, const char* key, TbScreenMode* mode) {
    ev(name, {0});
    TbResult r;
    r.mCode = static_cast<int32_t>(ret((std::string(key) + "_res").c_str()));
    mode->mWidth = static_cast<int32_t>(ret((std::string(key) + "_w").c_str()));
    mode->mHeight = static_cast<int32_t>(ret((std::string(key) + "_h").c_str()));
    mode->mDepth = static_cast<int32_t>(ret((std::string(key) + "_d").c_str()));
    return r;
}

}  // namespace

TbResult LbScreen_ModeFindFirst(TbScreenModeFind*, TbScreenMode* mode) { return ModeFake("FindFirst", "ff", mode); }
TbResult LbScreen_ModeFindNext(TbScreenModeFind*, TbScreenMode* mode) { return ModeFake("FindNext", "fn", mode); }
void LbScreen_ModeFindEnd(TbScreenModeFind*) { ev("FindEnd", {0}); }
void LbScreen_WaitVbi() { ev("Vbi", {0}); }

TbPaletteEntry* LbScreen_GetPaletteEntries(TbPaletteEntry* entries, uint32_t start, uint32_t count) {
    ev("Get", {static_cast<int64_t>(start), static_cast<int64_t>(count)});
    uint32_t seed = static_cast<uint32_t>(ret("get")) & 0xff;
    uint8_t* p = reinterpret_cast<uint8_t*>(entries);
    for (int i = 0; i < 256; i++)
        for (int k = 0; k < 4; k++) p[i * 4 + k] = static_cast<uint8_t>(seed + i * 7 + k * 13 + (i >> 3) * 5);
    return entries;
}

void LbScreen_SetPaletteEntries(const TbPaletteEntry* entries, uint32_t start, uint32_t count) {
    ev("Set", {static_cast<int64_t>(start), static_cast<int64_t>(count),
               static_cast<int64_t>(Checksum(reinterpret_cast<const uint8_t*>(entries)))});
}

extern "C" {

uint64_t tpw_fs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"init", &gWinScrnInitialised}, {"fa", &gLbScreenModeFlagA}, {"fb", &gLbScreenModeFlagB},
        {"active", &gLbFadeActive},     {"step", &gLbFadeStep},      {"saved", gLbFadeSaved},
        {"zero", gLbFadeZero},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbResult));
    out[n++] = static_cast<int64_t>(sizeof(TbScreenMode));
    return n;
}

uint32_t tpw_fs_mode(uint32_t w, uint32_t h, uint32_t d) { return LbScreen_IsModeAvailable(w, h, d); }
uint32_t tpw_fs_fade(uint64_t pal, uint32_t n, int32_t type) {
    return LbScreen_FadePalette(reinterpret_cast<TbPalette*>(static_cast<uintptr_t>(pal)), n, static_cast<TbPaletteFadeType>(type));
}
void tpw_fs_dd(int32_t code) { OS_dd_error(code); }

}  // extern "C"
