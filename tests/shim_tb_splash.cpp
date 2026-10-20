// extern "C" wrappers for tests/test_tb_splash.py (ctypes): engine tb_splash.cpp.
//
// The CSplash under test lives in static storage (gei_Splash for the static initialiser).
// gei_DrawPrimitive is a test object that logs BeginDraw, EndDraw and DrawFilledBox (with
// its arguments) and answers DrawFilledBox from a list.
#include <cstdlib>
#include <cstring>
#include <new>

#include "tb_drawprimitive.h"
#include "tb_splash.h"

extern "C" {
int32_t tpw_dp_n = 0;
int32_t tpw_dp_kind[32];      // 1 BeginDraw, 2 EndDraw, 3 DrawFilledBox
float tpw_dp_box[32][4];
int32_t tpw_dp_colour[32];
uint64_t tpw_dp_texture[32];
int32_t tpw_dp_results[8];
int32_t tpw_dp_nresults = 0;
}

namespace {

class TestDrawPrimitive : public CDrawPrimitive {
public:
    int32_t Init() override { return 1; }
    int32_t SetBlendingMode(int32_t) override { return 1; }
    int32_t SetTexture(int32_t, void*) override { return 1; }
    int32_t Execute(SPrimitive*) override { return 1; }
    int32_t ExecuteShadow(SPrimitive*) override { return 1; }
    int32_t SetFogParameters(float, float, uint32_t) override { return 1; }
    int32_t BeginDraw() override {
        note(1);
        return 1;
    }
    int32_t EndDraw() override {
        note(2);
        return 1;
    }
    int32_t DrawTestPoly() override { return 1; }
    int32_t DrawFilledBox(float x1, float y1, float x2, float y2, int32_t colour, CTextureBase* texture) override {
        int32_t i = tpw_dp_n;
        note(3);
        if (i < 32) {
            tpw_dp_box[i][0] = x1;
            tpw_dp_box[i][1] = y1;
            tpw_dp_box[i][2] = x2;
            tpw_dp_box[i][3] = y2;
            tpw_dp_colour[i] = colour;
            tpw_dp_texture[i] = reinterpret_cast<uintptr_t>(texture);
        }
        int32_t k = mBoxes++;
        if (tpw_dp_nresults == 0) return 1;
        return tpw_dp_results[k < tpw_dp_nresults ? k : tpw_dp_nresults - 1];
    }
    int32_t mBoxes = 0;

private:
    void note(int32_t kind) {
        if (tpw_dp_n < 32) tpw_dp_kind[tpw_dp_n] = kind;
        tpw_dp_n++;
    }
};

TestDrawPrimitive gTestDraw;

// gei_Splash's destructor runs at exit and would wait out the first splash on the fake
// clock: start the test library with the first splash already over.
struct NoFirstSplash {
    NoFirstSplash() { gSplashFirst = 0; }
} gNoFirstSplash;
alignas(CSplash) unsigned char gSplashStore[sizeof(CSplash)];
alignas(CDrawToBackBuffer) unsigned char gBackStore[sizeof(CDrawToBackBuffer)];

}  // namespace

extern "C" {

void tpw_dp_reset() {
    tpw_dp_n = 0;
    gTestDraw.mBoxes = 0;
}
void tpw_dp_install(int32_t on) { gei_DrawPrimitive = on != 0 ? &gTestDraw : nullptr; }

CSplash* tpw_splash_store() { return reinterpret_cast<CSplash*>(gSplashStore); }
CSplash* tpw_splash_global() { return &gei_Splash; }
CDrawToBackBuffer* tpw_back_store() { return reinterpret_cast<CDrawToBackBuffer*>(gBackStore); }
CDrawToBackBuffer* tpw_new_back() { return new CDrawToBackBuffer; }
void* tpw_block(uint32_t n, const uint8_t* init) {
    void* p = std::malloc(n != 0 ? n : 1);
    if (init != nullptr) std::memcpy(p, init, n);
    return p;
}

// ints: frame, progress, frames, fill, f14, startMs, drawCount; scale separately
void tpw_splash_set(CSplash* s, const int32_t* v, float scale, CDrawToBackBuffer* back) {
    s->mFrame = v[0];
    s->mProgress = v[1];
    s->mFrames = v[2];
    s->mFill = v[3];
    s->mF14 = v[4];
    s->mStartMs = static_cast<uint32_t>(v[5]);
    s->mDrawCount = v[6];
    s->mScale = scale;
    s->mBackBuffer = back;
}
void tpw_splash_get(const CSplash* s, int32_t* v, float* scale, CDrawToBackBuffer** back) {
    v[0] = s->mFrame;
    v[1] = s->mProgress;
    v[2] = s->mFrames;
    v[3] = s->mFill;
    v[4] = s->mF14;
    v[5] = static_cast<int32_t>(s->mStartMs);
    v[6] = s->mDrawCount;
    *scale = s->mScale;
    *back = s->mBackBuffer;
}
void tpw_back_set(CDrawToBackBuffer* b, void* data, int32_t w, int32_t h, int32_t bpp) {
    b->mps_Data = data;
    b->mWidth = w;
    b->mHeight = h;
    b->mBytesPerPixel = bpp;
}
void tpw_back_get(const CDrawToBackBuffer* b, void** data, int32_t* whb) {
    *data = b->mps_Data;
    whb[0] = b->mWidth;
    whb[1] = b->mHeight;
    whb[2] = b->mBytesPerPixel;
}
int32_t* tpw_splash_first() { return &gSplashFirst; }
uint32_t* tpw_splash_start() { return &gSplashStartMs; }

int32_t tpw_CDrawToBackBuffer_dtor(CDrawToBackBuffer* b, int16_t flag) {
    b->~CDrawToBackBuffer();
    return flag > 0 ? 1 : 0;
}
int32_t tpw_CDrawToBackBuffer_Close(CDrawToBackBuffer* b) { return b->Close(); }
int32_t tpw_CDrawToBackBuffer_Overlay(CDrawToBackBuffer* b, char* name, int32_t x, int32_t y) {
    return b->Overlay(name, x, y);
}
int32_t tpw_CSplash_dtor(CSplash* s, int16_t flag) {
    s->~CSplash();
    return flag > 0 ? 1 : 0;
}
int32_t tpw_CSplash_Init(CSplash* s, char* language, char* path, int32_t frames) {
    return s->Init(language, path, frames);
}
int32_t tpw_CSplash_Close(CSplash* s) { return s->Close(); }
int32_t tpw_CSplash_WinProgressBar(CSplash* s) { return s->WinProgressBar(); }
int32_t tpw_CSplash_InterpolateSplash(CSplash* s) { return s->InterpolateSplash(); }
void tpw_sinit_tb_splash() { new (&gei_Splash) CSplash(); }

}  // extern "C"
