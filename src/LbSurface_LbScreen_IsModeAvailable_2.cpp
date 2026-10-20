#include <cstring>
#include <initializer_list>
#include "LbSurface.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "platform/macdoze.h"
#include "WinScrn.h"
#include "LbSCBack.h"

int32_t gLbScreenModeFlagA;
int32_t gLbScreenModeFlagB;
int32_t gLbFadeActive;
uint32_t gLbFadeStep;
uint8_t gLbFadeSaved[0x400];
uint8_t gLbFadeZero[0x400];

// glue 0x1006c0cc: MSL __destroy_arr. Not translated yet; the destructor argument is a
// TVector descriptor whose layout is not declared. Defined empty so the launcher links.
extern "C" void __destroy_arr(void* array, void* destructor, int32_t size, int32_t count) {
    (void)array; (void)destructor; (void)size; (void)count;
}

// 0x10068298
uint32_t LbScreen_IsModeAvailable(uint32_t width, uint32_t height, uint32_t depth) {
    uint32_t found = 0;
    if (gWinScrnInitialised == 0) return 0;
    TbScreenModeFind find;
    TbScreenMode mode;
    TbResult first = LbScreen_ModeFindFirst(&find, &mode);
    if ((first.mCode >= 0) != (gLbScreenModeFlagA == 0)) return found;
    for (;;) {
        if (static_cast<uint32_t>(mode.mWidth) == width && static_cast<uint32_t>(mode.mHeight) == height &&
            static_cast<uint32_t>(mode.mDepth) == depth) {
            found = 1;
            break;
        }
        TbResult next = LbScreen_ModeFindNext(&find, &mode);
        if ((next.mCode >= 0) != (gLbScreenModeFlagB == 0)) break;
    }
    LbScreen_ModeFindEnd(&find);
    return found;
}

static void FadeStep(uint8_t* out, const uint8_t* from, const uint8_t* to, int32_t step, int32_t total) {
    for (int i = 0; i < 128; i++) {
        for (int k : {0, 1, 2, 4, 5, 6}) {
            int32_t a = from[i * 8 + k];
            int32_t b = to[i * 8 + k];
            out[i * 8 + k] = static_cast<uint8_t>(a + (step * (b - a)) / total);
        }
    }
}

// 0x100683ec
uint32_t LbScreen_FadePalette(TbPalette* palette, uint32_t count, TbPaletteFadeType type) {
    uint8_t buffer[0x400];
    uint8_t* target = reinterpret_cast<uint8_t*>(palette);
    if (target == nullptr) {
        target = gLbFadeZero;
        std::memset(gLbFadeZero, 0, 0x400);
    }
    if (type == 1) {
        LbScreen_GetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(gLbFadeSaved), 0, 0x100);
        for (gLbFadeStep = 1; gLbFadeStep < count; gLbFadeStep = gLbFadeStep + 1) {
            FadeStep(buffer, gLbFadeSaved, target, static_cast<int32_t>(gLbFadeStep), static_cast<int32_t>(count));
            LbScreen_WaitVbi();
            LbScreen_SetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(buffer), 0, 0x100);
        }
        LbScreen_SetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(target), 0, 0x100);
        gLbFadeActive = 0;
    } else {
        if (gLbFadeActive == 0) {
            gLbFadeStep = 0;
            gLbFadeActive = 1;
            LbScreen_GetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(gLbFadeSaved), 0, 0x100);
        }
        gLbFadeStep = gLbFadeStep + 1;
        if (gLbFadeStep == count) gLbFadeActive = 0;
        FadeStep(buffer, gLbFadeSaved, target, static_cast<int32_t>(gLbFadeStep), static_cast<int32_t>(count));
        LbScreen_WaitVbi();
        LbScreen_SetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(buffer), 0, 0x100);
    }
    return gLbFadeStep;
}

// 0x10067e70: every branch of the original's code comparison tree returns without effect.
void OS_dd_error(int32_t) {}
