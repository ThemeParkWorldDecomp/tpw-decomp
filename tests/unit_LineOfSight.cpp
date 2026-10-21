// Test library for the line of sight and the path smoothing (unit w195; libtpw_unit_LineOfSight.dylib, tests/test_LineOfSight.py), main app:
//   src/game/park/LineOfSight.cpp   bresenham_start, line_of_sight, line_of_sight_cleanup, wallhug_cleanup, wallhug_tricky and the eight globals.
//
// The callees outside the unit are fakes that record what the real ones read: MAP_wall_in_way answers from a wall table of 256 x 256 squares
// (a byte per square, bit d = a wall in direction d) that the case loads; wallhug_trivial plans a walk (kind 0 to 2: a real walk of adjacent
// squares, x first, y first or alternating; kind 3: the test's queues "wh_count", "wh_pt", "wh_ret") and answers 0x70000000 when a
// planned walk is longer than the case's limit; UDebug records its format.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "TPCamera.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using calllog::ev;
using calllog::ret;

namespace {
uint8_t gWalls[256 * 256];   // gWalls[x * 256 + y]
int32_t gPlanKind;
int32_t gPlanLimit;
bool gQuiet;  // bit 8 of the fill word: the wall calls are not logged (the 4096-event log would overflow)
}  // namespace

// ---- the fakes
int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    if (!gQuiet) ev("MAP_wall_in_way", {x, z, dir, static_cast<int32_t>(mode)});
    const uint8_t mask = gWalls[x * 256 + z];
    return dir < 8 ? ((mask >> dir) & 1) : 0;
}

void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

// The harness queues hold 64 values: each fake queue is spread over keys "name:chunk" of 32 values (the last chunk repeats).
static int32_t gQn[3];
static int64_t Q(const char* key, int32_t index) {
    int32_t chunk = gQn[index]++ / 32;
    if (chunk > 15) chunk = 15;
    return ret(key, chunk, 0);
}

int32_t wallhug_trivial(wallhug_path* path, NavMode mode) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(path);
    ev("wallhug_trivial", {static_cast<int32_t>(mode), bytes[2], bytes[3], bytes[4], bytes[5]});
    if (gPlanKind < 3) {
        // A real walk from the start to the end: one square per step, in the order the kind says.
        uint8_t pts[2 * 512];
        int32_t n = 0;
        int32_t x = bytes[2];
        int32_t y = bytes[3];
        const int32_t ex = bytes[4];
        const int32_t ey = bytes[5];
        while (x != ex || y != ey) {
            if (n >= gPlanLimit) return 0x70000000;
            bool stepX;
            if (gPlanKind == 0) stepX = x != ex;
            else if (gPlanKind == 1) stepX = y == ey;
            else stepX = (n % 2 == 0 && x != ex) || y == ey;
            if (stepX) x += ex > x ? 1 : -1;
            else y += ey > y ? 1 : -1;
            pts[2 * n] = static_cast<uint8_t>(x);
            pts[2 * n + 1] = static_cast<uint8_t>(y);
            ++n;
        }
        path->mCount = static_cast<uint8_t>(n);
        for (int32_t i = 0; i < n; ++i) {
            path->mPoints[i].mX = pts[2 * i];
            path->mPoints[i].mY = pts[2 * i + 1];
        }
        return static_cast<int32_t>(Q("wh_ret", 2));
    }
    const int64_t count = Q("wh_count", 0) & 0xff;
    path->mCount = static_cast<uint8_t>(count);
    for (int64_t i = 0; i < count; ++i) {
        path->mPoints[i].mX = static_cast<uint8_t>(Q("wh_pt", 1));
        path->mPoints[i].mY = static_cast<uint8_t>(Q("wh_pt", 1));
    }
    return static_cast<int32_t>(Q("wh_ret", 2));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_los_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"xmajor", &gBresenhamXMajor}, {"xdir", &gBresenhamXDir}, {"ydir", &gBresenhamYDir}, {"err", &gBresenhamError},
        {"dy", &gBresenhamDY},         {"dx", &gBresenhamDX},     {"mode", &gWallhugNavMode}, {"step", &gWallhugStep[0][0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable like every Ptr32 the headers hold).
uint64_t tpw_los_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The sizes the module relies on (HOST_LAYOUT).
int32_t tpw_los_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(wallhug_path));
    out[n++] = static_cast<int64_t>(offsetof(wallhug_path, mPoints));
    return n;
}

// Loads the case's wall table (every square `fill`, then the entries x << 16 | y << 8 | mask) and the planner (kind, limit).
void tpw_los_setup(int32_t kind, int32_t limit, uint32_t fill, const uint32_t* entries, int32_t count) {
    std::memset(gWalls, static_cast<int>(fill), sizeof(gWalls));
    for (int32_t i = 0; i < count; ++i) gWalls[((entries[i] >> 16) & 0xff) * 256 + ((entries[i] >> 8) & 0xff)] = static_cast<uint8_t>(entries[i]);
    gQuiet = (fill & 0x100) != 0;
    gQn[0] = gQn[1] = gQn[2] = 0;
    gPlanKind = kind;
    gPlanLimit = limit;
}

// A waypoint travels in the top half of a register: x in bits 24 to 31, y in bits 16 to 23.
static wallhug_waypoint Waypoint(uint32_t word) {
    return wallhug_waypoint{static_cast<uint8_t>(word >> 24), static_cast<uint8_t>(word >> 16)};
}

void tpw_los_bresenham(uint32_t from, uint32_t to) { bresenham_start(Waypoint(from), Waypoint(to)); }
int32_t tpw_los_sight(uint32_t from, uint32_t to) { return line_of_sight(Waypoint(from), Waypoint(to)); }
int32_t tpw_los_cleanup(uint64_t path, uint32_t first) { return line_of_sight_cleanup(reinterpret_cast<wallhug_path*>(path), first); }
uint32_t tpw_los_wallhug_cleanup(uint64_t path, uint32_t result) { return wallhug_cleanup(reinterpret_cast<wallhug_path*>(path), result); }
uint32_t tpw_los_tricky(uint64_t path, int32_t mode) { return wallhug_tricky(reinterpret_cast<wallhug_path*>(path), static_cast<NavMode>(mode)); }

}  // extern "C"
