// Test library for DisableVoteButton and EarmuffsButtonDown (unit v5766; libtpw_unit_DisableVoteButton.dylib), main app:
//   src/game/ui/ui_onlinePanel.cpp
//
// Windows are arena blocks: word +0x200 holds the child GetWindow answers, word +0x204 a tag (1..4). GetWindow, SetState and the
// virtual Enable are fakes that log the tag and their arguments.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "online.h"

using namespace calllog;

InterfaceWindow* gOnlinePanel;

static InterfaceWindow* gWins[5];   // by tag 1..4

namespace {

uint32_t Tag(const void* p) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(p) + 0x204); }

int16_t FakeEnable(InterfaceWindow* self, int16_t on) {
    ev("Enable", {Tag(self), on});
    return 0;
}

void* gTable[40];

void* FakeTable() {
    auto pm = &InterfaceWindow::Enable;
    uintptr_t v[2];
    std::memcpy(v, &pm, sizeof v);
    size_t slot = v[0] / 8;   // arm64 Itanium ABI: the pointer field is the vtable offset, bit 0 of the adjustment marks virtual
    for (void*& e : gTable) e = reinterpret_cast<void*>(&FakeEnable);
    gTable[slot] = reinterpret_cast<void*>(&FakeEnable);
    return gTable;
}

}  // namespace

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(this);
    ev("GetWindow", {Tag(this), id});
    uint32_t child = *reinterpret_cast<const uint32_t*>(p + 0x200);   // the child's tag
    return child < 5 ? gWins[child] : nullptr;
}

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {Tag(this), state, redraw});
    return 0;
}

extern "C" {

uint64_t tpw_dvb_addr(const char* name) {
    if (std::strcmp(name, "gOnlinePanel") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlinePanel));
    return 0;
}

uint64_t tpw_dvb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dvb_layout(int64_t* out) {
    (void)out;
    return 0;
}

// The host's vtable pointer is at +0; the Mac's is at +0x118.
void tpw_dvb_vptr(uint64_t addr) { *reinterpret_cast<void**>(static_cast<uintptr_t>(addr)) = FakeTable(); }

void tpw_dvb_register(uint32_t tag, uint64_t addr) {
    if (tag < 5) gWins[tag] = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(addr));
}

void tpw_dvb_disable(uint32_t flag) { online_callee::DisableVoteButton(flag != 0); }
void tpw_dvb_earmuffs(uint32_t flag) { EarmuffsButtonDown(flag != 0); }

}  // extern "C"
