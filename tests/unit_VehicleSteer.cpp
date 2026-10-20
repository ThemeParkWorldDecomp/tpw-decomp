// Test library for the ride vehicles' steering code (unit w203; libtpw_unit_VehicleSteer.dylib, tests/test_VehicleSteer.py), main app:
//   src/Vehicle_steer.cpp   the 23 functions of vehicle.cpp from perp to Pilot::is_finished_path, and the six globals it defines
//
// The callees outside the unit are fakes that record what the real ones read: the two candidate walks (0x101356b8, 0x1013598c), Vehicle::recalc_path
// (0x10133be4) and Vehicle::next_step (0x10133564). A walk does not call the callback it is given (the module tests the three callbacks directly): it
// adds the case's queued numbers to gVehSteerSum, as the callbacks would, and the obstacle walk sets gVehAvoidNearest for do_avoid_obstacle. FixMul,
// FixDiv, isqrt and xaxis_circle_intersect are the real sources. Nothing else of other files is needed.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "VehicleSteer.h"
#include "World.h"

using namespace calllog;

namespace {

// A queued number as the 32-bit word the PowerPC stub adds to the Mac's memory.
uint32_t Queued(const char* key) { return static_cast<uint32_t>(ret(key, 0)); }

// gVehSteerSum plus the queued (cand_x, cand_y), wrapping like the Mac's add.
void AddQueuedToSum() {
    gVehSteerSum.x = static_cast<Fixed>(static_cast<uint32_t>(gVehSteerSum.x) + Queued("cand_x"));
    gVehSteerSum.y = static_cast<Fixed>(static_cast<uint32_t>(gVehSteerSum.y) + Queued("cand_y"));
}

Vector2d* AsVector(uint64_t address) { return reinterpret_cast<Vector2d*>(static_cast<uintptr_t>(address)); }
Vehicle* AsVehicle(uint64_t address) { return reinterpret_cast<Vehicle*>(static_cast<uintptr_t>(address)); }
Pilot* AsPilot(uint64_t address) { return reinterpret_cast<Pilot*>(static_cast<uintptr_t>(address)); }
const Obstacle* AsObstacle(uint64_t address) { return reinterpret_cast<const Obstacle*>(static_cast<uintptr_t>(address)); }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))

// 0x101356b8: the walk over the vehicles around; records the vehicle, the radius and the callback, adds what the callbacks would have summed.
void callback_collision_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Vehicle*)) {
    ev("callback_collision_candidates", {lab(&vehicle), radius, lab(reinterpret_cast<const void*>(function))});
    AddQueuedToSum();
}

// 0x1013598c (an empty function on the Mac): the same walk over the obstacles; for do_avoid_obstacle it also sets the nearest hit (the queue
// "cand_near", else the global as it is).
void callback_obstacle_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Obstacle*)) {
    ev("callback_obstacle_candidates", {lab(&vehicle), radius, lab(reinterpret_cast<const void*>(function))});
    AddQueuedToSum();
    if (function == do_avoid_obstacle) gVehAvoidNearest = static_cast<Fixed>(ret("cand_near", static_cast<int64_t>(gVehAvoidNearest)));
}

// 0x10133be4: the path search (translated by unit w209, a fake here); records the vehicle, the target's two words and the mode; answers 0 (the module's
// PowerPC stub leaves r3 as it was and none of the unit's functions reads the answer).
int32_t Vehicle::recalc_path(const Vector2d& target, RecalcPathMode mode) {
    ev("Vehicle::recalc_path", {lab(this), target.x, target.y, static_cast<int32_t>(mode)});
    return 0;
}

// 0x10133564: one step along the path (untranslated).
void Vehicle::next_step() { ev("Vehicle::next_step", {lab(this)}); }

extern "C" {

// The host address of a global or a function by the module's name for it (ADDR's keys).
uint64_t tpw_vst_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"zero", &gVehZero},
        {"one", &gVehOne},
        {"sum", &gVehSteerSum},
        {"range", &gVehAvoidRange},
        {"nearest", &gVehAvoidNearest},
        {"list", &gVehBehaviours},
        {"do_separation", reinterpret_cast<const void*>(&do_separation)},
        {"do_repel_obstacle", reinterpret_cast<const void*>(&do_repel_obstacle)},
        {"do_avoid_obstacle", reinterpret_cast<const void*>(&do_avoid_obstacle)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_vst_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_vst_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Vehicle, mTarget));
    out[n++] = static_cast<int64_t>(sizeof(Vehicle));
    out[n++] = static_cast<int64_t>(sizeof(BehaviourEntry));
    out[n++] = static_cast<int64_t>(offsetof(BehaviourEntry, mBehaviour));
    out[n++] = static_cast<int64_t>(sizeof(BehaviourList));
    out[n++] = static_cast<int64_t>(sizeof(Obstacle));
    out[n++] = static_cast<int64_t>(offsetof(Obstacle, mRadius));
    return n;
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t); a Vector2d answer is written to `out`, as the Mac's hidden pointer

void tpw_vst_perp(uint64_t out, uint64_t v) { *AsVector(out) = perp(*AsVector(v)); }
void tpw_vst_normalise(uint64_t out, uint64_t v) { *AsVector(out) = normalise(*AsVector(v)); }
void tpw_vst_truncate(uint64_t out, uint64_t v, int32_t max) { *AsVector(out) = truncate(*AsVector(v), max); }

void tpw_vst_point_to_local(uint64_t out, uint64_t vehicle, uint64_t p) { *AsVector(out) = AsVehicle(vehicle)->point_to_local(*AsVector(p)); }
void tpw_vst_vec_to_global(uint64_t out, uint64_t vehicle, uint64_t v) { *AsVector(out) = AsVehicle(vehicle)->vec_to_global(*AsVector(v)); }
void tpw_vst_predict_pos(uint64_t out, uint64_t vehicle, int32_t t) { *AsVector(out) = AsVehicle(vehicle)->predict_pos(t); }

void tpw_vst_clear_behaviours() { Vehicle::clear_behaviours(); }

// A Vehicle from the heap, for the destructor's delete form (the arena's blocks are not deletable).
uint64_t tpw_vst_new() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new Vehicle)); }

// form 0: the destructor in place; 1: delete (the Mac's flag above zero); 2: delete of NULL
void tpw_vst_dtor(uint64_t vehicle, int32_t form) {
    if (form == 0) {
        AsVehicle(vehicle)->~Vehicle();
    } else if (form == 1) {
        delete AsVehicle(vehicle);
    } else {
        Vehicle* none = nullptr;
        delete none;
    }
}

// The behaviours: the Mac passes the behaviour object in r4, which none of the five reads; the wrappers take (and ignore) its address as well.
void tpw_vst_seek(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Seek seek;
    *AsVector(out) = seek.calculate(*AsVehicle(vehicle));
}
void tpw_vst_arrival(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Arrival arrival;
    *AsVector(out) = arrival.calculate(*AsVehicle(vehicle));
}
void tpw_vst_separation(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Separation separation;
    *AsVector(out) = separation.calculate(*AsVehicle(vehicle));
}
void tpw_vst_repel_obstacle(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Repel_Obstacle repel;
    *AsVector(out) = repel.calculate(*AsVehicle(vehicle));
}
void tpw_vst_avoid_obstacle(uint64_t out, uint64_t behaviour, uint64_t vehicle) {
    (void)behaviour;
    Avoid_Obstacle avoid;
    *AsVector(out) = avoid.calculate(*AsVehicle(vehicle));
}

void tpw_vst_do_separation(uint64_t me, uint64_t other) { do_separation(*AsVehicle(me), AsVehicle(other)); }
void tpw_vst_do_repel_obstacle(uint64_t vehicle, uint64_t obstacle) { do_repel_obstacle(*AsVehicle(vehicle), AsObstacle(obstacle)); }
void tpw_vst_do_avoid_obstacle(uint64_t vehicle, uint64_t obstacle) { do_avoid_obstacle(*AsVehicle(vehicle), AsObstacle(obstacle)); }

void tpw_vst_pilot_reset(uint64_t pilot, int32_t x, int32_t y, int32_t vx, int32_t vy) { AsPilot(pilot)->reset(x, y, vx, vy); }
void tpw_vst_pilot_new_target(uint64_t pilot, int32_t x, int32_t y) { AsPilot(pilot)->new_target(x, y); }
void tpw_vst_pilot_move(uint64_t pilot) { AsPilot(pilot)->move(); }
void tpw_vst_pilot_get_pos(uint64_t pilot, uint64_t out_x, uint64_t out_y) {
    AsPilot(pilot)->get_pos(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(out_x)), *reinterpret_cast<int32_t*>(static_cast<uintptr_t>(out_y)));
}
void tpw_vst_pilot_set_speed(uint64_t pilot, float speed) { AsPilot(pilot)->set_speed(speed); }

int32_t tpw_vst_vehicle_finished(uint64_t vehicle) { return AsVehicle(vehicle)->is_finished_path(); }
int32_t tpw_vst_pilot_finished(uint64_t pilot) { return AsPilot(pilot)->is_finished_path(); }

}  // extern "C"
