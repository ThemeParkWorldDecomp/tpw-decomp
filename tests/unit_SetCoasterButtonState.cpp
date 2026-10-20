// Test library for SetCoasterButtonState (unit v5578; libtpw_unit_SetCoasterButtonState.dylib, tests/test_SetCoasterButtonState.py), main app.
// Callees DisableCurrentPylonEditState and HandleCoasterButtonDown are recording fakes; gCoasterEditDown is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"

using namespace calllog;

uint32_t gCoasterEditDown;

void DisableCurrentPylonEditState() { ev("Disable", {}); }

void HandleCoasterButtonDown(uint32_t buttons) { ev("ButtonDown", {static_cast<int64_t>(buttons)}); }

extern "C" {

uint64_t tpw_scb_addr(const char* name) {
    if (std::strcmp(name, "down") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gCoasterEditDown));
    return 0;
}

uint64_t tpw_scb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_scb_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_scb_set(uint32_t state, uint32_t arg) { SetCoasterButtonState(state, arg); }

}  // extern "C"
