// Unit v51000, bullfrog WinScrn.cpp: LbScreen_SetMode (0x10069100, 2928 B).
#include "TbFPUPrecision.h"
#include "WinScrn.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "LbSCBack.h"
#include "platform/macdoze.h"

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
