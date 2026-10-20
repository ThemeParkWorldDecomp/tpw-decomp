// Test library for UI_2DMap_MapAreaCallback (unit v5751; libtpw_unit_UI_2DMap_MapAreaCallback.dylib), main app:
//   src/UpdateSatellite_UI_2DMap_MapAreaCallback.cpp
//
// CUI2DMap::MapAreaCallback is a fake recording its arguments; gUI2DMap is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

CUI2DMap gUI2DMap;

void CUI2DMap::MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("MapAreaCallback", {lab(this), lab(window), message, a, b});
}

extern "C" {

uint64_t tpw_mac_addr(const char* name) {
    if (std::strcmp(name, "gUI2DMap") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gUI2DMap));
    return 0;
}

uint64_t tpw_mac_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_mac_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_mac_call(uint64_t window, int32_t message, int32_t a, int32_t b) {
    UI_2DMap_MapAreaCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
