// Test library for WinScrn.cpp's LbScreen_Initialise, ReleaseMode and WindowProc (unit v5056; libtpw_unit_LbScreen_Initialise.dylib,
// tests/test_LbScreen_Initialise.py), main app:
//   src/LbScreen_Initialise.cpp   the three functions and the new window globals
//
// The callees outside the unit are recording fakes; the objects they get are fake addresses never dereferenced. The globals of
// CreateDirectDrawObject.cpp (window, palette, surfaces, FPU handler) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbFPUPrecision.h"
#include "WinScrn.h"

using namespace calllog;

void* gWinScrnHWnd;
IDirectDrawPalette* gWinScrnPalette;
TbSurface gWinScrnSurfaceA;
TbSurface gWinScrnSurfaceB;
TbSurface gWinScrnSurfaceC;
_ScreenFPUHandler gScreenFPUHandler;

namespace {
int64_t U(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }
int64_t UW(uint32_t v) { return static_cast<int64_t>(v); }

int32_t UserProc(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam) {
    ev("UserWndProc", {U(window), UW(message), UW(wParam), U(lParam)});
    return static_cast<int32_t>(ret("UserWndProc"));
}
}  // namespace

void TbFPUFlag::Reset() { ev("FPUReset", {lab(this)}); }

void _LbSystemEventCallback::EventNotification(int32_t type, void* data) {
    const int32_t* words = static_cast<const int32_t*>(data);
    ev("EventNotification", {lab(this), type, words[0], words[1]});
}

int32_t IDirectDraw::SetCooperativeLevel(GrafPort* window, uint32_t flags) {
    ev("SetCooperativeLevel", {U(this), U(window), UW(flags)});
    return 0;
}

uint32_t NS_MacDoze::IUnknown::Release() {
    ev("Release", {U(this)});
    return 0;
}

void LbApp_SetHWnd(void* hwnd) { ev("SetHWnd", {U(hwnd)}); }
void LbApp_SetCloseDownRequest(int32_t request) { ev("SetCloseDownRequest", {request}); }
void* LbApp_GetHWnd() {
    ev("GetHWnd", {});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("GetHWnd")));
}
void* LbApp_GetHInstance() {
    ev("GetHInstance", {});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("GetHInstance")));
}
LbUserWndProc LbApp_GetUserWndProc() {
    int64_t v = ret("GetUserWndProc");
    ev("GetUserWndProc", {});
    return v != 0 ? UserProc : nullptr;
}
const char* LbApp_GetWindowName() {
    ev("GetWindowName", {});
    return reinterpret_cast<const char*>(static_cast<uintptr_t>(ret("GetWindowName")));
}
void RegisterBullfrogWindowClass() { ev("RegisterClass", {}); }
TbResult CreateScreenModeList(int32_t mode, GrafPort* window, NS_MacDoze::_GUID* guid) {
    ev("CreateScreenModeList", {UW(static_cast<uint32_t>(mode)), U(window), U(guid)});
    TbResult r;
    r.mCode = static_cast<int32_t>(ret("CreateScreenModeList"));
    return r;
}

void* NS_MacDoze::CreateWindowEx(uint32_t exStyle, const char* className, const char* windowName, uint32_t style, int32_t x,
                                 int32_t y, int32_t width, int32_t height, GrafPort* parent, void* menu, void* instance,
                                 void* param) {
    const unsigned char* c = reinterpret_cast<const unsigned char*>(className);
    uint32_t cls = (uint32_t(c[0]) << 24) | (uint32_t(c[1]) << 16) | (uint32_t(c[2]) << 8) | uint32_t(c[3]);
    ev("CreateWindowEx", {UW(exStyle), UW(cls), U(windowName), UW(style), UW(static_cast<uint32_t>(x)), UW(static_cast<uint32_t>(y)),
                          UW(static_cast<uint32_t>(width)), UW(static_cast<uint32_t>(height)), U(parent), U(menu), U(instance),
                          U(param)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("CreateWindowEx")));
}
int32_t NS_MacDoze::GetSystemMetrics(int32_t index) {
    ev("GetSystemMetrics", {index});
    return static_cast<int32_t>(ret("GetSystemMetrics"));
}
int32_t NS_MacDoze::DefWindowProc(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam) {
    ev("DefWindowProc", {U(window), UW(message), UW(wParam), U(lParam)});
    return static_cast<int32_t>(ret("DefWindowProc"));
}
void NS_MacDoze::SetCursor(SamsUtilities::UCursor* cursor) { ev("SetCursor", {U(cursor)}); }

extern "C" {

uint64_t tpw_v5056_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"init", &gWinScrnInitialised},          {"guid", &gWinScrnGuid},
        {"f120", &gWinScrnF120},                 {"active", &gWinScrnActive},
        {"clipper", &gWinScrnClipper},           {"dd", &gWinScrnDirectDraw},
        {"coop", &gWinScrnCoopWindow},           {"cls", gWinScrnClassName},
        {"modeflag", &gWinScrnModeFlag},         {"focus", &gWinScrnFocus},
        {"hwnd", &gWinScrnHWnd},                 {"palette", &gWinScrnPalette},
        {"fpub", &gScreenFPUHandler.mF00},       {"fpu", &gScreenFPUHandler.mVtable},
        {"cb", &gLbSystemEventCallback},         {"ddA", &gWinScrnSurfaceA.mDDSurface},
        {"ddB", &gWinScrnSurfaceB.mDDSurface},   {"ddC", &gWinScrnSurfaceC.mDDSurface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5056_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5056_layout(int64_t*) { return 0; }

int32_t tpw_v5056_init(uint32_t guid, int32_t mode, uint32_t hwnd, uint32_t window) {
    return LbScreen_Initialise(reinterpret_cast<NS_MacDoze::_GUID*>(static_cast<uintptr_t>(guid)), mode,
                               reinterpret_cast<void*>(static_cast<uintptr_t>(hwnd)),
                               reinterpret_cast<void*>(static_cast<uintptr_t>(window))).mCode;
}
void tpw_v5056_release() { ReleaseMode(); }
int32_t tpw_v5056_wproc(uint32_t window, uint32_t message, uint32_t wParam, uint32_t lParam) {
    return WindowProc(reinterpret_cast<GrafPort*>(static_cast<uintptr_t>(window)), message, wParam,
                      reinterpret_cast<int32_t*>(static_cast<uintptr_t>(lParam)));
}

}  // extern "C"
