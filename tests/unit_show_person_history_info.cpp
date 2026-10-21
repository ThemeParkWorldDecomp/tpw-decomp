// Test library for show_person_history_info (libtpw_unit_show_person_history_info.dylib), main app:
//   src/game/keyboard.cpp
// CPeep::ReportAllEvents is a recording fake; gIntrface is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "intrface.h"
#include "ThingReceivers.h"
#include "keytable_actions.h"

using namespace calllog;

CIntrface gIntrface;   // 0x102dc1ac

void CPeep::ReportAllEvents() const {
    ev("ReportAllEvents", {lab(const_cast<CPeep*>(this))});
}

extern "C" {

uint64_t tpw_shpi_addr(const char* name) {
    if (std::strcmp(name, "gIntrface") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gIntrface));
    return 0;
}

uint64_t tpw_shpi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_shpi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mPointedThing));
    return n;
}

uint32_t tpw_shpi_run() { return static_cast<uint32_t>(show_person_history_info()); }

}  // extern "C"
