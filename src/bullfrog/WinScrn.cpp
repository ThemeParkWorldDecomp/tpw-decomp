// WinScrn.cpp: name proven by __sinit_WinScrn_cpp in the Bullfrog library.

#include "AudioPlaceHolder.h"
#include "bfl_Refpack.h"
#include "DrawState.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "LbSCBack.h"
#include "LbSurface.h"
#include "platform/macdoze.h"
#include "TbFPUPrecision.h"
#include "WinScrn.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>

int32_t gLbScreenModeFlagA;
int32_t gLbScreenModeFlagB;
int32_t gLbFadeActive;
uint32_t gLbFadeStep;
uint8_t gLbFadeSaved[0x400];
uint8_t gLbFadeZero[0x400];

// glue 0x1006c0cc: MSL __destroy_arr. Not translated yet; the destructor argument is a
// TVector descriptor whose layout is not declared. Defined empty so the launcher links.
extern "C" void __destroy_arr(void* array, void* destructor, int32_t size, int32_t count) {
    (void)array; (void)destructor; (void)size; (void)count;
}

// 0x10068298
uint32_t LbScreen_IsModeAvailable(uint32_t width, uint32_t height, uint32_t depth) {
    uint32_t found = 0;
    if (gWinScrnInitialised == 0) return 0;
    TbScreenModeFind find;
    TbScreenMode mode;
    TbResult first = LbScreen_ModeFindFirst(&find, &mode);
    if ((first.mCode >= 0) != (gLbScreenModeFlagA == 0)) return found;
    for (;;) {
        if (static_cast<uint32_t>(mode.mWidth) == width && static_cast<uint32_t>(mode.mHeight) == height &&
            static_cast<uint32_t>(mode.mDepth) == depth) {
            found = 1;
            break;
        }
        TbResult next = LbScreen_ModeFindNext(&find, &mode);
        if ((next.mCode >= 0) != (gLbScreenModeFlagB == 0)) break;
    }
    LbScreen_ModeFindEnd(&find);
    return found;
}

static void FadeStep(uint8_t* out, const uint8_t* from, const uint8_t* to, int32_t step, int32_t total) {
    for (int i = 0; i < 128; i++) {
        for (int k : {0, 1, 2, 4, 5, 6}) {
            int32_t a = from[i * 8 + k];
            int32_t b = to[i * 8 + k];
            out[i * 8 + k] = static_cast<uint8_t>(a + (step * (b - a)) / total);
        }
    }
}

// 0x100683ec
uint32_t LbScreen_FadePalette(TbPalette* palette, uint32_t count, TbPaletteFadeType type) {
    uint8_t buffer[0x400];
    uint8_t* target = reinterpret_cast<uint8_t*>(palette);
    if (target == nullptr) {
        target = gLbFadeZero;
        std::memset(gLbFadeZero, 0, 0x400);
    }
    if (type == 1) {
        LbScreen_GetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(gLbFadeSaved), 0, 0x100);
        for (gLbFadeStep = 1; gLbFadeStep < count; gLbFadeStep = gLbFadeStep + 1) {
            FadeStep(buffer, gLbFadeSaved, target, static_cast<int32_t>(gLbFadeStep), static_cast<int32_t>(count));
            LbScreen_WaitVbi();
            LbScreen_SetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(buffer), 0, 0x100);
        }
        LbScreen_SetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(target), 0, 0x100);
        gLbFadeActive = 0;
    } else {
        if (gLbFadeActive == 0) {
            gLbFadeStep = 0;
            gLbFadeActive = 1;
            LbScreen_GetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(gLbFadeSaved), 0, 0x100);
        }
        gLbFadeStep = gLbFadeStep + 1;
        if (gLbFadeStep == count) gLbFadeActive = 0;
        FadeStep(buffer, gLbFadeSaved, target, static_cast<int32_t>(gLbFadeStep), static_cast<int32_t>(count));
        LbScreen_WaitVbi();
        LbScreen_SetPaletteEntries(reinterpret_cast<TbPaletteEntry*>(buffer), 0, 0x100);
    }
    return gLbFadeStep;
}

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

// ---- WinScrn.cpp
void LbScreen_StopOpenFadePalette() { gLbFadeActive = 0; }   // 0x10068710

void LbScreen_SetDirectDrawGuid(NS_MacDoze::_GUID* guid) { gWinScrnGuid = guid; }   // 0x10068754

// bullfrog WinScrn.cpp (unit v5737): LbScreen_Terminate, LbApp_ShowMessage, LbScreen_GetDirectDraw, the display mode enumeration.

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// 0x100687a8, 72 B
IDirectDraw* LbScreen_GetDirectDraw(int32_t addRef) {
    if (addRef != 0 && gWinScrnDirectDraw != nullptr) gWinScrnDirectDraw->AddRef();
    return gWinScrnDirectDraw;
}

// Unit v5056, bullfrog WinScrn.cpp: LbScreen_Initialise 0x10068c60, ReleaseMode 0x100689ac, WindowProc 0x10068820.

int32_t gWinScrnInitialised;
NS_MacDoze::_GUID* gWinScrnGuid;
int32_t gWinScrnF120;
int32_t gWinScrnActive;
NS_MacDoze::IUnknown* gWinScrnClipper;
IDirectDraw* gWinScrnDirectDraw;
GrafPort* gWinScrnCoopWindow;
char gWinScrnClassName[64];
int32_t gWinScrnModeFlag;
uint8_t gWinScrnFocus;
_LbSystemEventCallback gLbSystemEventCallback;

int32_t WindowProc(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam) {
    if (message == 0x1c) {
        gWinScrnFocus = wParam != 0 ? 1 : 0;
    } else if (message == 0x10) {
        LbApp_SetCloseDownRequest(1);
        return 0;
    } else if (message == 6) {
        if (window == static_cast<GrafPort*>(LbApp_GetHWnd())) {
            int32_t active = (wParam - 1 < 2) ? 1 : 0;
            gWinScrnActive = active;
            NS_MacDoze::SetCursor(nullptr);
            int32_t event[2] = {4, active};
            gLbSystemEventCallback.EventNotification(0, event);
        }
    } else if (message == 0x112) {
        if (wParam == 0xf100) return 0;
    }
    if (LbApp_GetUserWndProc() != nullptr) return LbApp_GetUserWndProc()(window, message, wParam, lParam);
    return NS_MacDoze::DefWindowProc(window, message, wParam, lParam);
}
void ReleaseMode() {
    gScreenFPUHandler.mF00 = 0;
    reinterpret_cast<TbFPUFlag*>(&gScreenFPUHandler.mVtable)->Reset();
    gWinScrnActive = 0;
    gWinScrnF120 = 0;
    if (gWinScrnSurfaceB.mDDSurface == nullptr) return;
    int32_t event[2] = {5, 0};
    gLbSystemEventCallback.EventNotification(0, event);
    if (gWinScrnDirectDraw != nullptr) gWinScrnDirectDraw->SetCooperativeLevel(gWinScrnCoopWindow, 8);
    if (gWinScrnSurfaceC.mDDSurface != nullptr) {
        reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnSurfaceC.mDDSurface)->Release();
        gWinScrnSurfaceC.mDDSurface = nullptr;
    }
    if (gWinScrnSurfaceA.mDDSurface != nullptr) {
        reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnSurfaceA.mDDSurface)->Release();
        gWinScrnSurfaceA.mDDSurface = nullptr;
    }
    if (gWinScrnSurfaceB.mDDSurface != nullptr) {
        reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnSurfaceB.mDDSurface)->Release();
        gWinScrnSurfaceB.mDDSurface = nullptr;
    }
    gWinScrnPalette = nullptr;
    if (gWinScrnClipper != nullptr) {
        gWinScrnClipper->Release();
        gWinScrnClipper = nullptr;
    }
}

// 0x10068b10, 232 B
void LbScreen_Terminate() {
    if (gWinScrnInitialised != 0) {
        gWinScrnInitialised = 0;
        ReleaseMode();
        if (gWinScrnDirectDraw != nullptr) {
            gWinScrnDirectDraw->RestoreDisplayMode();
            gWinScrnDirectDraw->Release();
            gWinScrnDirectDraw = nullptr;
            LbSurface_RegisterDirectDraw(gWinScrnDirectDraw);
        }
        if (gWinScrnHWnd != nullptr) NS_MacDoze::DestroyWindow(static_cast<GrafPort*>(gWinScrnHWnd));
        gWinScrnHWnd = nullptr;
    }
    while (TbListNode* node = LbList_GetFirst(&gWinScrnModeList)) {
        node->mPrev->mNext = node->mNext;
        node->mNext->mPrev = node->mPrev;
        LbSE_OperatorDelete(node);
    }
}

int32_t LbScreen_IsInitialised() { return gWinScrnInitialised; }   // 0x10068c24

TbResult LbScreen_Initialise(NS_MacDoze::_GUID* guid, int32_t mode, void* hwnd, void* window) {
    TbResult result;
    if (gWinScrnInitialised != 0) {
        result.mCode = 0;
        return result;
    }
    LbApp_SetHWnd(hwnd);
    if (hwnd == nullptr) {
        RegisterBullfrogWindowClass();
        void* instance = LbApp_GetHInstance();
        int32_t height = NS_MacDoze::GetSystemMetrics(1);
        int32_t width = NS_MacDoze::GetSystemMetrics(0);
        const char* name = LbApp_GetWindowName();
        gWinScrnHWnd = NS_MacDoze::CreateWindowEx(8, gWinScrnClassName, name, 0x80080000u, 0, 0, width, height, nullptr,
                                                  nullptr, instance, nullptr);
        if (gWinScrnHWnd == nullptr) {
            result.mCode = -1;
            return result;
        }
        LbApp_SetHWnd(gWinScrnHWnd);
    }
    TbResult modes = CreateScreenModeList(mode, static_cast<GrafPort*>(window), guid);
    bool modesOk = modes.mCode >= 0;
    bool noFlag = gWinScrnModeFlag == 0;
    if (modesOk != noFlag) {
        result.mCode = -1;
        return result;
    }
    gWinScrnGuid = guid;
    gWinScrnInitialised = 1;
    result.mCode = 0;
    return result;
}

int32_t LbScreen_IsActive() { return gWinScrnActive; }   // 0x10068e20

// 0x10068e54, 240 B
void LbApp_ShowMessage(const char* format, ...) {
    char text[0x82c];
    va_list args;
    va_start(args, format);
    vsprintf(text, format, args);
    va_end(args);
    GrafPort* owner = gWinScrnCoopWindow;
    if (gWinScrnDirectDraw != nullptr) IDirectDraw_FlipToGDISurface(gWinScrnDirectDraw);
    if (gWinScrnCoopWindow != nullptr)
        NS_MacDoze::SetWindowPos(gWinScrnCoopWindow, reinterpret_cast<GrafPort*>(static_cast<intptr_t>(-2)), 0, 0, 0, 0, 3);
    NS_MacDoze::MessageBoxA(owner, text, "TO DO", 0x2000);
    if (gWinScrnCoopWindow != nullptr)
        NS_MacDoze::SetWindowPos(gWinScrnCoopWindow, reinterpret_cast<GrafPort*>(static_cast<intptr_t>(-1)), 0, 0, 0, 0, 3);
}

// 0x10068f70, 100 B
TbResult LbScreen_ModeFindFirst(TbScreenModeFind* find, TbScreenMode* mode) {
    find->mCursor = LbList_GetFirst(&gWinScrnModeList);
    return LbScreen_ModeFindNext(find, mode);
}

// 0x10069024, 72 B: the Mac's listing is not in the brief; from the draft. The cursor is a TbDisplayMode.
TbResult LbScreen_ModeFindNext(TbScreenModeFind* find, TbScreenMode* mode) {
    TbResult result;
    TbDisplayMode* node = static_cast<TbDisplayMode*>(find->mCursor);
    if (node->mNext == nullptr) {
        result.mCode = -1;
        return result;
    }
    int32_t* out = reinterpret_cast<int32_t*>(mode);
    uint32_t height = node->mHeight;
    out[0] = static_cast<int32_t>(node->mWidth);
    out[1] = static_cast<int32_t>(height);
    out[2] = static_cast<int32_t>(node->mBitCount);
    find->mCursor = node->mNext;
    result.mCode = 0;
    return result;
}

void LbScreen_ModeFindEnd(TbScreenModeFind*) {}   // 0x100690bc

// Unit v51000, bullfrog WinScrn.cpp: LbScreen_SetMode (0x10069100, 2928 B).

namespace {

// The cleanup the original repeats at every failure: give the DirectDraw object back and tell the surface library.
void DropDirectDraw() {
    if (gWinScrnDirectDraw != nullptr) {
        gWinScrnDirectDraw->RestoreDisplayMode();
        gWinScrnDirectDraw->Release();
        gWinScrnDirectDraw = nullptr;
        LbSurface_RegisterDirectDraw(gWinScrnDirectDraw);
    }
}

TbResult Answer(int32_t code) {
    TbResult result;
    result.mCode = code;
    return result;
}

}  // namespace

// The Mac passes the result in a hidden pointer (r3); parameters are r4..r8. `flags` bits: 1 back buffer in video memory,
// 2 flipping chain, 4 (adds 2), 8 (0x2000 caps), 0x10 windowed, 0x20 video memory off-screen surface, 0x40 FPU flags, 0x80 triple buffer.
TbResult LbScreen_SetMode(uint32_t width, uint32_t height, uint32_t bpp, uint32_t flags, const TbPalette* palette) {
    int32_t hr = 0;   // the original's local at 0x2f8
    uint8_t* globalPalette = reinterpret_cast<uint8_t*>(&_lbGlobalPalette);

    if (gScreenFPUHandler.mF00 != 0) reinterpret_cast<TbFPUFlag*>(&gScreenFPUHandler.mVtable)->CheckFlags();
    if ((flags & 4) != 0) flags |= 2;
    if ((flags & 0x10) != 0) {
        if ((flags & 2) != 0) return Answer(-1);
        flags |= 1;
    }
    int32_t closeEvent[2] = {6, 0};
    gLbSystemEventCallback.EventNotification(0, closeEvent);
    ReleaseMode();
    if (gWinScrnDirectDraw == nullptr) {
        TbResult created = CreateDirectDrawObject(gWinScrnGuid, &gWinScrnDirectDraw, nullptr);
        hr = created.mCode;
        if (hr < 0) {
            DropDirectDraw();
            LbApp_ShowMessage("Failed to create Direct Draw object");
            return Answer(hr);
        }
    }
    const bool windowed = (flags & 0x10) != 0;
    if (windowed) {
        DDSURFACEDESC mode;
        std::memset(&mode, 0, sizeof(mode));   // host: the structure is larger than the Mac's 0x6c bytes
        NS_MacDoze::ZeroMemory(&mode, 0x6c);
        mode.dwSize = 0x6c;
        int32_t rv = IDirectDraw_GetDisplayMode(gWinScrnDirectDraw, &mode);
        hr = rv;
        if (rv < 0) return Answer(rv);
        if (mode.ddpfPixelFormat.dwRGBBitCount != bpp) return Answer(-1);
    }
    LbSurface_RegisterDirectDraw(gWinScrnDirectDraw);
    gWinScrnCoopWindow = gWinScrnHWnd != nullptr ? static_cast<GrafPort*>(gWinScrnHWnd) : static_cast<GrafPort*>(LbApp_GetHWnd());
    NS_MacDoze::ShowWindow(gWinScrnCoopWindow, 5);
    if (gWinScrnCoopWindow != nullptr) {
        NS_MacDoze::tagMSG msg;
        while (NS_MacDoze::PeekMessage(&msg, nullptr, 0, 0, 1) != 0) {
            uint32_t words[4];
            std::memcpy(words, msg.mBytes, sizeof(words));
            NS_MacDoze::DefWindowProc(reinterpret_cast<GrafPort*>(static_cast<uintptr_t>(words[0])), words[1], words[2],
                                      reinterpret_cast<int32_t*>(static_cast<uintptr_t>(words[3])));
        }
    }
    NS_MacDoze::Sleep(0);
    gWinScrnActive = 1;
    gWinScrnF120 = 1;
    uint32_t level = windowed ? 8 : 0x51;
    if ((flags & 0x40) != 0) {
        level |= 0x800;
        gScreenFPUHandler.mF00 = 1;
        reinterpret_cast<TbFPUFlag*>(&gScreenFPUHandler.mVtable)->SetFlags(0x3f, 1);
    }
    hr = gWinScrnDirectDraw->SetCooperativeLevel(gWinScrnCoopWindow, level);
    if (hr < 0) {
        DropDirectDraw();
        return Answer(hr);
    }
    if (windowed) {
        hr = gWinScrnDirectDraw->CreateClipper(0, reinterpret_cast<IDirectDrawClipper**>(&gWinScrnClipper), nullptr);
        if (hr < 0) {
            DropDirectDraw();
            return Answer(hr);
        }
        hr = static_cast<IDirectDrawClipper*>(gWinScrnClipper)->SetHWnd(0, gWinScrnCoopWindow);
        if (hr < 0) {
            DropDirectDraw();
            return Answer(hr);
        }
    }
    if (!windowed) {
        hr = gWinScrnDirectDraw->SetDisplayMode(width, height, bpp, 0, 0);
        if (hr < 0) {
            DropDirectDraw();
            return Answer(hr);
        }
    }
    if (palette != nullptr) {
        std::memcpy(globalPalette, palette, 0x400);
    } else {
        // Four bytes per entry: red = blue = the index, green 0; the fourth byte is left alone.
        for (uint32_t i = 0; i < 256; ++i) {
            globalPalette[i * 4] = static_cast<uint8_t>(i);
            globalPalette[i * 4 + 1] = 0;
            globalPalette[i * 4 + 2] = static_cast<uint8_t>(i);
        }
    }
    if (bpp == 8) {
        const TbPaletteEntry* entries = palette != nullptr ? reinterpret_cast<const TbPaletteEntry*>(palette)
                                                          : reinterpret_cast<const TbPaletteEntry*>(globalPalette);
        hr = IDirectDraw_CreatePalette(gWinScrnDirectDraw, 0x44, entries, &gWinScrnPalette, nullptr);
        if (hr < 0) {
            DropDirectDraw();
            return Answer(hr);
        }
    } else {
        // The original calls vtable slot 5 (Release) with no NULL check in the 8-bit branch below, with one here.
        if (gWinScrnPalette != nullptr) reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnPalette)->Release();
        gWinScrnPalette = nullptr;
    }

    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));   // host: the structure is larger than the Mac's 0x6c bytes
    NS_MacDoze::ZeroMemory(&desc, 0x6c);
    desc.dwSize = 0x6c;
    desc.dwFlags = 1;
    desc.dwCaps = 0x200;
    const bool chain = (flags & 2) != 0;
    if (chain) {
        desc.dwFlags |= 0x20;
        desc.dwCaps |= 0x18;
        desc.dwBackBufferCount = (flags & 0x80) != 0 ? 2 : 1;
    }
    const bool plain3d = (flags & 8) != 0;
    if (plain3d) desc.dwCaps |= 0x2000;
    hr = gWinScrnDirectDraw->CreateSurface(&desc, &gWinScrnSurfaceB.mDDSurface, nullptr);
    if (hr < 0) {
        gWinScrnSurfaceB.mDDSurface = nullptr;
        gWinScrnDirectDraw->SetCooperativeLevel(gWinScrnCoopWindow, 8);
        DropDirectDraw();
        return Answer(hr);
    }
    hr = gWinScrnSurfaceB.mDDSurface->GetSurfaceDesc(&desc);
    if (hr < 0) {
        gWinScrnSurfaceB.mDDSurface->Release();
        gWinScrnSurfaceB.mDDSurface = nullptr;
        DropDirectDraw();
        return Answer(hr);
    }
    gWinScrnDoubleBuffer = static_cast<int32_t>(desc.dwCaps & 0x200000);
    if (gWinScrnDoubleBuffer != 0 && (flags & 1) != 0) {
        gWinScrnSurfaceB.mDDSurface->Release();
        gWinScrnSurfaceB.mDDSurface = nullptr;
        desc.dwFlags = 1;
        desc.dwCaps = 0x200;
        desc.dwFlags |= 0x20;
        desc.dwCaps |= 0x18;
        desc.dwBackBufferCount = 1;
        hr = gWinScrnDirectDraw->CreateSurface(&desc, &gWinScrnSurfaceB.mDDSurface, nullptr);
        if (hr < 0) {
            gWinScrnSurfaceB.mDDSurface = nullptr;
            DropDirectDraw();
            return Answer(hr);
        }
        DDSCAPS caps;
        caps.dwCaps = 4;
        hr = IDirectDrawSurface_GetAttachedSurface(gWinScrnSurfaceB.mDDSurface, &caps, &gWinScrnSurfaceA.mDDSurface);
        if (hr < 0) return Answer(hr);
    }
    hr = -1;
    if (chain) {
        DDSCAPS caps;
        caps.dwCaps = 4;
        hr = IDirectDrawSurface_GetAttachedSurface(gWinScrnSurfaceB.mDDSurface, &caps, &gWinScrnSurfaceC.mDDSurface);
        if (hr < 0) return Answer(hr);
    } else {
        desc.dwFlags = 7;
        desc.dwHeight = height;
        desc.dwWidth = width;
        desc.dwCaps = (flags & 0x20) != 0 ? 0x4040 : 0x840;
        if (plain3d) desc.dwCaps |= 0x2000;
        hr = gWinScrnDirectDraw->CreateSurface(&desc, &gWinScrnSurfaceC.mDDSurface, nullptr);
    }
    if (gWinScrnSurfaceC.mDDSurface == nullptr) {
        gWinScrnSurfaceB.mDDSurface->Release();
        gWinScrnSurfaceB.mDDSurface = nullptr;
        if (bpp == 8) {
            reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnPalette)->Release();   // no NULL check, as the original
            gWinScrnPalette = nullptr;
        }
        DropDirectDraw();
        return Answer(hr);
    }
    if (windowed) {
        gWinScrnSurfaceB.mDDSurface->SetClipper(static_cast<IDirectDrawClipper*>(gWinScrnClipper));
        if (bpp == 8) {
            NS_MacDoze::InternalHandle* dc;
            if (IDirectDrawSurface_GetDC(gWinScrnSurfaceB.mDDSurface, &dc) >= 0) {
                NS_MacDoze::GetSystemPaletteEntries(dc, 0, 0x100, reinterpret_cast<TbPaletteEntry*>(globalPalette));
                IDirectDrawSurface_ReleaseDC(gWinScrnSurfaceB.mDDSurface, dc);
            }
        }
    }
    if (bpp == 8) IDirectDrawSurface_SetPalette(gWinScrnSurfaceB.mDDSurface, gWinScrnPalette);
    _InitialiseSurface(&gWinScrnSurfaceB, width, height, bpp, flags);
    _InitialiseSurface(&gWinScrnSurfaceC, width, height, bpp, flags);
    gWinScrnSurface = &gWinScrnSurfaceC;
    // The event record: type 7, width, height, the depth in the first byte of the fourth word, then a zero word.
    uint32_t modeEvent[5] = {7, width, height, 0, 0};
    reinterpret_cast<uint8_t*>(&modeEvent[3])[0] = static_cast<uint8_t>(bpp);
    gLbSystemEventCallback.EventNotification(0, modeEvent);
    return Answer(0);
}

// 0x10069cac, 116 B
TbResult LbScreen_RestoreMode() {
    ReleaseMode();
    if (gWinScrnDirectDraw != nullptr) {
        gWinScrnDirectDraw->RestoreDisplayMode();
        reinterpret_cast<NS_MacDoze::IUnknown*>(gWinScrnDirectDraw)->Release();
        gWinScrnDirectDraw = nullptr;
        LbSurface_RegisterDirectDraw(gWinScrnDirectDraw);
    }
    return {0};
}

// Unit v5334, bullfrog WinScrn.cpp: LbScreen_Swap, LbScreen_PartSwap and CreateScreenModeList.

int32_t gWinScrnDoubleBuffer;                  // 0x10075118
int32_t gWinScrnModeGate;                      // 0x10073a64
TbList gWinScrnModeList;                       // 0x10075128
int32_t gWinScrnClientOrigin[2];               // 0x10073a4c

namespace {
// The record the system event callback gets: a kind word and the surface (Mac 8 bytes, host pointer 8 bytes).
struct SurfaceEvent {
    int32_t mKind;
    TbSurface* mSurface;
};
void Notify(int32_t kind, TbSurface* surface) {
    SurfaceEvent event{kind, surface};
    gLbSystemEventCallback.EventNotification(1, &event);
}
}  // namespace

// 0x10069d4c
TbResult LbScreen_Swap(uint32_t flags) {
    const uint32_t f = flags ^ 1;
    int32_t r;
    if (gWinScrnSurfaceB.mF1C & 2) {
        Notify(2, gWinScrnSurface);
        do {
            do {
                r = IDirectDrawSurface_Flip(gWinScrnSurfaceB.mDDSurface, gWinScrnSurface->mDDSurface, f);
            } while (static_cast<uint32_t>(r) == 0x887601AEu);
        } while (static_cast<uint32_t>(r) == 0x8876021Cu);
        Notify(3, gWinScrnSurface);
    } else {
        Notify(0, gWinScrnSurface);
        if (gWinScrnSurfaceB.mF1C & 0x10) {
            int32_t fx[25];
            NS_MacDoze::ZeroMemory(fx, 100);
            fx[0] = 100;
            void* hwnd = LbApp_GetHWnd();
            int32_t rect[4];
            NS_MacDoze::GetClientRect(static_cast<GrafPort*>(hwnd), reinterpret_cast<SamsUtilities::URectangle::LongRect*>(rect));
            int32_t point[2] = {gWinScrnClientOrigin[0], gWinScrnClientOrigin[1]};
            NS_MacDoze::ClientToScreen(static_cast<GrafPort*>(hwnd), reinterpret_cast<NS_MacDoze::tagPOINT*>(point));
            rect[0] += point[0];
            rect[2] += point[0];
            rect[1] += point[1];
            rect[3] += point[1];
            r = gWinScrnSurfaceB.mDDSurface->Blt(reinterpret_cast<SamsUtilities::URectangle::LongRect*>(rect),
                                                 gWinScrnSurface->mDDSurface, nullptr, 0x1000000,
                                                 reinterpret_cast<DDBLTFX*>(fx));
        } else if (gWinScrnDoubleBuffer != 0) {
            gWinScrnSurfaceA.mDDSurface->BltFast(0, 0, gWinScrnSurface->mDDSurface, nullptr, f << 4);
            r = IDirectDrawSurface_Flip(gWinScrnSurfaceB.mDDSurface, gWinScrnSurfaceA.mDDSurface, f);
        } else {
            r = gWinScrnSurfaceB.mDDSurface->BltFast(0, 0, gWinScrnSurface->mDDSurface, nullptr, f << 4);
        }
        Notify(1, gWinScrnSurface);
    }
    if (static_cast<uint32_t>(r) == 0x887601C2u) {
        _CheckAndRestoreSurface(&gWinScrnSurfaceB);
        _CheckAndRestoreSurface(gWinScrnSurface);
    }
    return {r};
}

// 0x10069fec
TbResult LbScreen_PartSwap(const TbPoint& at, const TbRect& rect, uint32_t flags) {
    if (gWinScrnSurfaceB.mF1C & 2) return {-1};
    if (gWinScrnSurfaceB.mF1C & 0x10) return {-1};
    if (gWinScrnDoubleBuffer != 0) return {-1};
    Notify(0, nullptr);
    TbRect source = rect;
    const int32_t r = gWinScrnSurfaceB.mDDSurface->BltFast(
        static_cast<uint32_t>(at.x), static_cast<uint32_t>(at.y), gWinScrnSurface->mDDSurface,
        reinterpret_cast<SamsUtilities::URectangle::LongRect*>(&source), (flags ^ 1) << 4);
    Notify(0, nullptr);
    if (static_cast<uint32_t>(r) == 0x887601C2u) {
        _CheckAndRestoreSurface(&gWinScrnSurfaceB);
        _CheckAndRestoreSurface(gWinScrnSurface);
    }
    return {r != 0 ? -1 : 0};
}

// bullfrog WinScrn.cpp, the DirectDraw object, mode list, palette and static initialiser part (unit v5041).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1006b82c

// The file's statics (names invented). The surfaces are 0x50 bytes on the Mac, in the order A, B, C.
TbSurface gWinScrnSurfaceA;
TbSurface gWinScrnSurfaceB;
TbSurface gWinScrnSurfaceC;
_ScreenFPUHandler gScreenFPUHandler;
uint8_t gScreenFPUHandlerNode[0x2c];
const void* const kScreenFPUHandlerVtable[2] = {nullptr, nullptr};
uint8_t kScreenFPUHandlerDtorVector[8];

// 0x1006a198
void LbScreen_SwapClear(TbColour colour) {
    LbScreen_Swap(0);
    LbSurface_ClearRect(gWinScrnSurface, nullptr, colour, 0);
}

// 0x1006a220
void LbScreen_SetPaletteEntries(const TbPaletteEntry* entries, uint32_t start, uint32_t count) {
    std::memcpy(reinterpret_cast<uint8_t*>(&_lbGlobalPalette) + start * 4, entries, count << 2);
    if (gWinScrnPalette != nullptr && gWinScrnSurfaceB.mDDSurface != nullptr)
        gWinScrnPalette->SetEntries(0, start, count, entries);
}

// 0x1006a304
TbPaletteEntry* LbScreen_GetPaletteEntries(TbPaletteEntry* entries, uint32_t start, uint32_t count) {
    uint8_t* table = reinterpret_cast<uint8_t*>(&_lbGlobalPalette) + start * 4;
    if (gWinScrnPalette == nullptr) {
        std::memcpy(entries, table, count << 2);
    } else if (gWinScrnPalette->GetEntries(0, start, count, reinterpret_cast<TbPaletteEntry*>(table)) == 0) {
        std::memcpy(entries, table, count << 2);
    }
    return entries;
}

void LbScreen_WaitVbi() { IDirectDraw_WaitForVerticalBlank(gWinScrnDirectDraw, 1, nullptr); }   // 0x1006a400

// 0x1006a45c
bool EnumModesCallback(DDSURFACEDESC* desc, void* list) {
    // The Mac operator new never answers NULL; the listing tests for it all the same.
    TbDisplayMode* node = static_cast<TbDisplayMode*>(::operator new(sizeof(TbDisplayMode), std::nothrow));
    if (node == nullptr) return false;
    node->mWidth = desc->dwWidth;
    node->mHeight = desc->dwHeight;
    node->mBitCount = desc->ddpfPixelFormat.dwRGBBitCount;
    TbList* l = static_cast<TbList*>(list);
    LbList_Insert(l, l->mTailPrev, node);
    return true;
}

// 0x1006a514
TbResult CreateDirectDrawObject(NS_MacDoze::_GUID* guid, IDirectDraw** out, void**) {
    TbResult result;
    if (DirectDrawCreate(guid, out, nullptr) != 0) {
        result.mCode = -1;
    } else {
        if (gWinScrnHWnd != nullptr) LbApp_SetHWnd(gWinScrnHWnd);
        result.mCode = 0;
    }
    return result;
}

// 0x1006a5d8
TbResult CreateScreenModeList(int32_t mode, GrafPort* window, NS_MacDoze::_GUID* guid) {
    IDirectDraw* dd = nullptr;
    int32_t created;
    if (DirectDrawCreate(guid, &dd, nullptr) != 0) {
        created = -1;
    } else {
        if (gWinScrnHWnd != nullptr) LbApp_SetHWnd(gWinScrnHWnd);
        created = 0;
    }
    const uint8_t have = static_cast<uint8_t>((created >> 31) + 1);
    if (have != (gWinScrnModeGate == 0 ? 1 : 0)) return {-1};
    LbList_Init(&gWinScrnModeList);
    if (mode != 0) {
        GrafPort* w = window;
        if (window == nullptr) {
            RegisterBullfrogWindowClass();
            void* instance = LbApp_GetHInstance();
            w = static_cast<GrafPort*>(NS_MacDoze::CreateWindowEx(0, gWinScrnClassName, "ModeList", 0x80000000u, 0, 0, 0x500, 0x400,
                                                                  nullptr, nullptr, instance, nullptr));
            if (w == nullptr) return {-1};
        }
        if (dd->SetCooperativeLevel(w, 0x55) != 0) {
            reinterpret_cast<NS_MacDoze::IUnknown*>(dd)->Release();
            return {-1};
        }
        IDirectDraw_EnumDisplayModes(dd, 0, nullptr, &gWinScrnModeList, EnumModesCallback);
        dd->SetCooperativeLevel(w, 8);
        if (window == nullptr) NS_MacDoze::DestroyWindow(w);
    } else {
        IDirectDraw_EnumDisplayModes(dd, 0, nullptr, &gWinScrnModeList, EnumModesCallback);
    }
    reinterpret_cast<NS_MacDoze::IUnknown*>(dd)->Release();
    return {0};
}

// ---- WinScrn.cpp: RegisterBullfrogWindowClass 0x1006a824 and _ScreenFPUHandler::~_ScreenFPUHandler 0x1006aa8c

uint8_t gRegisterBullfrogWindowClassGuard;   // 0x100750f4
int32_t gRegisterBullfrogWindowClassDone;    // 0x100750f8

void RegisterBullfrogWindowClass() {
    // A function-local `static int` with a compiler guard byte: the guard zeroes the value once.
    if (gRegisterBullfrogWindowClassGuard == 0) {
        gRegisterBullfrogWindowClassDone = 0;
        gRegisterBullfrogWindowClassGuard = 1;
    }
    if (gRegisterBullfrogWindowClassDone == 0) {
        gRegisterBullfrogWindowClassDone = 1;
        NS_MacDoze::tagWNDCLASSA wc{};
        NS_MacDoze::ZeroMemory(&wc, 0x28);
        wc.style = 3;
        wc.lpfnWndProc = WindowProc;   // the Mac stores the TVector at 0x10070e24
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = LbApp_GetHInstance();
        if (LbApp_GetWindowIcon() != 0) {
            const uint32_t icon = static_cast<uint16_t>(LbApp_GetWindowIcon());
            void* instance = LbApp_GetHInstance();
            wc.hIcon = NS_MacDoze::LoadIcon(instance, reinterpret_cast<const char*>(static_cast<uintptr_t>(icon)));
        }
        wc.hbrBackground = NS_MacDoze::GetStockObject(4);
        wc.lpszMenuName = nullptr;
        std::sprintf(gWinScrnClassName, "_BullfrogLibScreen");   // the string at 0x1006cc44
        wc.lpszClassName = gWinScrnClassName;
        NS_MacDoze::RegisterClass(&wc);
    }
}

// 0x1006a968
static void InitSurface(TbSurface& s) {
    for (int i = 0; i < 5; i++) s.mColour[i] = 0;
    s.mF20 = 0;
    s.mDDSurface = nullptr;
    new (&s.mArea) TbRenderArea();   // the out-of-line constructor, 0x10016630
}

void __sinit_WinScrn_cpp() {
    InitSurface(gWinScrnSurfaceC);
    InitSurface(gWinScrnSurfaceB);
    InitSurface(gWinScrnSurfaceA);
    gScreenFPUHandler.mF00 = 0;
    gScreenFPUHandler.mVtable = kScreenFPUHandlerVtable;
    gScreenFPUHandler.mF08 = 0;
    gScreenFPUHandler.mF0A = 0;
    gScreenFPUHandler.mF0C = 0;
    __register_global_object(&gScreenFPUHandler, kScreenFPUHandlerDtorVector, gScreenFPUHandlerNode);
}

// 0x1006ab38: stores its vtable, then the base vtable, runs TbFPUFlag::Reset (the base
// destructor body), and deletes the object when the hidden flag is positive (the
// compiler's deleting destructor).
TbFPUSinglePrecision::~TbFPUSinglePrecision() {}
