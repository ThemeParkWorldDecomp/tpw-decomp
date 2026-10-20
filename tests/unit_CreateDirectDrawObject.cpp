// Test library for the WinScrn.cpp DirectDraw/palette part (unit v5041; libtpw_unit_CreateDirectDrawObject.dylib,
// tests/test_CreateDirectDrawObject.py), main app:
//   src/CreateDirectDrawObject.cpp   CreateDirectDrawObject, EnumModesCallback, LbScreen_GetPaletteEntries,
//   LbScreen_SetPaletteEntries, LbScreen_SwapClear, __sinit_WinScrn_cpp, and the file's statics.
//
// The callees outside the unit are recording fakes (DirectDrawCreate, LbApp_SetHWnd, operator new (nothrow), LbList_Insert, the
// palette object, LbScreen_Swap, LbSurface_ClearRect, TbRenderArea::TbRenderArea, __register_global_object). The globals of other
// files the unit reads (the window handle, the DirectDraw palette, the screen surface, the global palette) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "WinScrn.h"

using namespace calllog;

// ---- globals of other files, defined here
struct TbPalette {
    uint8_t mBytes[0x400];
};
TbPalette _lbGlobalPalette;
void* gWinScrnHWnd;
IDirectDrawPalette* gWinScrnPalette;
TbSurface* gWinScrnSurface;

namespace {

TbList gList;
TbListNode gPrev;
void* gLastNew;

int64_t Sum(const void* p, uint32_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    uint32_t s = 0;
    for (uint32_t i = 0; i < n; i++) s += (i + 1) * b[i];
    return s;
}

class FakePalette : public IDirectDrawPalette {
public:
    int32_t GetEntries(uint32_t flags, uint32_t start, uint32_t count, TbPaletteEntry* entries) override {
        int64_t offset = reinterpret_cast<uint8_t*>(entries) - reinterpret_cast<uint8_t*>(&_lbGlobalPalette);
        ev("GetEntries", {lab(this), flags, start, count, offset});
        if (ret("fill") != 0) {
            uint8_t* b = reinterpret_cast<uint8_t*>(entries);
            for (uint32_t k = 0; k < count * 4; k++) b[k] = static_cast<uint8_t>(k * 37 + start + 5);
        }
        return static_cast<int32_t>(ret("GetEntries"));
    }
    int32_t SetEntries(uint32_t flags, uint32_t start, uint32_t count, const TbPaletteEntry* entries) override {
        ev("SetEntries", {lab(this), flags, start, count, lab(entries), Sum(entries, count * 4)});
        return 0;
    }
};
FakePalette gFake;

}  // namespace

// ---- the fakes
int32_t DirectDrawCreate(NS_MacDoze::_GUID* guid, IDirectDraw** out, NS_MacDoze::IUnknown* outer) {
    ev("DirectDrawCreate", {lab(guid), lab(out), static_cast<int64_t>(reinterpret_cast<uintptr_t>(outer))});
    return static_cast<int32_t>(ret("DirectDrawCreate"));
}

void LbApp_SetHWnd(void* hwnd) { ev("LbApp_SetHWnd", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(hwnd) & 0xffffffffu)}); }

// The host node is 8 bytes bigger than the Mac's (two native pointers): the logged size is the Mac's.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ev("new", {0x14});   // the Mac's node size; the host node is bigger (native pointers)
    if (ret("new") == 0) return nullptr;
    gLastNew = tpw::ArenaAllocZeroed(static_cast<uint32_t>(size));
    return gLastNew;
}

void LbList_Insert(TbList* list, TbListNode* after, TbListNode* node) {
    const TbDisplayMode* m = static_cast<const TbDisplayMode*>(node);
    ev("LbList_Insert", {lab(list), lab(after), node == gLastNew ? 1 : 0, m->mWidth, m->mHeight, m->mBitCount});
}

TbResult LbScreen_Swap(uint32_t flags) {
    ev("LbScreen_Swap", {flags});
    TbResult r;
    r.mCode = 0;
    return r;
}

TbResult LbSurface_ClearRect(const TbSurface* s, const TbRect* rect, TbColour c, uint32_t flags) {
    ev("LbSurface_ClearRect", {lab(s), lab(rect),
                               static_cast<int64_t>((uint32_t(c.mAlpha) << 24) | (c.mRed << 16) | (c.mGreen << 8) | c.mBlue),
                               c.mIndex, flags});
    TbResult r;
    r.mCode = 0;
    return r;
}

TbRenderArea::TbRenderArea() { ev("TbRenderArea", {lab(this)}); }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_ws_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"hwnd", &gWinScrnHWnd},
        {"pal", &gWinScrnPalette},
        {"screen", &gWinScrnSurface},
        {"lbpal", &_lbGlobalPalette},
        {"palobj", &gFake},
        {"list", &gList},
        {"prev", &gPrev},
        {"areaA", &gWinScrnSurfaceA.mArea},
        {"areaB", &gWinScrnSurfaceB.mArea},
        {"areaC", &gWinScrnSurfaceC.mArea},
        {"handler", &gScreenFPUHandler},
        {"node", gScreenFPUHandlerNode},
        {"dtor", kScreenFPUHandlerDtorVector},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ws_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ws_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbPaletteEntry));
    out[n++] = static_cast<int64_t>(sizeof(TbPalette));
    return n;
}

// ---- state set-up and read-back that depends on the host layout
void tpw_ws_set_pal(uint32_t on) { gWinScrnPalette = on ? &gFake : nullptr; }

static TbSurface* Surf(uint32_t i) { return i == 0 ? &gWinScrnSurfaceA : i == 1 ? &gWinScrnSurfaceB : &gWinScrnSurfaceC; }

// [colour x5, mF20, mDDSurface != 0]
void tpw_ws_set_surf(uint32_t i, const uint32_t* w) {
    TbSurface* s = Surf(i);
    for (int k = 0; k < 5; k++) s->mColour[k] = static_cast<uint8_t>(w[k]);
    s->mF20 = w[5];
    s->mDDSurface = w[6] ? reinterpret_cast<IDirectDrawSurface*>(static_cast<uintptr_t>(0x1000)) : nullptr;
}

void tpw_ws_get_surf(uint32_t i, uint32_t* w) {
    TbSurface* s = Surf(i);
    for (int k = 0; k < 5; k++) w[k] = s->mColour[k];
    w[5] = s->mF20;
    w[6] = s->mDDSurface != nullptr;
}

// [mF00, mVtable is the vtable, mF08, mF0A, mF0C]
void tpw_ws_set_handler(const uint32_t* w) {
    gScreenFPUHandler.mF00 = static_cast<uint8_t>(w[0]);
    gScreenFPUHandler.mVtable = w[1] ? kScreenFPUHandlerVtable : nullptr;
    gScreenFPUHandler.mF08 = static_cast<uint16_t>(w[2]);
    gScreenFPUHandler.mF0A = static_cast<uint16_t>(w[3]);
    gScreenFPUHandler.mF0C = static_cast<uint16_t>(w[4]);
}

void tpw_ws_get_handler(uint32_t* w) {
    w[0] = gScreenFPUHandler.mF00;
    w[1] = gScreenFPUHandler.mVtable == kScreenFPUHandlerVtable;
    w[2] = gScreenFPUHandler.mF08;
    w[3] = gScreenFPUHandler.mF0A;
    w[4] = gScreenFPUHandler.mF0C;
}

// ---- the functions
int32_t tpw_ws_create(uint64_t guid, uint64_t out, uint64_t unused) {
    return CreateDirectDrawObject(reinterpret_cast<NS_MacDoze::_GUID*>(static_cast<uintptr_t>(guid)),
                                  reinterpret_cast<IDirectDraw**>(static_cast<uintptr_t>(out)),
                                  reinterpret_cast<void**>(static_cast<uintptr_t>(unused))).mCode;
}

uint32_t tpw_ws_enum(uint32_t height, uint32_t width, uint32_t bits) {
    gList.mTailPrev = &gPrev;
    DDSURFACEDESC d;
    std::memset(&d, 0, sizeof d);
    d.dwHeight = height;
    d.dwWidth = width;
    d.ddpfPixelFormat.dwRGBBitCount = bits;
    return EnumModesCallback(&d, &gList) ? 1 : 0;
}

uint64_t tpw_ws_get(uint64_t dst, uint32_t start, uint32_t count) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(LbScreen_GetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(static_cast<uintptr_t>(dst)), start, count)));
}

void tpw_ws_set(uint64_t src, uint32_t start, uint32_t count) {
    LbScreen_SetPaletteEntries(reinterpret_cast<const TbPaletteEntry*>(static_cast<uintptr_t>(src)), start, count);
}

void tpw_ws_swapclear(uint32_t argb, uint32_t index) {
    // TbColour's constructor lives in another file; the bytes are built raw and copied (the copy is trivial).
    alignas(TbColour) uint8_t raw[sizeof(TbColour)] = {static_cast<uint8_t>(argb >> 24), static_cast<uint8_t>(argb >> 16),
                                                       static_cast<uint8_t>(argb >> 8), static_cast<uint8_t>(argb),
                                                       static_cast<uint8_t>(index)};
    LbScreen_SwapClear(*reinterpret_cast<const TbColour*>(raw));
}

void tpw_ws_sinit() { __sinit_WinScrn_cpp(); }

}  // extern "C"
