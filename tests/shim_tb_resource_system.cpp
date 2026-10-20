// extern "C" wrappers for tests/test_tb_resource_system.py (ctypes): engine
// tb_resource_system.cpp and CErrorHandler::GetAltTabStatus.
//
// CSystem holds two pointers, so the test reads and writes its fields through accessors.
// The renderer is a test CRenderSystem that logs ClearViewport and FlipScreen (with the
// object they were called on) and answers FlipScreen from a list the test sets.
#include <cstring>
#include <new>

#include "MapWho.h"
#include "tb_error.h"
#include "tb_resource_system.h"

extern "C" {
int32_t tpw_rs_log_n = 0;
int32_t tpw_rs_log[32];         // 1 ClearViewport, 2 FlipScreen
int32_t tpw_rs_log_ok[32];      // called on the installed object
int32_t tpw_rs_flips[8];        // FlipScreen's answers in order (then the last again)
int32_t tpw_rs_nflips = 0;
}

namespace {

class TestRenderSystem : public CRenderSystem {
public:
    int32_t ClearViewport() override { note(1); return 1; }
    int32_t FlipScreen() override {
        note(2);
        int32_t i = mFlip < tpw_rs_nflips ? mFlip : tpw_rs_nflips - 1;
        mFlip++;
        return i >= 0 ? tpw_rs_flips[i] : 0;
    }
    int32_t TempShutdown() override { return 0; }
    int32_t ReInitialize() override { return 0; }
    int32_t GetTextureMemory() override { return 0; }
    int32_t GetVRAMSize() override { return 0; }
    void* CreateTextureBase() override { return nullptr; }

    int32_t mFlip = 0;

private:
    void note(int32_t kind) {
        if (tpw_rs_log_n >= 32) return;
        tpw_rs_log[tpw_rs_log_n] = kind;
        tpw_rs_log_ok[tpw_rs_log_n] = this == gei_System.mRenderSystem ? 1 : 0;
        tpw_rs_log_n++;
    }
};

TestRenderSystem gTestRenderSystem;
alignas(CSystem) unsigned char gSystemStore[sizeof(CSystem)];

}  // namespace

extern "C" {

void tpw_rs_reset() {
    tpw_rs_log_n = 0;
    gTestRenderSystem.mFlip = 0;
}

CSystem* tpw_system_global() { return &gei_System; }
CSystem* tpw_system_store() { return reinterpret_cast<CSystem*>(gSystemStore); }

// Scalar fields in original order: flags, light frame, 0x08, 0x0c, clocks per ms, total
// physical memory, renderer, 0x20.
void tpw_system_scalars(CSystem* s, uint32_t* v, int32_t write) {
    uint32_t* f[] = {&s->mFlags, reinterpret_cast<uint32_t*>(&s->mLightFrame), reinterpret_cast<uint32_t*>(&s->mF08),
                     reinterpret_cast<uint32_t*>(&s->mF0C), reinterpret_cast<uint32_t*>(&s->mClocksPerMs),
                     &s->mTotalPhys, reinterpret_cast<uint32_t*>(&s->mRenderer), reinterpret_cast<uint32_t*>(&s->mF20)};
    for (int32_t i = 0; i < 8; i++) {
        if (write != 0) *f[i] = v[i];
        else v[i] = *f[i];
    }
}
void* tpw_system_window(CSystem* s, void* window, int32_t write) {
    if (write != 0) s->mWindow = static_cast<GrafPort*>(window);
    return s->mWindow;
}
// Render system: 1 = the test renderer, 0 = NULL (in and out); anything else reads as -1.
int32_t tpw_system_renderer(CSystem* s, int32_t which, int32_t write) {
    if (write != 0) s->mRenderSystem = which == 1 ? &gTestRenderSystem : nullptr;
    if (s->mRenderSystem == nullptr) return 0;
    return s->mRenderSystem == &gTestRenderSystem ? 1 : -1;
}
void* tpw_system_cpu(CSystem* s) { return &s->mCPU; }
void tpw_mapwho_set(uintptr_t value) { gei_MapWho = reinterpret_cast<CMapWho*>(value); }

void tpw_CSystem_OutputDebugInfo(CSystem* s) { s->OutputDebugInfo(); }
void tpw_CSystem_UpdateLights(CSystem* s) { s->UpdateLights(); }
int32_t tpw_RenderSystem_Init(int32_t mode) { return RenderSystem_Init(mode); }
int32_t tpw_RenderSystem_Close() { return RenderSystem_Close(); }
void tpw_RenderSystem_ClearViewport() { RenderSystem_ClearViewport(); }
int32_t tpw_RenderSystem_FlipScreen() { return RenderSystem_FlipScreen(); }
void tpw_RenderSystem_Render() { RenderSystem_Render(); }
int32_t tpw_RenderSystem_Flush() { return RenderSystem_Flush(); }
void tpw_RenderSystem_FrontLink(uintptr_t link, int32_t list) {
    RenderSystem_FrontLink(reinterpret_cast<CMapLinkBase*>(link), list);
}
void tpw_RenderSystem_FrontUnlink(uintptr_t link, int32_t list) {
    RenderSystem_FrontUnlink(reinterpret_cast<CMapLinkBase*>(link), list);
}
void tpw_RenderSystem_FrontReset(int32_t list) { RenderSystem_FrontReset(list); }
void tpw_RenderSystem_MapLink(int32_t a, int32_t b, int32_t c, int32_t d, uintptr_t link, int32_t list) {
    RenderSystem_MapLink(a, b, c, d, reinterpret_cast<CMapLinkBase*>(link), list);
}
void tpw_RenderSystem_MapUnLink(int32_t a, int32_t b, int32_t c, int32_t d, uintptr_t link) {
    RenderSystem_MapUnLink(a, b, c, d, reinterpret_cast<CMapLinkBase*>(link));
}
void tpw_RenderSystem_Reset(int32_t a, int32_t b, int32_t c, int32_t d) { RenderSystem_Reset(a, b, c, d); }
void tpw_RenderSystem_ResetAll() { RenderSystem_ResetAll(); }
void tpw_sinit_tb_resource_system() { new (&gei_System) CSystem(); }
int32_t tpw_CErrorHandler_GetAltTabStatus(CErrorHandler* h) { return h->GetAltTabStatus(); }

}  // extern "C"
