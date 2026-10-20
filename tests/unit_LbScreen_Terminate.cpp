// Test library for the WinScrn terminate / message / mode enumeration functions (unit v5737; libtpw_unit_LbScreen_Terminate.dylib,
// tests/test_LbScreen_Terminate.py), main app:
//   src/LbScreen_Initialise_LbScreen_Terminate.cpp   the five functions; src/LbList.cpp runs for real (LbList_GetFirst).
//
// The callees outside the unit are fakes that record the arguments the real ones read; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "LbSCBack.h"
#include "WinScrn.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

int32_t gWinScrnInitialised;
IDirectDraw* gWinScrnDirectDraw;
void* gWinScrnHWnd;
GrafPort* gWinScrnCoopWindow;
TbList gWinScrnModeList;

// ---- the fakes
void ReleaseMode() { ev("ReleaseMode", {gWinScrnInitialised}); }
void LbSurface_RegisterDirectDraw(IDirectDraw* dd) { ev("RegisterDirectDraw", {lab(dd)}); }
void LbSE_OperatorDelete(void* block) { ev("delete", {lab(block)}); }

int32_t IDirectDraw::RestoreDisplayMode() {
    ev("RestoreDisplayMode", {lab(this)});
    return 0;
}
uint32_t NS_MacDoze::IUnknown::Release() {
    ev("Release", {lab(this)});
    return 0;
}
uint32_t NS_MacDoze::IUnknown::AddRef() {
    ev("AddRef", {lab(this)});
    return 0;
}
int32_t IDirectDraw_FlipToGDISurface(IDirectDraw* self) {
    ev("FlipToGDISurface", {lab(self)});
    return 0;
}
namespace NS_MacDoze {
void DestroyWindow(GrafPort* window) { ev("DestroyWindow", {lab(window)}); }
int32_t MessageBoxA(GrafPort* window, const char* text, const char* caption, uint32_t flags) {
    ev("MessageBoxA", {lab(window), static_cast<int64_t>(flags)}, {text, caption});
    return 0;
}
int32_t SetWindowPos(GrafPort* window, GrafPort* after, int32_t x, int32_t y, int32_t cx, int32_t cy, uint32_t flags) {
    ev("SetWindowPos", {lab(window), static_cast<int32_t>(reinterpret_cast<intptr_t>(after)), x, y, cx, cy, static_cast<int64_t>(flags)});
    return 0;
}
}  // namespace NS_MacDoze

extern "C" {

uint64_t tpw_ws_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"init", &gWinScrnInitialised}, {"dd", &gWinScrnDirectDraw}, {"hwnd", &gWinScrnHWnd},
        {"coop", &gWinScrnCoopWindow},  {"list", &gWinScrnModeList},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ws_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ws_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TbScreenModeFind, mCursor));
    out[n++] = static_cast<int64_t>(sizeof(TbListNode));
    out[n++] = static_cast<int64_t>(offsetof(TbDisplayMode, mWidth));
    out[n++] = static_cast<int64_t>(sizeof(TbList));
    return n;
}

void tpw_ws_term() { LbScreen_Terminate(); }
void tpw_ws_show(const char* format, int32_t a, int32_t b) { LbApp_ShowMessage(format, a, b); }
uint64_t tpw_ws_getdd(int32_t addRef) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LbScreen_GetDirectDraw(addRef))); }
int32_t tpw_ws_first(uint64_t find, uint64_t mode) {
    return LbScreen_ModeFindFirst(reinterpret_cast<TbScreenModeFind*>(find), reinterpret_cast<TbScreenMode*>(mode)).mCode;
}
int32_t tpw_ws_next(uint64_t find, uint64_t mode) {
    return LbScreen_ModeFindNext(reinterpret_cast<TbScreenModeFind*>(find), reinterpret_cast<TbScreenMode*>(mode)).mCode;
}

}  // extern "C"
