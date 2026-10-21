// TbSystemInfo.cpp: name proven by __sinit_TbSystemInfo_cpp in the Bullfrog library.

#include "AudioPlaceHolder.h"
#include "bfl_Refpack.h"
#include "DrawState.h"
#include "LbDraw.h"
#include "LbDrawPrimitives.h"
#include "LbDrawText.h"
#include "LbPalette.h"
#include "LbRect.h"
#include "LbSCBack.h"
#include "LbSurface.h"
#include "platform/macdoze.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"
#include "TbFPUPrecision.h"
#include "WinScrn.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Unit w242: remaining legacy LbDraw dispatch and sprite helpers.

void LbDraw_ScaledSprite(int32_t x, int32_t y, const TbSprite* sprite,
                         uint32_t width, uint32_t height) {
    LbDraw_SetSpriteScalingData(x, y, sprite->mWidth, sprite->mHeight, width, height);
}

void LbDraw_SetSpriteScalingData(int32_t, int32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t) {
    gLbSpriteDrawState[0] = _lbpCurrentRA->mStride;
    gLbSpriteDrawState[1] = LbRect_Width(&_lbCurrentClipRect);
    gLbSpriteDrawState[2] = LbRect_Height(&_lbCurrentClipRect);
    gLbSpriteDrawState[3] = static_cast<int32_t>(_lbCurrentDrawFlags);
    const uintptr_t origin = reinterpret_cast<uintptr_t>(_lbCurrentOriginPtr) +
        (static_cast<uint32_t>(_lbCurrentClipRect.left) *
         static_cast<uint32_t>(_lbpCurrentRA->mDepth) >> 3) +
        static_cast<uint32_t>(_lbCurrentClipRect.top) *
        static_cast<uint32_t>(_lbpCurrentRA->mStride);
    gLbSpriteDrawState[4] = static_cast<int32_t>(origin);
    if (_lbCurrentDrawFlags & 0x18)
        gLbSpriteDrawState[5] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbCurrentGhostTable));
    if (_lbCurrentDrawFlags & 0x20) {
        gLbSpriteDrawState[6] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbpCurrentFadeTable));
        gLbSpriteDrawState[7] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbpCurrentFadeRow));
    }
}

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

uint32_t LbSystemInfo_CPUGetFamily() {   // 0x10043b7c
    const uint32_t type = static_cast<uint32_t>(gTbSystemInfoCPU.mType);
    if (type == 0) return 4;
    return (type >> 8) & 0xf;
}

// ---- TbSystemInfo.cpp
void LbSystemInfo_CPUGetVendor(char* out) {   // 0x10043bcc: 13 bytes, copied from the last to the first
    for (int32_t i = 12; i >= 0; --i) out[i] = gTbSystemInfoCPU.mName[i];
}

uint32_t LbSystemInfo_CPUhasMMX() { return (gTbSystemInfoCPU.mFeatures >> 23) & 1; }   // 0x10043c28

int32_t LbSystemInfo_CPUhasRDTSC() { return gTbSystemInfoCPU.mHasRDTSC; }   // 0x10043c68

int32_t LbSystemInfo_CPUGetTickRate(uint32_t unused) {   // 0x10043ca4
    return gTbSystemInfoCPU.clocks_per_millisec(static_cast<int32_t>(unused));
}

// 0x10073a90 (the Mac's TbFPUFlag vtable); the contents are never read by the host.
const void* const kScreenFPUHandlerFlagVtable[2] = {nullptr, nullptr};

__attribute__((weak)) void ScreenFPUHandler_OperatorDelete(void* block) { ::operator delete(block); }

_ScreenFPUHandler* ScreenFPUHandler_Dtor(_ScreenFPUHandler* self, int16_t flag) {
    if (self != nullptr) {
        // The Mac tests this + 4 against NULL (never true for a real object); this == -4 is not modelled on the host.
        self->mVtable = kScreenFPUHandlerVtable;
        self->mVtable = kScreenFPUHandlerFlagVtable;
        reinterpret_cast<TbFPUFlag*>(&self->mVtable)->Reset();
        if (flag > 0) ScreenFPUHandler_OperatorDelete(self);
    }
    return self;
}

// ---- TbSystemInfo.cpp: __sinit_TbSystemInfo_cpp 0x10043d04

bullfrog::CPU gTbSystemInfoCPU;       // 0x100748ac
uint16_t gTbSystemInfoF1C[4];         // 0x100748c8

void __sinit_TbSystemInfo_cpp() {
    gTbSystemInfoCPU.detect();
    gTbSystemInfoF1C[0] = 0;
    gTbSystemInfoF1C[1] = 0;
    gTbSystemInfoF1C[2] = 0;
    gTbSystemInfoF1C[3] = 0;
}
