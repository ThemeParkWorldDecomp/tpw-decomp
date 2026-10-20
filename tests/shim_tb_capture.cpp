// extern "C" wrappers for tests/test_tb_capture.py (ctypes): engine tb_capture.cpp, the
// tb_error.cpp pieces (CErrorHandler::Push and Pop, SError, the static initialiser) and
// the Wavelet.cpp engine callbacks (DefaultMalloc, DefaultFree, SetEngineCallbacks).
//
// The CCapture under test lives in static storage here; the test reads and writes its
// pointer fields through accessors (the host layout differs from the PowerPC one). The
// engine allocator callbacks are test fakes: new blocks are filled with 0xcd, and the
// free callback records the block and clears the caller's pointer, as the game's MacFree
// does, but keeps the memory so the test can still read it (tpw_cap_release frees it).
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Wavelet.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_error.h"

namespace {

alignas(CCapture) unsigned char gCaptureStore[sizeof(CCapture)];
CDirectDraw gFakeDDraw;
unsigned char gFakeSurfaceObject[16];   // stands in for the IDirectDrawSurface4 object

}  // namespace

extern "C" {

// ---- allocator fakes: kind 1 = malloc (size, block), 2 = free (block)
int32_t tpw_cap_malloc_fail_at = -1;   // index of the malloc call that returns NULL
int32_t tpw_cap_log_n = 0;
int32_t tpw_cap_log_kind[64];
uint64_t tpw_cap_log_size[64];
void* tpw_cap_log_ptr[64];
int32_t tpw_cap_mallocs = 0;
void* tpw_cap_blocks[64];
int32_t tpw_cap_nblocks = 0;

static void cap_log(int32_t kind, uint64_t size, void* p) {
    if (tpw_cap_log_n >= 64) return;
    tpw_cap_log_kind[tpw_cap_log_n] = kind;
    tpw_cap_log_size[tpw_cap_log_n] = size;
    tpw_cap_log_ptr[tpw_cap_log_n] = p;
    tpw_cap_log_n++;
}

static void* cap_malloc(uint32_t size) {
    int32_t index = tpw_cap_mallocs++;
    if (index == tpw_cap_malloc_fail_at) {
        cap_log(1, size, nullptr);
        return nullptr;
    }
    // plain malloc, as the game's MacMalloc: the blocks stay out of operator new's log
    unsigned char* p = static_cast<unsigned char*>(std::malloc(size != 0 ? size : 1));
    std::memset(p, 0xcd, size);
    if (tpw_cap_nblocks < 64) tpw_cap_blocks[tpw_cap_nblocks++] = p;
    cap_log(1, size, p);
    return p;
}

int32_t tpw_cap_free_clears = 1;   // 0: like the engine's DefaultFree, the pointer stays

static void cap_free(void*& block) {
    cap_log(2, 0, block);
    if (tpw_cap_free_clears != 0) block = nullptr;
}

void tpw_cap_reset() {
    tpw_cap_log_n = 0;
    tpw_cap_mallocs = 0;
    tpw_cap_malloc_fail_at = -1;
    tpw_cap_free_clears = 1;
}
void tpw_cap_install() {
    gMalloc = cap_malloc;
    gFree = cap_free;
}
void tpw_cap_release() {
    for (int32_t i = 0; i < tpw_cap_nblocks; i++) std::free(tpw_cap_blocks[i]);
    tpw_cap_nblocks = 0;
}

// ---- the capture object and the DirectDraw stand-ins
CCapture* tpw_capture_store() { return reinterpret_cast<CCapture*>(gCaptureStore); }
void tpw_capture_set(CCapture* c, void* screen, void* rgb, int32_t pixels, const char* name, int32_t pending) {
    c->mScreen = screen;
    c->mRGB = rgb;
    c->mPixels = pixels;
    std::memcpy(c->mName, name, sizeof c->mName);
    // pending: -1 = NULL, otherwise an offset into mName
    c->mPending = pending < 0 ? nullptr : c->mName + pending;
}
void tpw_capture_get(const CCapture* c, void** screen, void** rgb, int32_t* pixels, char* name, int32_t* pending) {
    *screen = c->mScreen;
    *rgb = c->mRGB;
    *pixels = c->mPixels;
    std::memcpy(name, c->mName, sizeof c->mName);
    *pending = c->mPending == nullptr ? -1 : static_cast<int32_t>(c->mPending - c->mName);
}
void* tpw_fake_surface() { return gFakeSurfaceObject; }
void tpw_ddraw_install(int32_t has_ddraw) {
    gFakeDDraw.mBack = reinterpret_cast<IDirectDrawSurface4*>(gFakeSurfaceObject);
    gei_DDraw = has_ddraw != 0 ? &gFakeDDraw : nullptr;
}
int32_t* tpw_ddresult() { return &gDDResult; }

// ---- tb_capture.cpp
int32_t tpw_CCapture_SaveUncompressedTGA(CCapture* c, char* name) { return c->SaveUncompressedTGA(name); }
int32_t tpw_CCapture_ConvertCapture(CCapture* c) { return c->ConvertCapture(); }
int32_t tpw_CCapture_GetCapture(CCapture* c) { return c->GetCapture() ? 1 : 0; }
int32_t tpw_CCapture_Capture(CCapture* c) { return c->Capture(); }
int32_t tpw_CCapture_SetCapture(CCapture* c, char* name) { return c->SetCapture(name); }
void tpw_sinit_tb_capture() { new (&gei_Capture) CCapture(); }
CCapture* tpw_gei_Capture() { return &gei_Capture; }

// ---- tb_error.cpp
void tpw_CErrorHandler_Push(CErrorHandler* h, int32_t level, int32_t code) { h->Push(level, code); }
// The popped entry as an offset into the handler (-1 for NULL).
int32_t tpw_CErrorHandler_Pop(CErrorHandler* h) {
    SError* e = h->Pop();
    return e == nullptr ? -1 : static_cast<int32_t>(reinterpret_cast<char*>(e) - reinterpret_cast<char*>(h));
}
void tpw_SError_ctor(void* storage) { new (storage) SError(); }
void tpw_sinit_tb_error() { new (&gei_ErrorHandler) CErrorHandler(); }

// ---- Wavelet.cpp
void* tpw_DefaultMalloc(uint32_t size) { return DefaultMalloc(size); }
void tpw_DefaultFree(void** block) { DefaultFree(*block); }
void tpw_SetEngineCallbacks(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e, uintptr_t f) {
    SetEngineCallbacks(reinterpret_cast<TbIsFirstPersonFn>(a), reinterpret_cast<TbUseSmallTexturesFn>(b),
                       reinterpret_cast<TbReadClockFn>(c), reinterpret_cast<TbUpdateMouseFn>(d),
                       reinterpret_cast<TbMallocFn>(e), reinterpret_cast<TbFreeFn>(f));
}
void tpw_get_engine_callbacks(uint64_t* out) {
    out[0] = reinterpret_cast<uintptr_t>(gIsFirstPerson);
    out[1] = reinterpret_cast<uintptr_t>(gUseSmallTextures);
    out[2] = reinterpret_cast<uintptr_t>(gReadClock);
    out[3] = reinterpret_cast<uintptr_t>(gUpdateMouse);
    out[4] = reinterpret_cast<uintptr_t>(gMalloc);
    out[5] = reinterpret_cast<uintptr_t>(gFree);
}

}  // extern "C"
