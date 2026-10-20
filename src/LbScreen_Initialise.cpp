// Unit v5056, bullfrog WinScrn.cpp: LbScreen_Initialise 0x10068c60, ReleaseMode 0x100689ac, WindowProc 0x10068820.
#include "TbFPUPrecision.h"
#include "WinScrn.h"

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
