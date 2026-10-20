// Differential boundary fakes for u8b. The unit's start-up functions run normally;
// every other engine/platform call records its arguments and uses per-case queues.
#include "calllog.h"
#include "tb_ddraw.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_error.h"
#include "platform/macdoze.h"
#include "platform/opengl.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>
#include <cstdlib>
using namespace calllog;

CSystem::CSystem() {}
CSystem gei_System;
CDisplay gei_Display;
CDisplay::CDisplay() {}   // the unit keeps a blank display of its own
CErrorHandler gei_ErrorHandler;
CDrawPrimitive* gei_DrawPrimitive;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Push", {level, code}); }

namespace {
IDirectDraw4 dd1, dd2, dd4;
IDirectDrawClipper clip;
IDirectDrawSurface4 primary, back, other;
SamsUtilities::ULogManager oldLogger;
SamsUtilities::UAllocator oldAllocator;
SamsUtilities::UOpenGL context;
alignas(CDirectDraw) unsigned char ddStore[sizeof(CDirectDraw)];
alignas(COpenGL) unsigned char glStore[sizeof(COpenGL)];
alignas(COpenGLDrawPrimitive) unsigned char drawStore[sizeof(COpenGLDrawPrimitive)];
alignas(SamsUtilities::UMemoryAlignedBase) unsigned char memStore[sizeof(SamsUtilities::UMemoryAlignedBase)];
CClipping* clipStore;
uint32_t descriptor[31];
int32_t skipBase;
// Destructive wrappers get heap copies of the prepared host object representations.
// This keeps the reusable fixture storage alive and retains its native vtables.
template<class T> T* heapCopy(const void* source) {
    void* storage = std::malloc(sizeof(T));
    std::memcpy(storage, source, sizeof(T));
    return static_cast<T*>(storage);
}
void label(int32_t n, const void* p) { tpw_cl_label(n, p); }
void* ptr(int32_t label) {
    switch (label) {
    case 11: return &dd1; case 12: return &dd2; case 13: return &dd4;
    case 14: return &clip; case 15: return &primary; case 16: return &back;
    case 17: return &other; case 19: return reinterpret_cast<void*>(0x1234);
    default: return nullptr;
    }
}
uint32_t size32(uint32_t n) { return n == sizeof(DDSURFACEDESC2) ? 124 : n; }
uint32_t beword(const void* p) {
    const auto* b = static_cast<const uint8_t*>(p);
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
}
void beput(void* p, uint32_t v) {
    auto* b = static_cast<uint8_t*>(p);
    b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;
}
void fillDesc(DDSURFACEDESC2* d) {
    std::memcpy(d, descriptor, 9 * 4);
    d->dwSize = descriptor[0] == 124 ? sizeof *d : descriptor[0];
    d->lpSurface = ptr(descriptor[9]);
    std::memcpy(&d->ddckCKDestOverlay, descriptor + 10, 21 * 4);
}
void descWords(const DDSURFACEDESC2& d, uint32_t* w) {
    std::memcpy(w, &d, 9 * 4); w[0] = size32(d.dwSize); w[9] = lab(d.lpSurface);
    std::memcpy(w + 10, &d.ddckCKDestOverlay, 21 * 4);
}
int32_t answer(const char* name, std::initializer_list<int64_t> args = {}) {
    ev(name, args); return static_cast<int32_t>(ret(name));
}
void getMode(const char* name, const void* self, DDSURFACEDESC2* d, int32_t& result) {
    ev(name, {lab(self), size32(d->dwSize)});
    result = static_cast<int32_t>(ret(name));
    if (!result) fillDesc(d);
}
}

namespace NS_MacDoze {
IUnknown::~IUnknown() {}
int32_t IUnknown::QueryInterface(const _GUID& id, void** out) {
    const int32_t version = &id == &IID_IDirectDraw2 ? 2 : 4;
    int32_t result = answer(version == 2 ? "Query2" : "Query4", {lab(this)});
    if (!result) *out = ptr(version == 2 ? 12 : 13);
    return result;
}
uint32_t IUnknown::Release() { return answer("Release", {lab(this)}); }
void ZeroMemory(void* p, uint32_t n) { ev("ZeroMemory", {n}); std::memset(p, 0, n); }
int32_t ShowWindow(GrafPort* w, int32_t n) { return answer("ShowWindow", {lab(w), n}); }
int32_t UpdateWindow(GrafPort* w) { return answer("UpdateWindow", {lab(w)}); }
int32_t GetClientRect(GrafPort* w, SamsUtilities::URectangle::LongRect* r) {
    ev("GetClientRect", {lab(w)}); *r = {3, 7, 303, 207}; return ret("GetClientRect");
}
int32_t GetWindowRect(GrafPort* w, SamsUtilities::URectangle::LongRect* r) {
    ev("GetWindowRect", {lab(w)}); *r = {21, 42, 341, 292}; return ret("GetWindowRect");
}
int32_t MoveWindow(GrafPort* w, int32_t x, int32_t y, int32_t a, int32_t b, int32_t repaint) {
    return answer("MoveWindow", {lab(w), x, y, a, b, repaint});
}
}
const NS_MacDoze::_GUID IID_IDirectDraw2{}, IID_IDirectDraw4{};
int32_t DirectDrawCreate(NS_MacDoze::_GUID* guid, IDirectDraw** out, NS_MacDoze::IUnknown* unk) {
    if (guid) {
        char hex[33];
        for (int i = 0; i < 16; ++i)
            std::snprintf(hex + i * 2, 3, "%02x", guid->bytes[i]);
        ev("DirectDrawCreate", {lab(unk)}, {hex});
    } else ev("DirectDrawCreate", {lab(unk)}, {"(null)"});
    const int32_t result = ret("DirectDrawCreate"); if (!result) *out = &dd1;
    return result;
}
int32_t DirectDrawEnumerateA(int32_t (*)(NS_MacDoze::_GUID*, char*, char*, void*), void* p) {
    // Enumeration owns the callback. Populate its output deterministically on both sides.
    ev("Enumerate", {lab(p)});
    auto* d = static_cast<CDirectDraw*>(p);
    d->mDeviceCount = ret("devices", 4);
    // Two GUIDs compare equal as C strings but differ after their NUL byte.
    const uint8_t bytes[4][16] = {{9,1}, {3,0,2}, {3,0,8}, {1,5}};
    const int32_t order = ret("device_order");
    for (int i = 0; i < 4; ++i)
        std::memcpy(&d->mDevices[i], bytes[order ? 3-i : i], 16);
    return ret("Enumerate");
}
int32_t IDirectDraw::CreateClipper(uint32_t f, IDirectDrawClipper** out, NS_MacDoze::IUnknown* u) {
    const int32_t result = answer("CreateClipper", {lab(this), f, lab(u)});
    if (!result) *out = &clip; return result;
}
int32_t IDirectDraw::SetCooperativeLevel(GrafPort* w, uint32_t flags) {
    return answer("SetCooperativeLevel", {lab(this), lab(w), flags});
}
int32_t IDirectDraw::SetDisplayMode(uint32_t w, uint32_t h, uint32_t b, uint32_t x, uint32_t y) {
    return answer("SetDisplayMode", {lab(this), w, h, b, x, y});
}
int32_t IDirectDraw::RestoreDisplayMode() { return answer("RestoreDisplayMode", {lab(this)}); }
int32_t IDirectDraw::GetCaps(DDCAPS_DX6* h, DDCAPS_DX6* s) {
    const auto* hw = reinterpret_cast<uint32_t*>(h);
    const auto* sw = reinterpret_cast<uint32_t*>(s);
    uint32_t dirty = 0; for (int i = 1; i < 95; ++i) dirty |= hw[i] | sw[i];
    const int32_t result = answer("GetCaps", {lab(this), h->dwSize, s->dwSize, dirty});
    if (!result) {
        for (int i = 1; i < 95; ++i) { reinterpret_cast<uint32_t*>(h)[i] = i * 37; reinterpret_cast<uint32_t*>(s)[i] = i * 53; }
    }
    return result;
}
int32_t IDirectDraw4::GetDeviceIdentifier(DDDEVICEIDENTIFIER* d, uint32_t flags) {
    const int32_t result = answer("GetDeviceIdentifier", {lab(this), flags});
    if (!result) {
        std::memset(d, 0, sizeof *d); std::strcpy(d->driver, "fake-driver"); std::strcpy(d->description, "fake-description");
        d->driverVersionHigh = 0x12345678; d->driverVersionLow = 0x98765432;
        d->vendorID = 0x1002; d->deviceID = 0x4742;
    }
    return result;
}
int32_t IDirectDraw4::GetDisplayMode(DDSURFACEDESC2* d) { int32_t result; getMode("GetDisplayMode", this, d, result); return result; }
int32_t IDirectDraw4::CreateSurface(DDSURFACEDESC2* d, IDirectDrawSurface4** out, NS_MacDoze::IUnknown* u) {
    ev("CreateSurface", {lab(this), size32(d->dwSize), d->dwFlags, d->dwHeight, d->dwWidth,
                          d->dwBackBufferCount, d->ddsCaps.dwCaps, lab(u)});
    const int32_t result = ret("CreateSurface"); if (!result) *out = (d->ddsCaps.dwCaps == 0x6040) ? &back : &primary;
    return result;
}
int32_t IDirectDrawClipper::SetHWnd(uint32_t f, GrafPort* w) { return answer("SetHWnd", {lab(this), f, lab(w)}); }
int32_t IDirectDrawSurface::SetClipper(IDirectDrawClipper* c) { return answer("SetClipper", {lab(this), lab(c)}); }
int32_t IDirectDrawSurface::Restore() { return answer("Restore", {lab(this)}); }
int32_t IDirectDrawSurface::Unlock(void* r) { return answer("Unlock", {lab(this), lab(r)}); }
int32_t IDirectDrawSurface3::GetSurfaceDesc(DDSURFACEDESC2* d) { int32_t result; getMode("GetSurfaceDesc", this, d, result); return result; }
int32_t IDirectDrawSurface4::GetAttachedSurface(DDSCAPS2* c, IDirectDrawSurface4** out) {
    const int32_t result = answer("GetAttachedSurface", {lab(this), c->dwCaps}); if (!result) *out = &back; return result;
}
int32_t IDirectDrawSurface4::Blt(SamsUtilities::URectangle::LongRect* d, IDirectDrawSurface4* s,
                               SamsUtilities::URectangle::LongRect* r, uint32_t flags, DDBLTFX* fx) {
    uint32_t dirty = 0; for (auto w : fx->words) dirty |= w;
    return answer("Blt", {lab(this), lab(d), lab(s), lab(r), flags, fx->dwSize, dirty});
}
int32_t IDirectDrawSurface4::Lock(SamsUtilities::URectangle::LongRect* r, DDSURFACEDESC2* d,
                                uint32_t flags, NS_MacDoze::InternalHandle* event) {
    const int32_t result = answer("Lock", {lab(this), lab(r), size32(d->dwSize), flags, lab(event)});
    if (!result) fillDesc(d); return result;
}
int32_t ShiftFactor(uint32_t mask) { return answer("ShiftFactor", {mask}); }
int32_t BitsSet(uint32_t mask) { return answer("BitsSet", {mask}); }
void CDisplay::SetDisplay(int32_t w, int32_t h, int32_t b) {
    ev("SetDisplay", {w, h, b}); mWidth = w; mHeight = h; mBitsPerPixel = b;
}
void CDisplay::SetMaxBits() { ev("SetMaxBits"); }
void CDisplay::SetBitPattern(SBitData a, SBitData r, SBitData g, SBitData b) {
    ev("SetBitPattern", {a.mMask, a.mShift, a.mBits, r.mMask, r.mShift, r.mBits,
                         g.mMask, g.mShift, g.mBits, b.mMask, b.mShift, b.mBits});
}

namespace SamsUtilities {
ULogManager* gLogger;
UAllocator* gMemory;
ULogManager::ULogManager() { if (on()) ev("LoggerCtor", {newlab(this)}); }
void ULogManager::SetContext(const char* area, const char* file, int32_t line, uint32_t flags) {
    ev("SetContext", {lab(this), line, flags}, {area, file});
}
void ULogManager::Log(const char* format, ...) {
    char text[512]; va_list ap; va_start(ap, format); std::vsnprintf(text, sizeof text, format, ap); va_end(ap);
    ev("Log", {lab(this)}, {text});
}
void UDebug(const char* format, ...) {
    char text[512]; va_list ap; va_start(ap, format); std::vsnprintf(text, sizeof text, format, ap); va_end(ap);
    ev("UDebug", {}, {text});
}
UAllocator::UAllocator() { if (on()) ev("AllocatorCtor", {newlab(this)}); }
UMemoryAlignedBase::UMemoryAlignedBase(uint32_t size, uint32_t alignment, uint32_t flags, UAllocator* a) {
    ev("AlignedCtor", {newlab(this), size, alignment, flags, lab(a)}); mData = ptr(ret("data", 19));
}
UMemoryAlignedBase::~UMemoryAlignedBase() { ev("AlignedDtor", {lab(this)}); }
int32_t UOpenGL::GetMaxTextures(int32_t a, int32_t b, int32_t c) { return answer("GetMaxTextures", {lab(this), a, b, c}); }
}
IdxDirectDraw::~IdxDirectDraw() {}
namespace {
class Implementation : public IdxDirectDraw {
public:
    SamsUtilities::UOpenGL* DisplayGetContext() override { ev("DisplayGetContext"); return &context; }
} implementation;
}
IdxDirectDraw* IdxDirectDraw::mImplementation = &implementation;

#define GL1(n,t) void n(t a) { ev(#n, {a}); }
GL1(glPushAttrib, GLbitfield) GL1(glDepthMask, uint8_t) GL1(glClear, GLbitfield)
GL1(glEnable, GLenum) GL1(glDisable, GLenum) GL1(glDepthFunc, GLenum)
GL1(glEnableClientState, GLenum) GL1(glMatrixMode, GLenum) GL1(glFrontFace, GLenum)
void glPopAttrib() { ev("glPopAttrib"); }
void glLoadIdentity() { ev("glLoadIdentity"); }
void glClearColor(float a, float b, float c, float d) { ev("glClearColor", {fbits(a), fbits(b), fbits(c), fbits(d)}); }
void glViewport(int32_t x, int32_t y, int32_t w, int32_t h) { ev("glViewport", {x, y, w, h}); }
GLenum glGetError() { return answer("glGetError"); }

// Only start-up of the primitive object belongs to this unit.
CDrawPrimitive::CDrawPrimitive() {
    if (!skipBase) ev("PrimitiveCtor", {newlab(this)});
    for (int i = 0; i < 11; ++i) mFields[i] = 0x55aa0000 + i;
}
CDrawPrimitive::~CDrawPrimitive() {}
COpenGLDrawPrimitive::~COpenGLDrawPrimitive() {
    auto* base=static_cast<CDrawPrimitive*>(this);
    ev("PrimitiveDtor", {lab(base)});
    if(lab(base)>0) { ev("delete", {lab(base)}); forget(base); }
}
int32_t COpenGLDrawPrimitive::Init() { return answer("PrimitiveInit", {lab(static_cast<CDrawPrimitive*>(this))}); }
// Unused virtual slots still need definitions so the native vtable links.
int32_t COpenGL::FlipScreen() { return 0; }
int32_t COpenGL::GetVRAMSize() { return 0; }
int32_t COpenGL::GetTextureMemory() { return 0; }
void* COpenGL::CreateTextureBase() { return nullptr; }
int32_t COpenGLDrawPrimitive::SetBlendingMode(int32_t) { return 0; }
int32_t COpenGLDrawPrimitive::SetTexture(int32_t, void*) { return 0; }
int32_t COpenGLDrawPrimitive::Execute(SPrimitive*) { return 0; }
int32_t COpenGLDrawPrimitive::ExecuteShadow(SPrimitive*) { return 0; }
int32_t COpenGLDrawPrimitive::SetFogParameters(float, float, uint32_t) { return 0; }
int32_t COpenGLDrawPrimitive::BeginDraw() { return 0; }
int32_t COpenGLDrawPrimitive::EndDraw() { return 0; }
int32_t COpenGLDrawPrimitive::DrawTestPoly() { return 0; }
int32_t COpenGLDrawPrimitive::DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawEdgeBox() { return 0; }
int32_t COpenGLDrawPrimitive::DrawPoly(XZType*, int32_t, int32_t, float) { return 0; }
int32_t COpenGLDrawPrimitive::DrawProfileBox() { return 0; }
float COpenGLDrawPrimitive::GetPixelFillRate() { return 0.0f; }
int32_t COpenGLDrawPrimitive::DrawWiredframedBoundingBox(SVertex*, int32_t, int32_t) { return 0; }
int32_t COpenGLDrawPrimitive::GetPixel(int32_t, int32_t, uint32_t*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawDetectPoly(float, int32_t, int32_t, float, float, float, float) { return 0; }

extern "C" {
void* tpw_ds_draw_base() { return static_cast<CDrawPrimitive*>(reinterpret_cast<COpenGLDrawPrimitive*>(drawStore)); }
void* tpw_ds_pointer(int32_t n) { return ptr(n); }
void* tpw_ds_object(int32_t n) {
    switch(n) { case 1: return ddStore; case 2: return glStore; case 3: return drawStore; case 4: return memStore;
                case 20: return &oldLogger; case 21: return &oldAllocator; case 22: return &context;
                default: return clipStore; }
}
void tpw_ds_reset(const uint32_t* dd, const uint32_t* state, const uint32_t* desc) {
    // Fresh raw storage avoids destructor activity between cases.
    std::memset(ddStore, 0xcd, sizeof ddStore);
    auto* d = reinterpret_cast<CDirectDraw*>(ddStore);
    d->mDirectDraw1 = static_cast<IDirectDraw*>(ptr(dd[0])); d->mDirectDraw2 = static_cast<IDirectDraw*>(ptr(dd[1]));
    d->mDirectDraw = static_cast<IDirectDraw4*>(ptr(dd[2])); d->mClipper = static_cast<IDirectDrawClipper*>(ptr(dd[3]));
    d->mPrimary = static_cast<IDirectDrawSurface4*>(ptr(dd[4])); d->mBack = static_cast<IDirectDrawSurface4*>(ptr(dd[5]));
    d->mSurface18 = static_cast<NS_MacDoze::IUnknown*>(ptr(dd[6]));
    auto* tail = reinterpret_cast<uint32_t*>(&d->mDeviceInfo);
    for (int i = 7; i < 0x78c / 4; ++i) {
        const bool bytes = i < 0x41c / 4 || (i >= 0x434 / 4 && i < 0x484 / 4);
        if (bytes) beput(tail + i - 7, dd[i]); else tail[i - 7] = dd[i];
    }
    new (glStore) COpenGL; reinterpret_cast<COpenGL*>(glStore)->mMode = state[7];
    skipBase = 1; new (drawStore) COpenGLDrawPrimitive; skipBase = 0;
    auto* mem = reinterpret_cast<SamsUtilities::UMemoryAlignedBase*>(memStore); mem->mData = ptr(19);
    gei_DDraw = state[0] ? d : nullptr;
    gei_OpenGL = state[1] ? reinterpret_cast<COpenGL*>(glStore) : nullptr;
    gOpenGLDrawPrimitive = state[2] ? static_cast<CDrawPrimitive*>(reinterpret_cast<COpenGLDrawPrimitive*>(drawStore)) : nullptr;
    gei_DrawPrimitive = reinterpret_cast<CDrawPrimitive*>(ptr(19));
    // Close wrappers must own storage which the real delete may free.
    if (!clipStore) clipStore = static_cast<CClipping*>(std::malloc(sizeof(CClipping)));
    gei_Clipping = state[3] ? clipStore : nullptr;
    gOpenGLTextureMemory = state[4] ? mem : nullptr; gOpenGLTextureData = ptr(state[5]);
    gei_System.mWindow = reinterpret_cast<GrafPort*>(0x4567); gei_System.mRenderer = state[6];
    gei_System.mRenderSystem = reinterpret_cast<COpenGL*>(glStore);
    gei_System.mFlags = state[8]; gDDResult = state[9];
    gei_Display.mWidth = state[10]; gei_Display.mHeight = state[11]; gei_Display.mBitsPerPixel = state[12];
    gei_Display.mFlags = state[13]; gei_Display.mRedMask = 101; gei_Display.mGreenMask = 102;
    gei_Display.mBlueMask = 103; gei_Display.mRGBMask = 104;
    SamsUtilities::gLogger = state[14] ? &oldLogger : nullptr;
    SamsUtilities::gMemory = state[15] ? &oldAllocator : nullptr;
    std::memcpy(descriptor, desc, sizeof descriptor);
    for (int i = 0; i < 12; ++i) gOpenGLBlendModes[i] = state[16] + i;
}
void tpw_ds_dd_snapshot(uint32_t* w, int32_t global) {
    auto* d = global ? gei_DDraw : reinterpret_cast<CDirectDraw*>(ddStore); if (!d) return;
    w[0]=lab(d->mDirectDraw1); w[1]=lab(d->mDirectDraw2); w[2]=lab(d->mDirectDraw); w[3]=lab(d->mClipper);
    w[4]=lab(d->mPrimary); w[5]=lab(d->mBack); w[6]=lab(d->mSurface18);
    const auto* tail = reinterpret_cast<const uint32_t*>(&d->mDeviceInfo);
    for (int i=7; i<0x78c/4; ++i) {
        const bool bytes = i < 0x41c/4 || (i >= 0x434/4 && i < 0x484/4);
        w[i] = bytes ? beword(tail+i-7) : tail[i-7];
    }
}
void tpw_ds_state(uint32_t* s) {
    s[0]=gei_DDraw!=nullptr; s[1]=gei_OpenGL!=nullptr; s[2]=gOpenGLDrawPrimitive!=nullptr; s[3]=gei_Clipping!=nullptr;
    s[4]=gOpenGLTextureMemory!=nullptr; s[5]=lab(gOpenGLTextureData); s[6]=gei_System.mRenderer;
    s[7]=reinterpret_cast<COpenGL*>(glStore)->mMode; s[8]=gei_System.mFlags; s[9]=gDDResult;
    s[10]=gei_Display.mWidth; s[11]=gei_Display.mHeight; s[12]=gei_Display.mBitsPerPixel; s[13]=gei_Display.mFlags;
    s[14]=SamsUtilities::gLogger!=nullptr; s[15]=SamsUtilities::gMemory!=nullptr;
    s[16]=gei_Display.mRedMask; s[17]=gei_Display.mGreenMask; s[18]=gei_Display.mBlueMask; s[19]=gei_Display.mRGBMask;
    s[20]=gei_System.mRenderSystem!=nullptr; s[21]=gei_System.mWindow==reinterpret_cast<GrafPort*>(0x4567);
    if (gei_OpenGL) s[22]=gei_OpenGL->mMode;
    s[23] = lab(gei_DrawPrimitive);
}
int32_t tpw_ds_call(int32_t fn, const int32_t* a, uint32_t* out) {
    auto* d = reinterpret_cast<CDirectDraw*>(ddStore);
    auto* g = reinterpret_cast<COpenGL*>(glStore);
    switch (fn) {
    case 0: return DDraw_Init(reinterpret_cast<GrafPort*>(0x4567), a[0], a[1], a[2], a[3], a[4]);
    case 1:
        if (gei_DDraw == d) {
            gei_DDraw = heapCopy<CDirectDraw>(d);
            label(1, gei_DDraw);
        }
        return DDraw_Close();
    case 2: new (d) CDirectDraw; return 0;
    case 3:
        d->~CDirectDraw();
        if (int16_t(a[0]) > 0) ev("delete", {1});
        return 0;
    case 4: return d->Init(a[0], a[1], a[2], a[3], a[4], a[5]);
    case 5: return d->SetupCaps();
    case 6: return d->Close();
    case 7: return d->TempShutdown();
    case 8: return d->ReInitialize();
    case 9: descWords(d->LockBackbuffer(), out); return 0;
    case 10: return d->UnlockBackbuffer();
    case 11: return OpenGL_Init(a[0]);
    case 12:
        if (gei_OpenGL == g) {
            gei_OpenGL = new COpenGL;
            gei_OpenGL->mMode = g->mMode;
            label(2, gei_OpenGL);
        }
        return OpenGL_Close();
    case 13: new (g) COpenGL; return 0;
    case 14: return g->Init(a[0]);
    case 15: return g->Close();
    case 16: return g->TempShutdown();
    case 17: return g->ReInitialize();
    case 18: return g->InitBackground();
    case 19: return g->InitRenderStates();
    case 20: return g->MaxTextures(a[0], a[1]);
    case 21: COpenGLTexture::Initialise(); return 0;
    case 22:
        if (gOpenGLTextureMemory == reinterpret_cast<SamsUtilities::UMemoryAlignedBase*>(memStore)) {
            gOpenGLTextureMemory = heapCopy<SamsUtilities::UMemoryAlignedBase>(memStore);
            label(4, gOpenGLTextureMemory);
        }
        COpenGLTexture::Cleanup();
        return 0;
    case 23: {
        COpenGLPrintf p;
        std::memset(&p, 0xcd, sizeof p);
        p.printf_xy(a[0], a[1], static_cast<char*>(ptr(a[2])));
        std::memcpy(out, p.mOpaque, sizeof p.mOpaque);
        out[4] = lab(p.mText); out[5] = p.mX; out[6] = p.mY;
        return 0;
    }
    case 24: return DrawPrimitiveOpenGL_Init();
    case 25:
        if (gOpenGLDrawPrimitive) {
            gOpenGLDrawPrimitive = heapCopy<COpenGLDrawPrimitive>(drawStore);
            label(3, gOpenGLDrawPrimitive);
        }
        if (gOpenGLTextureMemory) {
            gOpenGLTextureMemory = heapCopy<SamsUtilities::UMemoryAlignedBase>(memStore);
            label(4, gOpenGLTextureMemory);
        }
        return DrawPrimitiveOpenGL_Close();
    case 26:
        skipBase = int16_t(a[0]) == 0;
        new (drawStore) COpenGLDrawPrimitive;
        skipBase = 0;
        return 0;
    case 27: return Clipping_Init();
    case 28: {
        const bool exists = gei_Clipping;
        auto result = Clipping_Close();
        if (exists) clipStore = nullptr;
        return result;
    }
    case 29: return g->ClearViewport();
    case 30: return CDirectDraw::FindDeviceCallback(a[0] ? &d->mDevices[0] : nullptr, nullptr, nullptr, d);
    }
    return -999;
}
void tpw_ds_draw_snapshot(uint32_t* out) {
    auto* p=reinterpret_cast<COpenGLDrawPrimitive*>(drawStore);
    out[0]=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p->mTexture));out[1]=p->mPasses;out[2]=p->mF10;
    for(int i=0;i<11;++i)out[3+i]=p->mFields[i];
    for(int i=0;i<12;++i)out[14+i]=gOpenGLBlendModes[i];
}
}
