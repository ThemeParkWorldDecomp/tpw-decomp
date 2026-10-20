// tb_resource_system.h: the engine's system settings object and the RenderSystem_* entry
// points (engine library, original tb_resource_system.cpp, functions at
// 0x100287a4..0x10029044; gei_System is built by the static initialiser).
#pragma once
#include <cstdint>

#include "CPU.h"

struct GrafPort;
class CMapLinkBase;

// The renderer interface (the RTTI name is "CRenderSystem"). COpenGL derives from it
// virtually and OpenGL_Init stores the base subobject in gei_System.mRenderSystem.
// Pure virtual: the vtable slots, in order, are the seven functions below.
class CRenderSystem {
public:
    virtual int32_t ClearViewport() = 0;
    virtual int32_t FlipScreen() = 0;          // 0 when the flip failed
    virtual int32_t TempShutdown() = 0;
    virtual int32_t ReInitialize() = 0;
    virtual int32_t GetTextureMemory() = 0;
    virtual int32_t GetVRAMSize() = 0;
    virtual void* CreateTextureBase() = 0;     // a CTextureBase
};

class CSystem {
public:
    enum : uint32_t {
        kFlag00000001 = 0x00000001,   // cleared while RenderSystem_Flush clears and flips
        kAltTab = 0x10000000,         // CErrorHandler::GetAltTabStatus found a lost surface
        kSecondViewport = 0x20000000, // PreprocessMatrices also prepares view matrix set [1]
        kLightsChanged = 0x40000000,  // set by UpdateLights
        // CMapWho::Render: the view moved since the last frame (or flag 0x40 asks for it every
        // frame); with kLightsChanged it makes RenderWindow empty the static render memory too.
        kViewChanged = 0x80000000,
        kFirstPerson = 0x00000100,    // RenderWindow: the view comes from the height field's DivideAndConquer
    };
    enum : int32_t {
        kRendererOpenGL = 3,          // the only renderer the Mac build has
    };

    // Inline in the original: its body is __sinit_tb_resource_system_cpp (0x10028f90).
    CSystem();

    void OutputDebugInfo();
    // Notes the display's frame count and raises kLightsChanged.
    void UpdateLights();

    uint32_t mFlags;                 // 0x00  0x03000141 after construction
    int32_t mLightFrame;             // 0x04  -1; UpdateLights: gei_Display.mFrameCount
    int32_t mF08;                    // 0x08  0
    int32_t mF0C;                    // 0x0c  0
    GrafPort* mWindow;               // 0x10  the game window; flips happen only when it is in front
    int32_t mClocksPerMs;            // 0x14  mCPU.clocks_per_millisec(1000)
    uint32_t mTotalPhys;             // 0x18  GlobalMemoryStatus().dwTotalPhys
    int32_t mRenderer;               // 0x1c  kRendererOpenGL
    int32_t mF20;                    // 0x20  1 after construction; Matrix_Init sets 5
    CRenderSystem* mRenderSystem;    // 0x24  set by OpenGL_Init, cleared by OpenGL_Close
    CPU mCPU;                        // 0x28  detect()ed by the constructor
};
// 0x48 bytes on the Mac; the two pointers make the host copy larger.

extern CSystem gei_System;   // engine data 0x10165f80

// Renderer selection: only kRendererOpenGL is handled; anything else asserts.
int32_t RenderSystem_Init(int32_t mode);
int32_t RenderSystem_Close();
void RenderSystem_ClearViewport();
// Flips when the game window is in front (returns 1 otherwise); a failed flip returns
// whether it was a lost surface (CErrorHandler::GetAltTabStatus).
int32_t RenderSystem_FlipScreen();
void RenderSystem_Render();
// Clears and flips three times (every buffer of a triple-buffered chain) with
// kFlag00000001 cleared; returns 1.
int32_t RenderSystem_Flush();
// Thin wrappers around gei_MapWho.
void RenderSystem_FrontLink(CMapLinkBase* link, int32_t list);
void RenderSystem_FrontUnlink(CMapLinkBase* link, int32_t list);
void RenderSystem_FrontReset(int32_t list);
void RenderSystem_MapLink(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link, int32_t list);
void RenderSystem_MapUnLink(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link);
void RenderSystem_Reset(int32_t a, int32_t b, int32_t c, int32_t d);
void RenderSystem_ResetAll();
