// Test library for LbScreen_RestoreMode (unit v5761; libtpw_unit_LbScreen_RestoreMode.dylib, tests/test_LbScreen_RestoreMode.py), bullfrog:
//   src/CreateDirectDrawObject_LbScreen_PartSwap_LbScreen_RestoreMode.cpp
//
// The callees outside the unit are recording fakes; gWinScrnDirectDraw is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "WinScrn.h"
#include "LbSurface.h"

using namespace calllog;

IDirectDraw* gWinScrnDirectDraw;

namespace {
char gDdObject[16];
}

void ReleaseMode() { ev("ReleaseMode", {}); }

void LbSurface_RegisterDirectDraw(IDirectDraw* dd) { ev("RegisterDirectDraw", {lab(dd)}); }

int32_t IDirectDraw::RestoreDisplayMode() {
    ev("RestoreDisplayMode", {lab(this)});
    return 0;
}

uint32_t NS_MacDoze::IUnknown::Release() {
    ev("Release", {lab(this)});
    return 0;
}

extern "C" {

uint64_t tpw_rm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"dd", &gWinScrnDirectDraw},
        {"ddobj", gDdObject},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

int32_t tpw_rm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbResult));
    return n;
}

uint32_t tpw_rm_run() { return static_cast<uint32_t>(LbScreen_RestoreMode().mCode); }

}  // extern "C"
