// Test library for the small bullfrog functions of unit v51071 (libtpw_unit_LbSystemInfo_CPUGetVendor.dylib,
// tests/test_LbSystemInfo_CPUGetVendor.py), bullfrog:
//   src/CreateDirectDrawObject_RegisterBullfrogWindowClass_2_LbSystemInfo_CPUGetVendor.cpp
//
// Callees outside the unit are recording fakes: CPU::clocks_per_millisec, NS_MacDoze::Sleep, IDirectDraw::WaitForVerticalBlank.
// The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "WinScrn.h"
#include "LbSurface.h"
#include "LbSCBack.h"
#include "DrawState.h"
#include "AudioPlaceHolder.h"
#include "TbGraphicFile.h"

using namespace calllog;

// ---- globals of other files, defined here
bullfrog::CPU gTbSystemInfoCPU;
int32_t gLbFadeActive;
int32_t gWinScrnInitialised;
NS_MacDoze::_GUID* gWinScrnGuid;
int32_t gWinScrnActive;
IDirectDraw* gWinScrnDirectDraw;
char gLbWindowName[64];
int16_t gLbWindowIcon;
uint32_t gTbJPEGSaverMasks[6];
double gMathLibPi;
extern "C" double gLbMathsAtanScale;
double gLbMathsAtanScale;
namespace bfl::draw {
CDrawContext gDrawContext8;
CDrawContext gDrawContext16;
CDrawContext gDrawContext24;
CDrawContext gDrawContext32;
}
using bfl::draw::gDrawContext8;
using bfl::draw::gDrawContext16;
using bfl::draw::gDrawContext24;
using bfl::draw::gDrawContext32;
uint8_t kDrawContextVtable8[0x20];
uint8_t kDrawContextVtable16[0x20];
uint8_t kDrawContextVtable24[0x20];
uint8_t kDrawContextVtable32[0x20];

namespace {
uint8_t gTestDd[8];
}

// ---- the fakes
int32_t bullfrog::CPU::clocks_per_millisec(int32_t unused) {
    ev("clocks", {lab(this), unused});
    return static_cast<int32_t>(ret("clocks"));
}

namespace NS_MacDoze {
void Sleep(uint32_t ms) { ev("Sleep", {static_cast<int64_t>(ms)}); }
}

int32_t IDirectDraw_WaitForVerticalBlank(IDirectDraw* self, uint32_t flags, NS_MacDoze::InternalHandle* handle) {
    ev("WaitVbi", {lab(self), static_cast<int64_t>(flags), lab(handle)});
    return 0;
}

extern "C" {

uint64_t tpw_lsi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"cpu", &gTbSystemInfoCPU}, {"guid", &gWinScrnGuid}, {"active", &gWinScrnActive}, {"init", &gWinScrnInitialised},
        {"ddslot", &gWinScrnDirectDraw}, {"dd", gTestDd}, {"fade", &gLbFadeActive}, {"jpeg", gTbJPEGSaverMasks},
        {"draw32", &gDrawContext32}, {"draw24", &gDrawContext24}, {"draw16", &gDrawContext16}, {"draw8", &gDrawContext8},
        {"t32", kDrawContextVtable32}, {"t24", kDrawContextVtable24}, {"t16", kDrawContextVtable16}, {"t8", kDrawContextVtable8},
        {"atan", &gLbMathsAtanScale}, {"pi", &gMathLibPi}, {"name", gLbWindowName}, {"icon", &gLbWindowIcon},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lsi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lsi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(bullfrog::CPU, mType));
    out[n++] = static_cast<int64_t>(offsetof(bullfrog::CPU, mFeatures));
    out[n++] = static_cast<int64_t>(offsetof(bullfrog::CPU, mHasRDTSC));
    out[n++] = static_cast<int64_t>(offsetof(bullfrog::CPU, mName));
    out[n++] = static_cast<int64_t>(sizeof(bullfrog::CPU));
    out[n++] = static_cast<int64_t>(sizeof(TbRect));
    out[n++] = static_cast<int64_t>(sizeof(TbSize));
    out[n++] = static_cast<int64_t>(sizeof(TbPoint));
    return n;
}

// One dispatcher: op numbers are the order of OPS in the Python module.
uint64_t tpw_lsi_call(int32_t op, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    TbRect* ra = reinterpret_cast<TbRect*>(a);
    TbRect* rb = reinterpret_cast<TbRect*>(b);
    TbSize* sa = reinterpret_cast<TbSize*>(a);
    const int32_t ib = static_cast<int32_t>(b), ic = static_cast<int32_t>(c), id = static_cast<int32_t>(d), ie = static_cast<int32_t>(e);
    switch (op) {
    case 0: LbSystemInfo_CPUGetVendor(reinterpret_cast<char*>(a)); return 0;
    case 1: return LbSystemInfo_CPUGetFamily();
    case 2: return LbSystemInfo_CPUhasMMX();
    case 3: return static_cast<uint32_t>(LbSystemInfo_CPUhasRDTSC());
    case 4: return static_cast<uint32_t>(LbSystemInfo_CPUGetTickRate(static_cast<uint32_t>(a)));
    case 5: LbTime_Delay(static_cast<uint32_t>(a)); return 0;
    case 6: return static_cast<uint32_t>(LbSize_Area(sa));
    case 7: LbSize_Set(sa, static_cast<uint32_t>(b), static_cast<uint32_t>(c)); return 0;
    case 8: return LbSize_IsNull(sa);
    case 9: return LbRect_Intersects(ra, rb);
    case 10: return LbRect_IsNormal(ra);
    case 11: return LbRect_IsEmpty(ra);
    case 12: LbRect_Offset(ra, ib, ic); return 0;
    case 13: return reinterpret_cast<uintptr_t>(LbRect_Copy(ra, rb));
    case 14: LbRect_SetWindow(ra, ib, ic, id, ie); return 0;
    case 15: LbRect_SetFromSize(ra, reinterpret_cast<TbSize*>(b)); return 0;
    case 16: LbRect_Set(ra, ib, ic, id, ie); return 0;
    case 17: LbPoint_Offset(reinterpret_cast<TbPoint*>(a), ib, ic); return 0;
    case 18: LbPoint_Set(reinterpret_cast<TbPoint*>(a), ib, ic); return 0;
    case 19: __sinit_LbMaths_cpp(); return 0;
    case 20: __sinit_TbJPEGSaver_cpp(); return 0;
    case 21: __sinit_Draw_cpp(); return 0;
    case 22: LbScreen_StopOpenFadePalette(); return 0;
    case 23: LbScreen_SetDirectDrawGuid(reinterpret_cast<NS_MacDoze::_GUID*>(static_cast<uintptr_t>(static_cast<uint32_t>(a)))); return 0;
    case 24: return static_cast<uint32_t>(LbScreen_IsInitialised());
    case 25: return static_cast<uint32_t>(LbScreen_IsActive());
    case 26: LbScreen_ModeFindEnd(reinterpret_cast<TbScreenModeFind*>(a)); return 0;
    case 27: LbScreen_WaitVbi(); return 0;
    case 28: return reinterpret_cast<uintptr_t>(LbApp_GetWindowName());
    case 29: return static_cast<uint32_t>(static_cast<int32_t>(LbApp_GetWindowIcon()));
    }
    return 0;
}

}  // extern "C"
