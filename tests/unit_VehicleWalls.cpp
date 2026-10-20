// Test library for the vehicles' wall avoidance and callbacks (unit w218; libtpw_unit_VehicleWalls.dylib, tests/test_VehicleWalls.py), main app:
//   src/Vehicle_walls.cpp          Avoid_Walls::calculate
//   src/VehicleCallbacks_nav.cpp   implement_nav_rulesA2B, callback_collision_candidates, callback_obstacle_candidates, map_local_to_global
//
// Real code besides the unit: unit w203's steering code (src/Vehicle_steer.cpp: Vehicle::predict_pos and the globals gVehZero and gVehOne), unit w209's
// map_global_to_local and map_fn (src/VehicleCallbacks_map.cpp) and src/Vehicle_path.cpp (it defines gVehMapData), FixMul, FixDiv, isqrt and
// line_circle_intersect. The callees outside are fakes that record what the real ones read, with the same event names and arguments as the module's PowerPC
// stubs: MAP_wall_in_way (0x10101ca4; the answer is the case's wall table entry for the three bytes), CMap::IsXYValid (0x101012f4; the answer is the case's
// valid window), GAME_ASSERTMSG (0x1000dde8; the condition and the text) and the two functions the collision walk calls (callback_a and callback_b: the
// labels of their two arguments). What src/Vehicle_path.cpp calls (wallhug_tricky, the map's time stamps, CTRACE) is faked as that module does and never reached.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "analyser.h"
#include "calllog.h"
#include "MapCell.h"
#include "platform/arena.h"
#include "TPCamera.h"
#include "VehicleSteer.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld;
Behaviour* gVehBehaviourTable[8] = {}; // Outside-unit storage fake.
CWorld*& gGameWorldRef = gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

// The case's wall table: MAP_wall_in_way answers the value of the entry with the same three bytes, else the fallback.
struct WallEntry {
    int32_t x, z, dir, value;
};
constexpr int32_t kWalls = 256;
WallEntry gWalls[kWalls];
int32_t gWallCount = 0;
int32_t gWallFallback = 0;

// The case's valid window: CMap::IsXYValid answers gValid[4] for x in gValid[0]..gValid[1] and y in gValid[2]..gValid[3], else 0.
int32_t gValid[5] = {0, -1, 0, -1, 1};

Vector2d* AsVector(uint64_t address) { return reinterpret_cast<Vector2d*>(static_cast<uintptr_t>(address)); }
Vehicle* AsVehicle(uint64_t address) { return reinterpret_cast<Vehicle*>(static_cast<uintptr_t>(address)); }

// The functions the collision walk calls: the labels of the two vehicles they get.
void CallbackA(const Vehicle& vehicle, const Vehicle* other) { ev("callback_a", {lab(&vehicle), lab(other)}); }
void CallbackB(const Vehicle& vehicle, const Vehicle* other) { ev("callback_b", {lab(&vehicle), lab(other)}); }
void ObstacleA(const Vehicle& vehicle, const Obstacle* other) { ev("callback_a", {lab(&vehicle), lab(other)}); }
void ObstacleB(const Vehicle& vehicle, const Obstacle* other) { ev("callback_b", {lab(&vehicle), lab(other)}); }

}  // namespace

// ---- the fakes: one per callee outside the unit

// 0x10101ca4: the three bytes and the mode (the Mac passes the registers unmasked; the module's stub records the low bytes).
int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode) {
    ev("MAP_wall_in_way", {x, z, dir, static_cast<int32_t>(mode)});
    for (int32_t i = 0; i < gWallCount; i++)
        if (gWalls[i].x == x && gWalls[i].z == z && gWalls[i].dir == dir) return gWalls[i].value;
    return gWallFallback;
}

// 0x101012f4 (a static function: x in r3, y in r4).
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("CMap::IsXYValid", {x, y});
    return x >= gValid[0] && x <= gValid[1] && y >= gValid[2] && y <= gValid[3] ? gValid[4] : 0;
}

// 0x1000dde8: the condition and the text (not formatted).
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// What src/Vehicle_path.cpp calls: this module never reaches them.
uint32_t wallhug_tricky(wallhug_path* path, NavMode mode) {
    ev("wallhug_tricky", {lab(path), static_cast<int32_t>(mode)});
    return 1;
}

uint32_t CMap::GetTimeStampCell(uint32_t x, uint32_t y) {
    ev("CMap::GetTimeStampCell", {x, y});
    return 0;
}

uint32_t CMap::MakeNewTimeStamp() {
    ev("CMap::MakeNewTimeStamp", {});
    return 0;
}

void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_vwl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"zero", &gVehZero},
        {"one", &gVehOne},
        {"mapdata", &gVehMapData},
        {"world", &gGameWorld},
        {"thingarray", &gThingArray[0]},
        {"guard", &gVehNavRulesInit},
        {"flag", &gVehNavRulesAsserted},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_vwl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_vwl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Vehicle, mTarget));
    out[n++] = static_cast<int64_t>(sizeof(Vehicle));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CMapCell, mWho));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellNext));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry::mThing));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell::mWho));
    out[n++] = static_cast<int64_t>(sizeof(CThing::mCellX));
    out[n++] = static_cast<int64_t>(sizeof(CThing::mCellNext));
    return n;
}

// The case's wall table: n entries of four numbers (x, z, dir, value) in `flat`, and the answer for every other triple.
void tpw_vwl_set_walls(const int32_t* flat, int32_t n, int32_t fallback) {
    gWallCount = n < kWalls ? n : kWalls;
    for (int32_t i = 0; i < gWallCount; i++) gWalls[i] = WallEntry{flat[4 * i], flat[4 * i + 1], flat[4 * i + 2], flat[4 * i + 3]};
    gWallFallback = fallback;
}

// The case's valid window and its answer.
void tpw_vwl_set_valid(int32_t x0, int32_t x1, int32_t y0, int32_t y1, int32_t answer) {
    gValid[0] = x0;
    gValid[1] = x1;
    gValid[2] = y0;
    gValid[3] = y1;
    gValid[4] = answer;
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t); a Vector2d answer is written to `out`, as the Mac's hidden pointer

// The Mac passes the behaviour object in r4, which Avoid_Walls::calculate never reads; the wrapper takes (and ignores) its address as well.
void tpw_vwl_avoid_walls(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Avoid_Walls walls;
    *AsVector(out) = walls.calculate(*AsVehicle(vehicle));
}

int32_t tpw_vwl_nav_rules(int32_t from, int32_t to, int32_t mode) { return implement_nav_rulesA2B(from, to, static_cast<NavMode>(mode)); }

// `which` picks callback_a (0) or callback_b (anything else), as the module's two TVectors.
void tpw_vwl_collision(uint64_t vehicle, int32_t radius, int32_t which) {
    callback_collision_candidates(*AsVehicle(vehicle), radius, which == 0 ? CallbackA : CallbackB);
}

void tpw_vwl_obstacle(uint64_t vehicle, int32_t radius, int32_t which) {
    callback_obstacle_candidates(*AsVehicle(vehicle), radius, which == 0 ? ObstacleA : ObstacleB);
}

void tpw_vwl_local_to_global(uint64_t out, uint32_t x, uint32_t y) { *AsVector(out) = map_local_to_global(x, y); }

}  // extern "C"
