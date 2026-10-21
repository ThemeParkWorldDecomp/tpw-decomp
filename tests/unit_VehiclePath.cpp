// Test library for the vehicles' path following (unit w209; libtpw_unit_VehiclePath.dylib, tests/test_VehiclePath.py), main app:
//   src/game/rides/vehicle.cpp          Follow_Path::calculate, Vehicle::next_step, recalc_path and is_stuck, Pilot::init_behaviours, Pilot::Pilot
//   src/game/rides/VehicleCallbacks.cpp  map_global_to_local, map_fn
//
// Real code besides the unit: unit w203's steering code (src/game/rides/vehicle.cpp: truncate, normalise, perp, is_finished_path, set_speed, clear_behaviours and the
// globals gVehZero, gVehOne and the behaviour list), FixMul, FixDiv and isqrt. The callees outside are fakes that record what the real ones read, with the same
// event names and arguments as the module's PowerPC stubs: wallhug_tricky (0x10136e7c; it also "plans": it copies the case's path bytes into the buffer),
// CMap::GetTimeStampCell, CMap::MakeNewTimeStamp and CMap::IsXYValid (the map is reported as its offset from gGameWorld), implement_nav_rulesA2B
// (0x10135a70), CTRACE, and the twelve behaviours of the case's pool (their calculate adds nothing but a queued force). The two candidate walks only exist
// because Vehicle_steer.cpp names them; this module never reaches them.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCamera.h"
#include "VehicleSteer.h"
#include "World.h"

using namespace calllog;

CWorld* gGameWorld;
Behaviour* gVehBehaviourTable[8] = {}; // Outside-unit storage fake; cases supply the table.

namespace {

const uint8_t* gSource = nullptr;   // the bytes the fake wallhug_tricky plans: the count at 0, the squares from 6 (the 0x208 bytes of a wallhug_path)

// The map's position as the fakes record it: the offset from gGameWorld (0x2d8 when GetMap did its arithmetic).
int32_t MapOffset(const void* map) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(map) - reinterpret_cast<uintptr_t>(gGameWorld)); }

// A behaviour of the case's pool: the force it asks for comes from the queues force_x and force_y.
class TestBehaviour final : public Behaviour {
public:
    Vector2d calculate(Vehicle& vehicle) override {
        ev("Behaviour::calculate", {lab(this), lab(&vehicle)});
        const Fixed x = static_cast<Fixed>(ret("force_x", 0));
        const Fixed y = static_cast<Fixed>(ret("force_y", 0));
        return Vector2d(x, y);
    }
};

constexpr int kPool = 12;
TestBehaviour gPool[kPool];

Vector2d* AsVector(uint64_t address) { return reinterpret_cast<Vector2d*>(static_cast<uintptr_t>(address)); }
Vehicle* AsVehicle(uint64_t address) { return reinterpret_cast<Vehicle*>(static_cast<uintptr_t>(address)); }

}  // namespace

// ---- the fakes: one per callee outside the unit

// 0x10136e7c: the wall hugger. Records the buffer, the mode and the four bytes of the start and the end; then, when the module gave it a plan, copies the count
// (byte 0) and the squares (bytes 6 to 0x207) into the buffer; answers the queue wh_answer (0x70000000 means no path).
uint32_t wallhug_tricky(wallhug_path* path, NavMode mode) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(path);
    ev("wallhug_tricky", {lab(path), static_cast<int32_t>(mode), bytes[2], bytes[3], bytes[4], bytes[5]});
    if (gSource != nullptr) {
        std::memcpy(reinterpret_cast<uint8_t*>(path), gSource, 1);
        std::memcpy(reinterpret_cast<uint8_t*>(path) + 6, gSource + 6, sizeof(wallhug_path) - 6);
    }
    return static_cast<uint32_t>(ret("wh_answer", 1));
}

// 0x10102724 (the map's own stamp of the cell at the map square (x, y)).
uint32_t CMap::GetTimeStampCell(uint32_t x, uint32_t y) {
    ev("CMap::GetTimeStampCell", {MapOffset(this), x, y});
    return static_cast<uint32_t>(ret("CMap::GetTimeStampCell", 0));
}

// 0x101025c4.
uint32_t CMap::MakeNewTimeStamp() {
    ev("CMap::MakeNewTimeStamp", {MapOffset(this)});
    return static_cast<uint32_t>(ret("CMap::MakeNewTimeStamp", 0));
}

// 0x101012f4 (a static function: x in r3, y in r4).
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("CMap::IsXYValid", {x, y});
    return static_cast<int32_t>(ret("CMap::IsXYValid", 1));
}

// 0x10135a70: the nav rules, called through gVehNavRules.
int32_t implement_nav_rulesA2B(int32_t from, int32_t to, NavMode mode) {
    ev("implement_nav_rulesA2B", {from, to, static_cast<int32_t>(mode)});
    return static_cast<int32_t>(ret("implement_nav_rulesA2B", 1));
}

// 0x1006fcbc: the trace; the level and the formatted text.
void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}

// 0x101356b8 and 0x1013598c: named by Vehicle_steer.cpp, never reached by this module's cases.
void callback_collision_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Vehicle*)) {
    ev("callback_collision_candidates", {lab(&vehicle), radius, lab(reinterpret_cast<const void*>(function))});
}

void callback_obstacle_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Obstacle*)) {
    ev("callback_obstacle_candidates", {lab(&vehicle), radius, lab(reinterpret_cast<const void*>(function))});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_vph_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"zero", &gVehZero},
        {"one", &gVehOne},
        {"list", &gVehBehaviours},
        {"path", &gVehPath},
        {"mapdata", &gVehMapData},
        {"world", &gGameWorld},
        {"table", &gVehBehaviourTable[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_vph_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_vph_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Vehicle, mTarget));
    out[n++] = static_cast<int64_t>(sizeof(Vehicle));
    out[n++] = static_cast<int64_t>(sizeof(BehaviourEntry));
    out[n++] = static_cast<int64_t>(offsetof(BehaviourEntry, mBehaviour));
    out[n++] = static_cast<int64_t>(sizeof(BehaviourList));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(gPool[0].mName) - reinterpret_cast<uintptr_t>(&gPool[0]));   // where a behaviour's name is
    out[n++] = static_cast<int64_t>(sizeof(wallhug_path));
    out[n++] = static_cast<int64_t>(kPool);
    return n;
}

// The plan the next wallhug_tricky copies into the buffer (the module's block of 0x208 bytes), or none (0).
void tpw_vph_set_source(uint64_t address) { gSource = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(address)); }

// The address of the pool's behaviour `index` (the module writes its name).
uint64_t tpw_vph_behaviour(int32_t index) {
    Behaviour* behaviour = &gPool[index];
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(behaviour));
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t); a Vector2d answer is written to `out`, as the Mac's hidden pointer

// The Mac passes the behaviour object in r4, which Follow_Path::calculate never reads; the wrapper takes (and ignores) its address as well.
void tpw_vph_follow_path(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Follow_Path follow;
    *AsVector(out) = follow.calculate(*AsVehicle(vehicle));
}

void tpw_vph_next_step(uint64_t vehicle) { AsVehicle(vehicle)->next_step(); }

int32_t tpw_vph_recalc_path(uint64_t vehicle, uint64_t target, int32_t mode) {
    return AsVehicle(vehicle)->recalc_path(*AsVector(target), static_cast<Vehicle::RecalcPathMode>(mode));
}

int32_t tpw_vph_is_stuck(uint64_t vehicle) { return AsVehicle(vehicle)->is_stuck(); }

void tpw_vph_init_behaviours() { Pilot::init_behaviours(); }

// The constructor in place (the Mac's returns `this`).
uint64_t tpw_vph_pilot_ctor(uint64_t address) {
    Pilot* pilot = new (reinterpret_cast<void*>(static_cast<uintptr_t>(address))) Pilot();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pilot));
}

void tpw_vph_map_global_to_local(uint64_t out, uint32_t x, uint32_t y) { *AsVector(out) = map_global_to_local(x, y); }

int32_t tpw_vph_map_fn(int32_t x, int32_t y, uint64_t data) {
    return map_fn(x, y, reinterpret_cast<void*>(static_cast<uintptr_t>(data)));
}

}  // extern "C"
