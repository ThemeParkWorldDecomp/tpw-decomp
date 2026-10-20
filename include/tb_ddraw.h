// tb_ddraw.h: the engine's DirectDraw wrapper (engine library, original tb_ddraw.cpp: the
// file name is in CDirectDraw::Init's log context; CDirectDraw::CDirectDraw at 0x10003890,
// right after tb_capture.cpp's static initialiser).
//
// PARTIAL: only what translated files use so far. Offsets in comments are the original's
// (0x78c bytes in all); the pointer fields make the host layout differ.
#pragma once
#include <cstdint>

#include "platform/indirectx.h"

class CDirectDraw {
public:
    CDirectDraw();                    // 0x10003890
    ~CDirectDraw();                   // 0x100038f0, calls Close
    int32_t Init(int32_t width, int32_t height, int32_t depth, int32_t device,
                 int32_t fullscreen, int32_t videoMemory); // 0x10003a48
    int32_t SetupCaps();              // 0x1000498c
    int32_t Close();                  // 0x10004cc0
    int32_t TempShutdown();           // 0x10004dfc
    int32_t ReInitialize();           // 0x10004e7c
    DDSURFACEDESC2 LockBackbuffer();   // 0x10004f8c, aggregate return
    int32_t UnlockBackbuffer();       // 0x10005098
    static int32_t FindDeviceCallback(NS_MacDoze::_GUID*, char*, char*, void*); // 0x10003974

    IDirectDraw* mDirectDraw1;        // 0x00
    IDirectDraw* mDirectDraw2;        // 0x04
    IDirectDraw4* mDirectDraw;        // 0x08
    IDirectDrawClipper* mClipper;     // 0x0c
    IDirectDrawSurface4* mPrimary;    // 0x10  primary surface
    IDirectDrawSurface4* mBack;       // 0x14  back buffer (CCapture::GetCapture reads it)
    NS_MacDoze::IUnknown* mSurface18; // 0x18
    DDDEVICEIDENTIFIER mDeviceInfo;   // 0x1c
    NS_MacDoze::_GUID mDevices[4];    // 0x444
    DDCAPS_DX6 mHardwareCaps;         // 0x484
    DDCAPS_DX6 mSoftwareCaps;         // 0x600
    uint32_t mFlags;                  // 0x77c, bit 1: full screen
    int32_t mF780;                    // 0x780, constructor sets 1
    int32_t mDeviceCount;             // 0x784 EngineStartup: below 2, the second video card setting is dropped
    int32_t mDevice;                  // 0x788
};

// DDraw_Init (0x100036b0): the display for `window` at width x height and `depth` bits;
// nonzero on success. DDraw_Close (0x1000380c) undoes it.
int32_t DDraw_Init(GrafPort* window, int32_t width, int32_t height, int32_t depth,
                   int32_t device, int32_t fullscreen);
int32_t DDraw_Close();

extern CDirectDraw* gei_DDraw;   // engine data 0x10052ad0, defined by tb_ddraw.cpp

// The HRESULT of the last DirectDraw call (engine data 0x10052b40, not exported; written by
// CDirectDraw's methods, CCapture::GetCapture and the OpenGL draw-primitive code;
// defined by tb_ddraw.cpp).
extern int32_t gDDResult;
