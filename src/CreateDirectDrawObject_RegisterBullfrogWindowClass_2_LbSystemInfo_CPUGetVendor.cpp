// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).
#include "WinScrn.h"
#include "TbFPUPrecision.h"
#include "bfl_Refpack.h"
#include "platform/macdoze.h"
#include "LbSurface.h"
#include "LbSCBack.h"
#include "DrawState.h"
#include "AudioPlaceHolder.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

// ---- TbSystemInfo.cpp
void LbSystemInfo_CPUGetVendor(char* out) {   // 0x10043bcc: 13 bytes, copied from the last to the first
    for (int32_t i = 12; i >= 0; --i) out[i] = gTbSystemInfoCPU.mName[i];
}

uint32_t LbSystemInfo_CPUGetFamily() {   // 0x10043b7c
    const uint32_t type = static_cast<uint32_t>(gTbSystemInfoCPU.mType);
    if (type == 0) return 4;
    return (type >> 8) & 0xf;
}

uint32_t LbSystemInfo_CPUhasMMX() { return (gTbSystemInfoCPU.mFeatures >> 23) & 1; }   // 0x10043c28

int32_t LbSystemInfo_CPUhasRDTSC() { return gTbSystemInfoCPU.mHasRDTSC; }   // 0x10043c68

int32_t LbSystemInfo_CPUGetTickRate(uint32_t unused) {   // 0x10043ca4
    return gTbSystemInfoCPU.clocks_per_millisec(static_cast<int32_t>(unused));
}

// ---- Draw.cpp
void LbTime_Delay(uint32_t ms) { NS_MacDoze::Sleep(ms); }   // 0x10046a54

// ---- LbDraw.cpp
int32_t LbSize_Area(const TbSize* size) { return size->width * size->height; }   // 0x10029f50

void LbSize_Set(TbSize* size, uint32_t width, uint32_t height) {   // 0x10029f18
    size->width = static_cast<int32_t>(width);
    size->height = static_cast<int32_t>(height);
}

bool LbSize_IsNull(const TbSize* size) {   // 0x10029ec4
    return size->width == 0 || size->height == 0;
}

bool LbRect_Intersects(const TbRect* a, const TbRect* b) {   // 0x10029e18
    bool x = false;
    bool y = false;
    bool result = false;
    if (a->left <= b->right && b->left <= a->right) x = true;
    if (x && a->top <= b->bottom) y = true;
    if (y && b->top <= a->bottom) result = true;
    return result;
}

bool LbRect_IsNormal(const TbRect* rect) {   // 0x10029db8
    return rect->left <= rect->right && rect->top <= rect->bottom;
}

bool LbRect_IsEmpty(const TbRect* rect) {   // 0x10029c74
    return rect->left == rect->right || rect->top == rect->bottom;
}

void LbRect_Offset(TbRect* rect, int32_t dx, int32_t dy) {   // 0x10029928
    rect->left += dx;
    rect->top += dy;
    rect->right += dx;
    rect->bottom += dy;
}

TbRect* LbRect_Copy(TbRect* dest, const TbRect* source) {   // 0x100298d0 (r3 still holds dest)
    dest->left = source->left;
    dest->top = source->top;
    dest->right = source->right;
    dest->bottom = source->bottom;
    return dest;
}

void LbRect_SetWindow(TbRect* rect, int32_t x, int32_t y, int32_t width, int32_t height) {   // 0x10029880
    rect->left = x;
    rect->top = y;
    rect->right = x + width;
    rect->bottom = y + height;
}

void LbRect_SetFromSize(TbRect* rect, const TbSize* size) {   // 0x10029824
    rect->left = 0;
    rect->top = 0;
    rect->right = size->width;
    rect->bottom = size->height;
}

void LbRect_Set(TbRect* rect, int32_t left, int32_t top, int32_t right, int32_t bottom) {   // 0x100297e4
    rect->left = left;
    rect->top = top;
    rect->right = right;
    rect->bottom = bottom;
}

void LbPoint_Offset(TbPoint* point, int32_t dx, int32_t dy) {   // 0x10029798
    point->x += dx;
    point->y += dy;
}

void LbPoint_Set(TbPoint* point, int32_t x, int32_t y) {   // 0x10029760
    point->x = x;
    point->y = y;
}

// ---- static initialisers
void __sinit_LbMaths_cpp() { gLbMathsAtanScale = 1024.0 / gMathLibPi; }   // 0x1002971c

void __sinit_TbJPEGSaver_cpp() {   // 0x100249c0
    gTbJPEGSaverMasks[0] = 0xff;
    gTbJPEGSaverMasks[1] = 0xff00;
    gTbJPEGSaverMasks[2] = 0xff0000;
    gTbJPEGSaverMasks[3] = 0;
    gTbJPEGSaverMasks[4] = 0x18;
    gTbJPEGSaverMasks[5] = 0;
}

void __sinit_Draw_cpp() {   // 0x100661a8
    bfl::draw::gDrawContext8.mVtable = kDrawContextVtable8;
    bfl::draw::gDrawContext16.mVtable = kDrawContextVtable16;
    bfl::draw::gDrawContext24.mVtable = kDrawContextVtable24;
    bfl::draw::gDrawContext32.mVtable = kDrawContextVtable32;
}

// ---- WinScrn.cpp
void LbScreen_StopOpenFadePalette() { gLbFadeActive = 0; }   // 0x10068710

void LbScreen_SetDirectDrawGuid(NS_MacDoze::_GUID* guid) { gWinScrnGuid = guid; }   // 0x10068754

int32_t LbScreen_IsInitialised() { return gWinScrnInitialised; }   // 0x10068c24

int32_t LbScreen_IsActive() { return gWinScrnActive; }   // 0x10068e20

void LbScreen_ModeFindEnd(TbScreenModeFind*) {}   // 0x100690bc

void LbScreen_WaitVbi() { IDirectDraw_WaitForVerticalBlank(gWinScrnDirectDraw, 1, nullptr); }   // 0x1006a400

// ---- LbSCBack.cpp
const char* LbApp_GetWindowName() { return gLbWindowName; }   // 0x10014340

int16_t LbApp_GetWindowIcon() { return gLbWindowIcon; }   // 0x10014308
