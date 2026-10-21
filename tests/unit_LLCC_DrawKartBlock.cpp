// Test library for LLCC_DrawKartBlock (unit v5536; libtpw_unit_LLCC_DrawKartBlock.dylib, tests/test_LLCC_DrawKartBlock.py), main app:
//   src/game/park/LLCellRide.cpp
//
// The callees (CMap::IsXYValid, LLCC_SystemGetRideTrack, ADD_DIRECTION, CMapCell::Modify) are fakes that record their arguments and
// answer from the test's queues. gGameWorld is defined here; the test points it at a small block (cells are only addressed).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld;

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}

int32_t LLCC_SystemGetRideTrack() {
    ev("GetRideTrack", {});
    return static_cast<int32_t>(ret("GetRideTrack"));
}

uint8_t ADD_DIRECTION(uint8_t first, uint8_t second) {
    ev("ADD_DIRECTION", {first, second});
    return static_cast<uint8_t>(first + second * 3);
}

int32_t CMapCell::Modify(CELL_MODES mode, int32_t a, int32_t b) {
    int64_t index = (reinterpret_cast<intptr_t>(this) - (reinterpret_cast<intptr_t>(gGameWorld) + 0x2d8)) / 0x44;
    ev("Modify", {index, static_cast<int32_t>(mode), a, b});
    return static_cast<int32_t>(ret("Modify"));
}

extern "C" {

uint64_t tpw_kb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_kb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_kb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    return n;
}

int32_t tpw_kb_draw(int32_t x, int32_t y, int32_t c, int32_t a, int32_t b, uint32_t dir) {
    return LLCC_DrawKartBlock(x, y, c, a, b, static_cast<uint8_t>(dir));
}

}  // extern "C"
