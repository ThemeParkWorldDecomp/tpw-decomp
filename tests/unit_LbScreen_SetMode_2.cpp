// Test library for LbScreen_SetMode (unit v51000; libtpw_unit_LbScreen_SetMode_2.dylib, tests/test_LbScreen_SetMode_2.py), main app ("bullfrog"):
//   src/LbScreen_Initialise_LbScreen_Terminate_LbScreen_SetMode_2.cpp   LbScreen_SetMode
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues. The globals of
// WinScrn.cpp, LbSCBack.cpp and LbPalette are defined here, zero at start like the Mac's bss. The objects the fakes hand out are
// 16-byte arena blocks (32-bit addressable), named obj1..obj12 for the test.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "LbSCBack.h"
#include "TbFPUPrecision.h"
#include "WinScrn.h"

using namespace calllog;

struct TbPalette { uint8_t mBytes[0x400]; };

// ---- globals of other files, defined here
TbPalette _lbGlobalPalette;
_ScreenFPUHandler gScreenFPUHandler;
void* gWinScrnHWnd;
IDirectDrawPalette* gWinScrnPalette;
TbSurface* gWinScrnSurface;
TbSurface gWinScrnSurfaceA;
TbSurface gWinScrnSurfaceB;
TbSurface gWinScrnSurfaceC;
NS_MacDoze::_GUID* gWinScrnGuid;
int32_t gWinScrnF120;
int32_t gWinScrnActive;
int32_t gWinScrnDoubleBuffer;
NS_MacDoze::IUnknown* gWinScrnClipper;
IDirectDraw* gWinScrnDirectDraw;
GrafPort* gWinScrnCoopWindow;
_LbSystemEventCallback gLbSystemEventCallback;

namespace {

constexpr int kObjects = 12;
void* gObjects[kObjects];
void* gInPalette;

void* Obj(int index) {   // 1-based, 0 is NULL
    if (index <= 0) return nullptr;
    if (gObjects[index - 1] == nullptr) gObjects[index - 1] = tpw::ArenaAllocZeroed(16);
    return gObjects[index - 1];
}

uint32_t Hash(const uint8_t* bytes, size_t count) {
    uint32_t h = 0;
    for (size_t i = 0; i < count; ++i) h = h * 31u + bytes[i];
    return h;
}

int32_t Hr(const char* key) { return static_cast<int32_t>(ret(key)); }

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs (pointers as lab(p))
int32_t TbFPUFlag::CheckFlags() {
    ev("CheckFlags", {lab(this)});
    return 0;
}

void TbFPUFlag::SetFlags(uint16_t set, uint16_t clear) { ev("SetFlags", {lab(this), set, clear}); }

void _LbSystemEventCallback::EventNotification(int32_t type, void* data) {
    const uint32_t* w = static_cast<const uint32_t*>(data);
    if (w[0] == 7) {
        ev("Event", {type, 7, w[1], w[2], static_cast<const uint8_t*>(data)[12], w[4]});
    } else {
        ev("Event", {type, w[0], w[1], 0, 0, 0});
    }
}

void ReleaseMode() { ev("ReleaseMode", {0}); }

TbResult CreateDirectDrawObject(NS_MacDoze::_GUID* guid, IDirectDraw** out, void** unused) {
    ev("CreateDD", {lab(guid), out == &gWinScrnDirectDraw ? 1 : 0, static_cast<int64_t>(reinterpret_cast<uintptr_t>(unused))});
    TbResult result;
    result.mCode = Hr("CreateDD");
    if (ret("CreateDD.out") != 0) *out = static_cast<IDirectDraw*>(Obj(1));
    return result;
}

void LbSurface_RegisterDirectDraw(IDirectDraw* dd) { ev("RegisterDD", {lab(dd)}); }

void LbApp_ShowMessage(const char* format, ...) {
    ev("ShowMessage", {Hash(reinterpret_cast<const uint8_t*>(format), std::strlen(format))});
}

void* LbApp_GetHWnd() {
    ev("GetHWnd", {0});
    return Obj(static_cast<int>(ret("GetHWnd")));
}

void _InitialiseSurface(TbSurface* surface, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    ev("InitialiseSurface", {lab(surface), a, b, c, d});
}

uint32_t NS_MacDoze::IUnknown::Release() {
    ev("Release", {lab(this)});
    return 0;
}

int32_t IDirectDraw::RestoreDisplayMode() {
    ev("RestoreDisplayMode", {lab(this)});
    return 0;
}

int32_t IDirectDraw::SetCooperativeLevel(GrafPort* window, uint32_t level) {
    ev("SetCooperativeLevel", {lab(this), lab(window), level});
    return Hr("SetCooperativeLevel");
}

int32_t IDirectDraw::CreateClipper(uint32_t flags, IDirectDrawClipper** out, NS_MacDoze::IUnknown* outer) {
    ev("CreateClipper", {lab(this), flags, out == reinterpret_cast<IDirectDrawClipper**>(&gWinScrnClipper) ? 1 : 0,
                         static_cast<int64_t>(reinterpret_cast<uintptr_t>(outer))});
    int32_t hr = Hr("CreateClipper");
    if (ret("CreateClipper.out") != 0) *out = static_cast<IDirectDrawClipper*>(Obj(2));
    return hr;
}

int32_t IDirectDrawClipper::SetHWnd(uint32_t flags, GrafPort* window) {
    ev("ClipperSetHWnd", {lab(this), flags, lab(window)});
    return Hr("SetHWnd");
}

int32_t IDirectDraw::SetDisplayMode(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    ev("SetDisplayMode", {lab(this), a, b, c, d, e});
    return Hr("SetDisplayMode");
}

int32_t IDirectDraw::CreateSurface(DDSURFACEDESC* desc, IDirectDrawSurface** out, NS_MacDoze::IUnknown* outer) {
    int64_t which = out == &gWinScrnSurfaceB.mDDSurface ? 1 : (out == &gWinScrnSurfaceC.mDDSurface ? 2 : 0);
    ev("CreateSurface", {lab(this), desc->dwSize, desc->dwFlags, desc->dwHeight, desc->dwWidth, desc->dwBackBufferCount, desc->dwCaps,
                         which, static_cast<int64_t>(reinterpret_cast<uintptr_t>(outer))});
    int32_t hr = Hr("CreateSurface");
    if (which == 1) {
        *out = static_cast<IDirectDrawSurface*>(Obj(4));
    } else if (ret("CreateSurface.out") != 0) {
        *out = static_cast<IDirectDrawSurface*>(Obj(5));
    }
    return hr;
}

int32_t IDirectDrawSurface::GetSurfaceDesc(DDSURFACEDESC* desc) {
    ev("GetSurfaceDesc", {lab(this)});
    desc->dwCaps = static_cast<uint32_t>(ret("GetSurfaceDesc.caps"));
    return Hr("GetSurfaceDesc");
}

int32_t IDirectDrawSurface::SetClipper(IDirectDrawClipper* clipper) {
    ev("SurfaceSetClipper", {lab(this), lab(clipper)});
    return 0;
}

int32_t IDirectDraw_GetDisplayMode(IDirectDraw* self, DDSURFACEDESC* desc) {
    ev("GetDisplayMode", {lab(self), desc->dwSize});
    desc->ddpfPixelFormat.dwRGBBitCount = static_cast<uint32_t>(ret("GetDisplayMode.bits"));
    return Hr("GetDisplayMode");
}

int32_t IDirectDraw_CreatePalette(IDirectDraw* self, uint32_t flags, const TbPaletteEntry* entries, IDirectDrawPalette** out,
                                  NS_MacDoze::IUnknown* outer) {
    ev("CreatePalette", {lab(self), flags, lab(entries), out == &gWinScrnPalette ? 1 : 0,
                         static_cast<int64_t>(reinterpret_cast<uintptr_t>(outer)),
                         Hash(reinterpret_cast<const uint8_t*>(entries), 0x400)});
    int32_t hr = Hr("CreatePalette");
    if (hr >= 0) *out = static_cast<IDirectDrawPalette*>(Obj(3));
    return hr;
}

int32_t IDirectDrawSurface_GetAttachedSurface(IDirectDrawSurface* self, DDSCAPS* caps, IDirectDrawSurface** out) {
    int64_t which = out == &gWinScrnSurfaceA.mDDSurface ? 3 : (out == &gWinScrnSurfaceC.mDDSurface ? 2 : 0);
    ev("GetAttachedSurface", {lab(self), caps->dwCaps, which});
    int32_t hr = Hr("GetAttachedSurface");
    if (ret("GetAttachedSurface.out") != 0) *out = static_cast<IDirectDrawSurface*>(Obj(which == 3 ? 6 : 5));
    return hr;
}

int32_t IDirectDrawSurface_GetDC(IDirectDrawSurface* self, NS_MacDoze::InternalHandle** dc) {
    ev("GetDC", {lab(self)});
    *dc = static_cast<NS_MacDoze::InternalHandle*>(Obj(10));
    return Hr("GetDC");
}

int32_t IDirectDrawSurface_ReleaseDC(IDirectDrawSurface* self, NS_MacDoze::InternalHandle* dc) {
    ev("ReleaseDC", {lab(self), lab(dc)});
    return 0;
}

int32_t IDirectDrawSurface_SetPalette(IDirectDrawSurface* self, IDirectDrawPalette* palette) {
    ev("SetPalette", {lab(self), lab(palette)});
    return 0;
}

void NS_MacDoze::ZeroMemory(void* p, uint32_t size) {
    ev("ZeroMemory", {size});
    std::memset(p, 0, size);
}

int32_t NS_MacDoze::ShowWindow(GrafPort* window, int32_t command) {
    ev("ShowWindow", {lab(window), command});
    return 0;
}

int32_t NS_MacDoze::PeekMessage(tagMSG* msg, GrafPort* window, uint32_t min, uint32_t max, uint32_t remove) {
    ev("PeekMessage", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(window)), min, max, remove});
    int32_t k = Hr("PeekMessage");
    if (k != 0) {
        uint32_t words[4] = {0x1000u + k, 0x2000u + k, 0x3000u + k, 0x4000u + k};
        std::memcpy(msg->mBytes, words, sizeof(words));
        return 1;
    }
    return 0;
}

int32_t NS_MacDoze::DefWindowProc(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam) {
    ev("DefWindowProc", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(window)), message, wParam,
                         static_cast<int64_t>(reinterpret_cast<uintptr_t>(lParam))});
    return 0;
}

void NS_MacDoze::Sleep(uint32_t ms) { ev("Sleep", {ms}); }

int32_t NS_MacDoze::GetSystemPaletteEntries(InternalHandle* dc, uint32_t start, uint32_t count, TbPaletteEntry* entries) {
    ev("GetSystemPaletteEntries", {lab(dc), start, count, entries == reinterpret_cast<TbPaletteEntry*>(&_lbGlobalPalette) ? 1 : 0});
    uint32_t seed = static_cast<uint32_t>(ret("GetSystemPaletteEntries"));
    uint8_t* bytes = reinterpret_cast<uint8_t*>(entries);
    for (uint32_t i = 0; i < 0x400; ++i) bytes[i] = static_cast<uint8_t>(i * 7u + seed);
    return 0;
}

extern "C" {

// The host address of a global (or a fake object) by the module's name for it.
uint64_t tpw_lbsm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"fpu", &gScreenFPUHandler.mF00},
        {"fpunode", &gScreenFPUHandler.mVtable},
        {"doublebuf", &gWinScrnDoubleBuffer},
        {"guid", &gWinScrnGuid},
        {"f120", &gWinScrnF120},
        {"active", &gWinScrnActive},
        {"A_dd", &gWinScrnSurfaceA.mDDSurface},
        {"B_surf", &gWinScrnSurfaceB},
        {"B_dd", &gWinScrnSurfaceB.mDDSurface},
        {"C_surf", &gWinScrnSurfaceC},
        {"C_dd", &gWinScrnSurfaceC.mDDSurface},
        {"surface", &gWinScrnSurface},
        {"clipper", &gWinScrnClipper},
        {"palette", &gWinScrnPalette},
        {"dd", &gWinScrnDirectDraw},
        {"hwnd", &gWinScrnHWnd},
        {"coop", &gWinScrnCoopWindow},
        {"gpal", &_lbGlobalPalette},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    if (std::strcmp(name, "inpal") == 0) {
        if (gInPalette == nullptr) gInPalette = tpw::ArenaAllocZeroed(0x400);
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gInPalette));
    }
    if (std::strncmp(name, "obj", 3) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Obj(std::atoi(name + 3))));
    return 0;
}

uint64_t tpw_lbsm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The sizes the module relies on (HOST_LAYOUT).
int32_t tpw_lbsm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbPalette));
    out[n++] = static_cast<int64_t>(sizeof(NS_MacDoze::tagMSG));
    out[n++] = static_cast<int64_t>(sizeof(_ScreenFPUHandler::mF00));
    return n;
}

int32_t tpw_lbsm_SetMode(uint32_t width, uint32_t height, uint32_t bpp, uint32_t flags, uint64_t palette) {
    return LbScreen_SetMode(width, height, bpp, flags, reinterpret_cast<const TbPalette*>(static_cast<uintptr_t>(palette))).mCode;
}

}  // extern "C"
