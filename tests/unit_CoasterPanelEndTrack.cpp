// Test library for CoasterPanelEndTrack (unit v5333; libtpw_unit_CoasterPanelEndTrack.dylib, tests/test_CoasterPanelEndTrack.py), main app:
//   src/ui_entryPricePanel_CoasterPanelEndTrack.cpp   CoasterPanelEndTrack
//
// Fakes (same events as the PowerPC stubs): operator new (size 1 or 8), CMMDefault::CMMDefault, CMMCoaster::CMMCoaster,
// CMouse::AssignMouseMode, and the mode's GetMode virtual. The globals of Mouse.cpp are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Mouse.h"
#include "TPCoaster.h"

using namespace calllog;

int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;

namespace {

struct FakeMode : CMouseMode {
    int32_t GetMode() override {
        ev("GetMode", {});
        return static_cast<int32_t>(ret("GetMode"));
    }
};

FakeMode gFakeMode;
CMouse gPreMouse;

}  // namespace

namespace coaster_end_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size ? size : 4);
}
}  // namespace coaster_end_callee

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

CMMCoaster::CMMCoaster() { ev("CMMCoaster", {newlab(this)}); }
CMMCoaster::~CMMCoaster() {}
void CMMCoaster::OnLeftClick(int32_t, int32_t) {}
void CMMCoaster::OnLeftRelease(int32_t, int32_t) {}
void CMMCoaster::OnRightClick(int32_t, int32_t) {}
int32_t CMMCoaster::GetMode() { return 8; }
void CMMCoaster::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

extern "C" {

uint64_t tpw_cpet_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMouse", &gMouse},
        {"gMouseMode", &gMouseMode},
        {"w1E0", &gMouseWord1E0},
        {"w1E4", &gMouseWord1E4},
        {"premouse", &gPreMouse},
        {"fakemode", &gFakeMode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cpet_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// pointer globals are 8 bytes on the host: the test sets them through these
void tpw_cpet_set(uint32_t mouse_kind, uint32_t mode_kind) {
    gMouse = mouse_kind ? &gPreMouse : nullptr;
    gMouseMode = mode_kind ? &gFakeMode : nullptr;
}
uint64_t tpw_cpet_get_gmouse() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMouse)); }

int32_t tpw_cpet_layout(int64_t*) { return 0; }

void tpw_cpet_run() { CoasterPanelEndTrack(); }

}  // extern "C"
