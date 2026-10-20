// Unit v5334, bullfrog WinScrn.cpp: LbScreen_Swap, LbScreen_PartSwap and CreateScreenModeList.
#include <cstdint>

#include "WinScrn.h"
#include "platform/macdoze.h"

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
