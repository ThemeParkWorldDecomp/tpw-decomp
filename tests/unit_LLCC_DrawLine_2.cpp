// Test library for LLCC_DrawLine, CLBGlobe::~CLBGlobe and CLBScreenComponentDownloadGlobe::WindowMessagesFn (unit v51031;
// libtpw_unit_LLCC_DrawLine_2.dylib, tests/test_LLCC_DrawLine_2.py), main app:
//   src/MapCell_kinds_LLCC_DrawLine_2.cpp
//
// Callees outside the unit are recording fakes (CMap::IsXYValid, CMapCell::Modify, the online globe's WindowMessagesFn). The virtual calls
// (slots 0x24, 0x28, 0x3c) land on a probe class whose vtable has the same order as CLBScreenComponent's and the online globe's: an arena
// block whose first word is a probe's vptr stands for any screen object. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "MapCell.h"
#include "World.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files, defined here
int32_t gValidTypeChangeLock;
int32_t gCalcNeighboursJoinEntry;
CWorld* gGameWorld;
CLBScreenSystem* gLobbyScreens;
OnlineParkSelection* gOnlineParkSelection;

namespace {

struct Probe {
    virtual ~Probe() {}
    virtual void Init() { ev("unexpected", {1}); }
    virtual void Tick() { ev("unexpected", {2}); }
    virtual void Render() { ev("unexpected", {3}); }
    virtual void Close() { ev("unexpected", {4}); }
    virtual void WindowMessagesFn(int32_t, int32_t, int32_t) { ev("unexpected", {5}); }
    virtual uint32_t ProcessCancel() { ev("unexpected", {6}); return 0; }
    virtual void Show() { ev("virt", {lab(this), 0x24, 0}); }
    virtual void Hide() { ev("virt", {lab(this), 0x28, 0}); }
    virtual int32_t IsVisible() { ev("unexpected", {7}); return 0; }
    virtual void RegisterGlobe(CLBGlobe*) { ev("unexpected", {8}); }
    virtual int32_t IsCityWanted(CFSOnlineCity*) { ev("unexpected", {9}); return 0; }
    virtual void Slot3C(CLBGlobe* globe) { ev("virt", {lab(this), 0x3c, lab(globe)}); }
};

}  // namespace

// ---- the fakes
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}

int32_t CMapCell::Modify(CELL_MODES mode, int32_t a, int32_t b) {
    int64_t index = this - &gGameWorld->mMapCells[0];
    ev("Modify", {index, static_cast<int32_t>(mode), a, b, gCalcNeighboursJoinEntry, gValidTypeChangeLock});
    return static_cast<int32_t>(ret("Modify"));
}

void CLBScreenComponentOnlineGlobe_WindowMessagesFn(CLBScreenComponentOnlineGlobe* globe, int32_t message, int32_t a, int32_t b) {
    ev("OnlineWMF", {lab(globe), message, a, b});
}

extern "C" {

uint64_t tpw_ldl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"lock", &gValidTypeChangeLock}, {"join", &gCalcNeighboursJoinEntry}, {"world", &gGameWorld},
        {"screens", &gLobbyScreens},     {"sel", &gOnlineParkSelection},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ldl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ldl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenComponentDownloadGlobe, mF70));
    out[n++] = static_cast<int64_t>(sizeof(CLBScreenComponentDownloadGlobe));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mOnlineIslands));
    out[n++] = static_cast<int64_t>(sizeof(CLBScreenSystem));
    out[n++] = static_cast<int64_t>(offsetof(CLBGlobe, mScreen));
    out[n++] = static_cast<int64_t>(sizeof(CLBGlobe));
    out[n++] = static_cast<int64_t>(offsetof(CLBGlobeCityFlag, mCity));
    out[n++] = static_cast<int64_t>(sizeof(CLBGlobeCityFlag));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase));
    return n;
}

// Makes the block a screen object: its first word becomes the probe's vptr.
void tpw_ldl_probe(uint64_t address) {
    static Probe probe;
    std::memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(address)), &probe, sizeof(void*));
}

// Makes the block a CLBGlobe (its vptr); the fields are written by the test.
void tpw_ldl_globe(uint64_t address) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(address))) CLBGlobe(); }

int32_t tpw_ldl_drawline(int32_t mode, int32_t x, int32_t y, uint64_t endX, uint64_t endY) {
    return LLCC_DrawLineBody(static_cast<CELL_MODES>(mode), x, y, reinterpret_cast<int32_t*>(static_cast<uintptr_t>(endX)),
                         reinterpret_cast<int32_t*>(static_cast<uintptr_t>(endY)));
}

// The complete destructor (no delete storage): the compiler's delete is not the function's text.
uint32_t tpw_ldl_dtor(uint64_t address) {
    CLBGlobe* globe = reinterpret_cast<CLBGlobe*>(static_cast<uintptr_t>(address));
    if (globe == nullptr) return 0;
    globe->~CLBGlobe();
    return 1;
}

void tpw_ldl_wmf(uint64_t address, int32_t message, int32_t a, int32_t b) {
    auto* globe = reinterpret_cast<CLBScreenComponentDownloadGlobe*>(static_cast<uintptr_t>(address));
    globe->CLBScreenComponentDownloadGlobe::WindowMessagesFn(message, a, b);
}

}  // extern "C"
