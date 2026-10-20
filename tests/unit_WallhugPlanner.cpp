// Test library for the walk planner (unit w196; libtpw_unit_WallhugPlanner.dylib, tests/test_WallhugPlanner.py), main app:
//   src/World_wallhug.cpp   wallhug_continue_trivial, wallhug_trivial and their six globals;
//   src/World_sight.cpp     w195's bresenham_start, line_of_sight and line_of_sight_cleanup, real on the host as on the PowerPC side.
//
// The callees outside the two files are fakes that record what the real ones read: MAP_wall_in_way answers from a wall table of 256 x 256
// squares (a byte per square, bit d = a wall in direction d) that the case loads; UDebug records its format.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "TPCamera.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using calllog::ev;

namespace {
uint8_t gWalls[256 * 256];   // gWalls[x * 256 + y]
bool gQuiet;                 // bit 8 of the fill word: the wall calls are not logged (the 4096-event log would overflow)
}  // namespace

// ---- the fakes
int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    if (!gQuiet) ev("MAP_wall_in_way", {x, z, dir, static_cast<int32_t>(mode)});
    const uint8_t mask = gWalls[x * 256 + z];
    return dir < 8 ? ((mask >> dir) & 1) : 0;
}

void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_whp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"xmajor", &gBresenhamXMajor}, {"xdir", &gBresenhamXDir},   {"ydir", &gBresenhamYDir},
        {"err", &gBresenhamError},     {"dy", &gBresenhamDY},       {"dx", &gBresenhamDX},
        {"mode", &gWallhugNavMode},    {"step", &gWallhugStep[0][0]}, {"rec", &gWallhugRecordHits},
        {"hitstep", &gWallhugHitStep}, {"hitdir", &gWallhugHitDir}, {"hitside", &gWallhugHitSide},
        {"hitcount", &gWallhugHitCount}, {"steps", &gWallhugSteps},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable like every Ptr32 the headers hold).
uint64_t tpw_whp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The sizes the module relies on (HOST_LAYOUT).
int32_t tpw_whp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(wallhug_path));
    out[n++] = static_cast<int64_t>(offsetof(wallhug_path, mPoints));
    return n;
}

// Loads the case's wall table (every square `fill`, then the entries x << 16 | y << 8 | mask).
void tpw_whp_setup(uint32_t fill, const uint32_t* entries, int32_t count) {
    std::memset(gWalls, static_cast<int>(fill), sizeof(gWalls));
    for (int32_t i = 0; i < count; ++i) gWalls[((entries[i] >> 16) & 0xff) * 256 + ((entries[i] >> 8) & 0xff)] = static_cast<uint8_t>(entries[i]);
    gQuiet = (fill & 0x100) != 0;
}

// A waypoint travels in the top half of a register: x in bits 24 to 31, y in bits 16 to 23.
static wallhug_waypoint Waypoint(uint32_t word) {
    return wallhug_waypoint{static_cast<uint8_t>(word >> 24), static_cast<uint8_t>(word >> 16)};
}

uint32_t tpw_whp_continue(uint64_t path, uint32_t start, uint32_t limit) {
    return wallhug_continue_trivial(reinterpret_cast<wallhug_path*>(path), Waypoint(start), limit);
}
int32_t tpw_whp_trivial(uint64_t path, int32_t mode) { return wallhug_trivial(reinterpret_cast<wallhug_path*>(path), static_cast<NavMode>(mode)); }

}  // extern "C"
