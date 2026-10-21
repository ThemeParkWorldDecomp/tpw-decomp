// Test library for the "backtrack" key (unit v5545; libtpw_unit_backtrack.dylib, tests/test_backtrack.py), main app:
//   src/game/keyboard.cpp   backtrack
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the
// globals of other files the unit reads or writes are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "Advisor.h"
#include "LLCellSystem.h"
#include "intrface.h"
#include "Mouse.h"
#include "MapCell.h"
#include "World.h"
#include "TPSound.h"

using namespace calllog;

// ---- globals of other files
CIntrface gIntrface;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
CWorld* gGameWorld;
int32_t gSoundBanks[8];

namespace {

// The mouse mode object the case points gMouseMode at: GetMode is the virtual the original calls through the vtable.
class FakeMode : public CMouseMode {
public:
    int32_t GetMode() override {
        ev("ModeGetMode", {});
        return static_cast<int32_t>(ret("ModeGetMode"));
    }
};

FakeMode gFakeMode;

}  // namespace

// ---- the fakes
CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }

// The vtable of CMMDefault (never called here): empty bodies so the fake constructor links.
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

int32_t LLCC_SystemIsMode(int32_t mode) {
    ev("IsMode", {mode});
    return static_cast<int32_t>(ret("IsMode"));
}

int32_t LLCC_SystemGetMode() {
    ev("SysGetMode", {});
    return static_cast<int32_t>(ret("SysGetMode"));
}

int32_t CMapCell::IsPath() const {
    ev("IsPath", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - reinterpret_cast<uintptr_t>(gGameWorld))});
    return static_cast<int32_t>(ret("IsPath"));
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SendEvent", {entity, bank, event, a, b, c});
    return 0;
}

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("SetMode", {mode, flag ? 1 : 0}); }

void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LeftUp", {x, y, shift, flag}); }

int32_t LLCC_SystemBacktrack(CObject* obj, bool fixBridges) {
    ev("Backtrack", {lab(obj), fixBridges ? 1 : 0});
    return 0;
}

extern "C" {

uint64_t tpw_bt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gIntrface", &gIntrface},       {"w1e0", &gMouseWord1E0}, {"w1e4", &gMouseWord1E4}, {"mouseMode", &gMouseMode},
        {"mouse", &gMouse},              {"world", &gGameWorld},   {"banks", gSoundBanks},   {"modeObj", &gFakeMode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mF24));
    return n;
}

uint32_t tpw_bt_backtrack() { return static_cast<uint32_t>(backtrack()); }

}  // extern "C"
