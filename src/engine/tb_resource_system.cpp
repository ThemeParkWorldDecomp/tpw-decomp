// tb_resource_system.cpp: name proven by __sinit_tb_resource_system_cpp in the engine library.

#include "MapWho.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "tb_error.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"

// tb_resource_system.cpp (decompiled from the engine library of SimTheme Park 2.04b3,
// functions at 0x100287a4..0x10029044).

CSystem gei_System;

CSystem::CSystem() {
    mCPU.detect();
    mFlags = 0x03000041;
    mFlags |= 0x100;
    mWindow = nullptr;
    mF20 = 1;
    mRenderSystem = nullptr;
    NS_MacDoze::_MEMORYSTATUS status;
    NS_MacDoze::GlobalMemoryStatus(&status);
    mTotalPhys = status.dwTotalPhys;
    mClocksPerMs = mCPU.clocks_per_millisec(1000);
    mRenderer = kRendererOpenGL;
    mLightFrame = -1;
    mF08 = 0;
    mF0C = 0;
}

void CSystem::OutputDebugInfo() {}

void CSystem::UpdateLights() {
    mLightFrame = gei_Display.mFrameCount;
    mFlags |= kLightsChanged;
}

int32_t RenderSystem_Init(int32_t mode) {
    if (gei_System.mRenderer == CSystem::kRendererOpenGL) return OpenGL_Init(mode);
    msl_assertion_failed("0", "tb_resource_system.cpp", 121);
    return 0;
}

int32_t RenderSystem_Close() {
    if (gei_System.mRenderer == CSystem::kRendererOpenGL) return OpenGL_Close();
    msl_assertion_failed("0", "tb_resource_system.cpp", 151);
    return 0;
}

void RenderSystem_ClearViewport() {
    gei_System.mRenderSystem->ClearViewport();
}

int32_t RenderSystem_FlipScreen() {
    if (FrontWindow() != gei_System.mWindow) return 1;   // in the background: nothing to show
    gei_Display.mFrameCount++;
    gei_Display.mF84 = 0;
    if (gei_System.mRenderSystem->FlipScreen() != 0) return 1;
    return gei_ErrorHandler.GetAltTabStatus();
}

void RenderSystem_Render() {
    gei_MapWho->Render();
}

int32_t RenderSystem_Flush() {
    uint32_t flags = gei_System.mFlags;
    gei_System.mFlags = flags & ~CSystem::kFlag00000001;
    for (int32_t i = 0; i < 3; i++) {   // unrolled in the original
        RenderSystem_ClearViewport();
        RenderSystem_FlipScreen();
    }
    gei_System.mFlags = flags;
    return 1;
}

void RenderSystem_FrontLink(CMapLinkBase* link, int32_t list) {
    gei_MapWho->Link(link, list);
}

void RenderSystem_FrontUnlink(CMapLinkBase* link, int32_t list) {
    gei_MapWho->Unlink(link, list);
}

void RenderSystem_FrontReset(int32_t list) {
    gei_MapWho->Reset(list);
}

void RenderSystem_MapLink(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link, int32_t list) {
    gei_MapWho->Link(a, b, c, d, link, list);
}

void RenderSystem_MapUnLink(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link) {
    gei_MapWho->Unlink(a, b, c, d, link);
}

void RenderSystem_Reset(int32_t a, int32_t b, int32_t c, int32_t d) {
    gei_MapWho->Reset(a, b, c, d);
}

void RenderSystem_ResetAll() {
    gei_MapWho->ResetAll();
}
