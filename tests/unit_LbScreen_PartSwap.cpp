// Test library for the screen swap and mode list functions (unit v5334; libtpw_unit_LbScreen_PartSwap.dylib, tests/test_LbScreen_PartSwap.py), main app:
//   src/bullfrog/WinScrn.cpp   LbScreen_Swap, LbScreen_PartSwap, CreateScreenModeList and four globals
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The surfaces and
// other globals of WinScrn.cpp that the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "WinScrn.h"
#include "platform/macdoze.h"

using namespace calllog;

#define U(x) static_cast<int64_t>(static_cast<uint32_t>(x))
#define PTR(p) U(reinterpret_cast<uintptr_t>(p))

// ---- globals of other files the unit reads, defined here
TbSurface gWinScrnSurfaceA;
TbSurface gWinScrnSurfaceB;
TbSurface* gWinScrnSurface;
void* gWinScrnHWnd;
char gWinScrnClassName[64];
_LbSystemEventCallback gLbSystemEventCallback;

static IDirectDraw* gDdObject;   // what the DirectDrawCreate fake hands out

namespace {
struct SurfaceEvent {
    int32_t mKind;
    TbSurface* mSurface;
};

int64_t Hash(const char* s) {
    uint32_t h = 0;
    if (s == nullptr) return 0;
    for (; *s; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return h;
}
}  // namespace

// ---- the fakes, the same event names and arguments as the module's PowerPC stubs
void _LbSystemEventCallback::EventNotification(int32_t type, void* data) {
    const SurfaceEvent* e = static_cast<const SurfaceEvent*>(data);
    ev("Event", {type, U(e->mKind), lab(e->mSurface)});}

int32_t _CheckAndRestoreSurface(const TbSurface* s) {
    ev("Restore", {lab(s)});
    return 0;
}

void* LbApp_GetHWnd() {
    ev("GetHWnd", {});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("hwnd")));
}
void* LbApp_GetHInstance() {
    ev("GetHInstance", {});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("hinst")));
}
void LbApp_SetHWnd(void* hwnd) { ev("SetHWnd", {PTR(hwnd)}); }
void RegisterBullfrogWindowClass() { ev("RegisterClass", {}); }
void LbList_Init(TbList* list) { ev("ListInit", {lab(list)}); }
bool EnumModesCallback(DDSURFACEDESC*, void*) { return false; }

int32_t IDirectDrawSurface::Blt(SamsUtilities::URectangle::LongRect* dest, IDirectDrawSurface* src,
                                SamsUtilities::URectangle::LongRect* srcRect, uint32_t flags, DDBLTFX* fx) {
    const int32_t* d = reinterpret_cast<const int32_t*>(dest);
    const uint32_t* f = reinterpret_cast<const uint32_t*>(fx);
    int64_t other = 0;
    for (int i = 1; i < 25; ++i) other |= f[i];
    ev("Blt", {lab(this), lab(src), PTR(srcRect), U(flags), fx ? 1 : 0, fx ? U(f[0]) : 0, other != 0, dest ? 1 : 0,
               dest ? U(d[0]) : 0, dest ? U(d[1]) : 0, dest ? U(d[2]) : 0, dest ? U(d[3]) : 0});
    return static_cast<int32_t>(ret("blt"));
}

int32_t IDirectDrawSurface::BltFast(uint32_t x, uint32_t y, IDirectDrawSurface* src, SamsUtilities::URectangle::LongRect* rect,
                                    uint32_t flags) {
    const int32_t* d = reinterpret_cast<const int32_t*>(rect);
    ev("BltFast", {lab(this), U(x), U(y), lab(src), rect ? 1 : 0, U(flags), rect ? U(d[0]) : 0, rect ? U(d[1]) : 0,
                   rect ? U(d[2]) : 0, rect ? U(d[3]) : 0});
    return static_cast<int32_t>(ret("bltfast"));
}

int32_t IDirectDrawSurface_Flip(IDirectDrawSurface* self, IDirectDrawSurface* other, uint32_t flags) {
    ev("Flip", {lab(self), lab(other), U(flags)});
    return static_cast<int32_t>(ret("flip"));
}

int32_t IDirectDraw::SetCooperativeLevel(GrafPort* window, uint32_t flags) {
    ev("SetCoop", {lab(this), PTR(window), U(flags)});
    return static_cast<int32_t>(ret("coop"));
}

int32_t IDirectDraw_EnumDisplayModes(IDirectDraw* self, uint32_t flags, DDSURFACEDESC* desc, void* context,
                                     bool (*callback)(DDSURFACEDESC*, void*)) {
    ev("EnumModes", {lab(self), U(flags), PTR(desc), lab(context), callback == EnumModesCallback ? 1 : 0});
    return 0;
}

int32_t DirectDrawCreate(NS_MacDoze::_GUID* guid, IDirectDraw** out, NS_MacDoze::IUnknown* unk) {
    ev("DirectDrawCreate", {PTR(guid), PTR(unk)});
    const int32_t r = static_cast<int32_t>(ret("ddc"));
    if (r == 0) *out = gDdObject;
    return r;
}

namespace NS_MacDoze {
int32_t IUnknown::QueryInterface(const _GUID&, void**) { return 0; }   // never called here (the vtable needs it)
uint32_t IUnknown::AddRef() { return 0; }
IUnknown::~IUnknown() {}
uint32_t IUnknown::Release() {
    ev("Release", {lab(this)});
    return 0;
}
void ZeroMemory(void* p, uint32_t n) {
    std::memset(p, 0, n);
    ev("Zero", {U(n)});
}
int32_t GetClientRect(GrafPort* window, SamsUtilities::URectangle::LongRect* rect) {
    ev("GetClientRect", {PTR(window)});
    int32_t* r = reinterpret_cast<int32_t*>(rect);
    r[0] = static_cast<int32_t>(ret("rect0"));
    r[1] = static_cast<int32_t>(ret("rect1"));
    r[2] = static_cast<int32_t>(ret("rect2"));
    r[3] = static_cast<int32_t>(ret("rect3"));
    return 0;
}
void ClientToScreen(GrafPort* window, tagPOINT* point) {
    int32_t* p = reinterpret_cast<int32_t*>(point);
    ev("ClientToScreen", {PTR(window), U(p[0]), U(p[1])});
    p[0] += static_cast<int32_t>(ret("cx"));
    p[1] += static_cast<int32_t>(ret("cy"));
}
void DestroyWindow(GrafPort* window) { ev("DestroyWindow", {PTR(window)}); }
void* CreateWindowEx(uint32_t exStyle, const char* className, const char* windowName, uint32_t style, int32_t x, int32_t y,
                     int32_t width, int32_t height, GrafPort* parent, void* menu, void* instance, void* param) {
    ev("CreateWindowEx", {U(exStyle), Hash(className), Hash(windowName), U(style), U(x), U(y), U(width), U(height), PTR(parent),
                          PTR(menu), PTR(instance), PTR(param)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("createwindow")));
}
}  // namespace NS_MacDoze

extern "C" {

uint64_t tpw_sw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"A", &gWinScrnSurfaceA},          {"B", &gWinScrnSurfaceB},     {"SURFPTR", &gWinScrnSurface},
        {"DBL", &gWinScrnDoubleBuffer},    {"GATE", &gWinScrnModeGate},  {"LIST", &gWinScrnModeList},
        {"ORIGIN", gWinScrnClientOrigin},  {"HWND", &gWinScrnHWnd},      {"CLASS", gWinScrnClassName},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// Real objects for the DirectDraw pointers (their members are called through the vtable): kind 0 a surface, 1 a DirectDraw.
uint64_t tpw_sw_newobj(int32_t kind) {
    // 32-bit addressable: the world writes these pointers as 32-bit words.
    if (kind == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (tpw::ArenaAllocZeroed(sizeof(IDirectDrawSurface))) IDirectDrawSurface()));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (tpw::ArenaAllocZeroed(sizeof(IDirectDraw))) IDirectDraw()));
}
void tpw_sw_set_ddobject(uint64_t p) { gDdObject = reinterpret_cast<IDirectDraw*>(static_cast<uintptr_t>(p)); }

uint64_t tpw_sw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbSurface, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(TbSurface, mDDSurface));
    return n;
}

// The TbResult is one 32-bit word.
static uint32_t Word(TbResult r) {
    uint32_t v;
    std::memcpy(&v, &r, 4);
    return v;
}

uint32_t tpw_sw_swap(uint32_t flags) {
    return Word(LbScreen_Swap(flags)); }
uint32_t tpw_sw_partswap(int32_t x, int32_t y, int32_t l, int32_t t, int32_t r, int32_t b, uint32_t flags) {
    TbPoint at{x, y};
    TbRect rect{l, t, r, b};
    return Word(LbScreen_PartSwap(at, rect, flags));
}
uint32_t tpw_sw_modelist(int32_t mode, uint32_t window, uint32_t guid) {
    return Word(CreateScreenModeList(mode, reinterpret_cast<GrafPort*>(static_cast<uintptr_t>(window)),
                                     reinterpret_cast<NS_MacDoze::_GUID*>(static_cast<uintptr_t>(guid))));
}

}  // extern "C"
