// LbTime.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AudioPlaceHolder.h"
#include "bfl_Refpack.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "helpers/RenderContextHelpers.h"
#include "LbDraw.h"
#include "LbDrawText.h"
#include "LbFile.h"
#include "LbPalette.h"
#include "LbSCBack.h"
#include "LbSurface.h"
#include "LbTime.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"
#include "TbFPUPrecision.h"
#include "TbRaConvert.h"
#include "TpwPropFont.h"
#include "WinScrn.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// LbTime_GetTime 0x100468e4 and LbTime_GetDate 0x10046968: fill the caller's structure from
// NS_MacDoze::GetLocalTime. The SYSTEMTIME is read by its Windows offsets (eight 16-bit
// fields: year 0, month 2, day of week 4, day 6, hour 8, minute 10, second 12, ms 14).

void LbTime_GetTime(TbTime* time) {
    uint16_t st[8];
    NS_MacDoze::GetLocalTime(reinterpret_cast<NS_MacDoze::_SYSTEMTIME*>(st));
    time->mHour = st[4];
    time->mMinute = st[5];
    time->mSecond = st[6];
    time->mMillisecond = st[7];
}

void LbTime_GetDate(TbDate* date) {
    uint16_t st[8];
    NS_MacDoze::GetLocalTime(reinterpret_cast<NS_MacDoze::_SYSTEMTIME*>(st));
    date->mDay = st[3];
    date->mMonth = st[1];
    date->mYear = st[0];
    date->mDayOfWeek = st[2];
}

uint32_t LbTime_GetClock() {
    return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
}

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

// ---- Draw.cpp
void LbTime_Delay(uint32_t ms) { NS_MacDoze::Sleep(ms); }   // 0x10046a54

// Units w234-w238: indexed and true-colour drawing primitives from TbSystemInfo.cpp and Draw.cpp.

int32_t gLbSpriteDrawState[10] = {};

namespace {
struct Writer8Sprite {
    uint8_t index;
    unsigned mode;
    explicit Writer8Sprite(TbColour colour) : index(colour.mIndex),
        mode((_lbCurrentDrawFlags & 8) != 0 ? 1 : (_lbCurrentDrawFlags & 0x10) != 0 ? 2 : 0) {}
    void operator()(uint8_t* destination) const {
        if (mode == 1) *destination = _lbCurrentGhostTable[uint32_t(index) * 256 + *destination];
        else if (mode == 2) *destination = _lbCurrentGhostTable[index + uint32_t(*destination) * 256];
        else *destination = index;
    }
};
}

void RenderContext8::LbDraw_Sprite(int32_t,int32_t,const TbSprite*) {
    gLbSpriteDrawState[0]=_lbpCurrentRA->mStride;
    gLbSpriteDrawState[1]=_lbCurrentClipRect.right-_lbCurrentClipRect.left;
    gLbSpriteDrawState[2]=_lbCurrentClipRect.bottom-_lbCurrentClipRect.top;
    gLbSpriteDrawState[3]=static_cast<int32_t>(_lbCurrentDrawFlags);
    gLbSpriteDrawState[4]=static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbCurrentOriginPtr));
    gLbSpriteDrawState[5]=static_cast<int32_t>(reinterpret_cast<uintptr_t>(_lbCurrentGhostTable));
    gLbSpriteDrawState[8]=_lbCurrentClipRect.left;
    gLbSpriteDrawState[9]=_lbCurrentClipRect.top;
}
void RenderContext8::LbDraw_OneColourSprite(int32_t x,int32_t y,const TbSprite* sprite,TbColour colour) {
    OneColourSpritePixels<1>(x,y,*sprite,Writer8Sprite(colour));
}

// 0x100474cc. The three comparisons with a constant TbError (data words 0x10073 7d4, 7d8, 7dc, never
// written) reduce to Succeeded().
TbError LbPalette_GenerateGhostTable(uint8_t** table, const TbPalette* palette, uint32_t percent,
                                     const char* name) {
    TbError error(0);
    uint8_t* buffer = *table;
    if (buffer == nullptr) {
        buffer = static_cast<uint8_t*>(LbMem_Alloc(0x10000));
        if (buffer == nullptr) return TbError(-1);
    }
    error = LbFile_Load(name, buffer, 0x10000, nullptr);
    if (!error.Succeeded()) {
        const uint8_t* colours = reinterpret_cast<const uint8_t*>(palette);
        const int32_t scale = static_cast<int32_t>(percent);
        uint8_t* out = buffer;
        for (int i = 0; i < 256; i++) {
            const uint8_t* from = colours + 4 * i;
            for (int j = 0; j < 256; j++) {
                const uint8_t* to = colours + 4 * j;
                int32_t r = from[0] + (scale * (to[0] - from[0])) / 100;
                int32_t g = from[1] + (scale * (to[1] - from[1])) / 100;
                int32_t b = from[2] + (scale * (to[2] - from[2])) / 100;
                *out++ = LbPalette_FindColour(palette, static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                              static_cast<uint8_t>(b));
            }
        }
        error = LbFile_Save(name, buffer, 0x10000);
        if (!error.Succeeded() && *table == nullptr) LbMem_Free(buffer);
    }
    if (error.Succeeded() && *table == nullptr) *table = buffer;
    return error;
}
