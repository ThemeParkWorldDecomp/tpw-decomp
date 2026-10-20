// Test library for AnaglyphToggle (unit v5715; libtpw_unit_AnaglyphToggle.dylib, tests/test_AnaglyphToggle.py), main app:
//   src/keytables_enable_AnaglyphToggle.cpp
//
// gei_Display is defined here as raw storage under the program's symbol name (the CDisplay constructor is not linked).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

int32_t AnaglyphToggle();

alignas(16) uint8_t gDisplayStorage[0x1000] __asm__("_gei_Display");

extern "C" {

uint64_t tpw_ana_addr(const char* name) {
    if (std::strcmp(name, "flags") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gDisplayStorage));
    return 0;
}

uint64_t tpw_ana_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ana_layout(int64_t* out) {
    (void)out;
    return 0;
}

uint32_t tpw_ana_toggle() { return static_cast<uint32_t>(AnaglyphToggle()); }

}  // extern "C"
