// vehicle.cpp: name proven by __sinit_vehicle_cpp in the main application.

#include "Debug.h"
#include "fixmath.h"
#include "ppc_math.h"
#include "Serialise.h"
#include "TPCamera.h"
#include "vector2d.h"
#include "VehicleSteer.h"
#include "World.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

// vehicle.cpp, unit w230: save/load and __sinit_vehicle_cpp. Native runtime boundaries below are support glue.

namespace {
void SwapForFile(Vector2d& v) {
    v.x = serialise::SwapWord(v.x);
    v.y = serialise::SwapWord(v.y);
}
void UnswapForFile(Vector2d& v) {
    v.x = serialise::UnswapWord(v.x);
    v.y = serialise::UnswapWord(v.y);
}

// Mac long and bool retain their distinct helper identities without host-long aliasing or bool normalization.
template<class Value, class Field> int32_t Scalar(void* file, Field& field, const char* name, int32_t save) {
    static_assert(sizeof(Value) == sizeof(Field), "serialized scalar width");
    if constexpr (std::is_same<Value, Field>::value) {
        return save ? SAVEVAR<Value>(file, &field, name, 0) : LOADVAR<Value>(file, &field, name, 0);
    }
    Value value;
    std::memcpy(&value, &field, sizeof(value));
    int32_t result = save ? SAVEVAR<Value>(file, &value, name, 0) : LOADVAR<Value>(file, &value, name, 0);
    if (!save) std::memcpy(&field, &value, sizeof(value));
    return result;
}
int32_t Vector(void* file, Vector2d& field, const char* name, int32_t save) {
    return save ? SAVEVARSTRUCT(file, &field, name, 0) : LOADVARSTRUCT(file, &field, name, 0);
}

Seek gSeek;
Arrival gArrival;
Separation gSeparation;
Avoid_Obstacle gAvoidObstacle;
Repel_Obstacle gRepelObstacle;
Avoid_Walls gAvoidWalls;
Follow_Path gFollowPath;
VehicleRegistrationNode gListNode, gSumNode, gObjectNodes[7];

// Placement construction keeps each real native class identity; it leaves the un-copied name tail alone.
template<class Type> void InitializeBehaviour(Type& object, const char* name, VehicleRuntimeCallback destructor, unsigned index) {
    new (&object) Type;
    VehicleRuntimeCopyName(object.mName, name);
    VehicleRuntimeRegister(&object, destructor, &gObjectNodes[index]);
}
} // namespace

// 0x10134a74, 268 B and 0x10134bc0, 252 B. Each Fixed is converted separately.
template<> int32_t SAVEVARSTRUCT<Vector2d>(void* file, Vector2d* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(8, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    Vector2d copy = *value;
    GAME_ASSERTMSG(1, "should be a struct");
    SwapForFile(copy);
    if (!LbFile_Write(file, &copy, 8, &written).Succeeded() || written != 8) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}
template<> int32_t LOADVARSTRUCT<Vector2d>(void* file, Vector2d* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(8, name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    if (!LbFile_Read(file, value, 8, &read).Succeeded() || read != 8) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should be a struct");
    UnswapForFile(*value);
    return 0;
}
// 0x10134cfc, 236 B and 0x10134e20, 260 B. NavMode is an unvalidated signed word.
template<> int32_t LOADVAR<NavMode>(void* file, NavMode* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
template<> int32_t SAVEVAR<NavMode>(void* file, NavMode* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}

// 0x10130eb4, 2212 B. Every failure is canonicalized, including negative helper failures.
int32_t Vehicle::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version != 0 && version != 1) return 0;
    if (Vector(file, mF40, "force", save)) return 1;
    if (Vector(file, mF38, "formation_pos", save)) return 1;
    if (!save) mF34 = 0;
    if (Vector(file, mForward, "local_xaxis", save)) return 1;
    if (Vector(file, mSide, "local_yaxis", save)) return 1;
    if (Scalar<MacLong>(file, mF00, "mass", save)) return 1;
    if (Scalar<MacLong>(file, mMaxForce, "max_force", save)) return 1;
    if (Scalar<MacLong>(file, mMaxSpeed, "max_speed", save)) return 1;
    if (Scalar<int32_t>(file, mUnreachable, "mCantReachDest", save)) return 1;
    if (Scalar<NavMode>(file, mNavMode, "nav_mode", save)) return 1;
    if (Scalar<int32_t>(file, mNumLoaded, "path_buffer_count", save)) return 1;
    if (Scalar<int32_t>(file, mWaypoint, "path_count", save)) return 1;
    if (Scalar<bool>(file, mPathFlag, "path_finished", save)) return 1;
    if (Scalar<int32_t>(file, mLastProgress, "path_last_progress", save)) return 1;
    if (Scalar<MacULong>(file, mStuckHistory, "path_stuck_buffer", save)) return 1;
    if (Scalar<int32_t>(file, mPathLegA, "path_subpath_dist", save)) return 1;
    if (Scalar<int32_t>(file, mPathLegB, "path_tail_dist", save)) return 1;
    if (Vector(file, mDestination, "path_target_pos", save)) return 1;
    if (Scalar<MacULong>(file, mPathStamp, "path_timestamp", save)) return 1;
    if (Scalar<MacLong>(file, mNumWaypoints, "path_total_count", save)) return 1;
    if (Scalar<int32_t>(file, mPathTotal, "path_total_dist", save)) return 1;
    if (Vector(file, mPos, "pos", save)) return 1;
    if (Scalar<MacLong>(file, mRadius, "radius", save)) return 1;
    for (int32_t p = 0; p < 5; ++p) {
        if (Vector(file, mWaypoints[p], "subpath_buffer[p]", save)) return 1;
        if (Scalar<int32_t>(file, mLegLength[p], "subpath_dist[p]", save)) return 1;
    }
    if (!save) {
        mF30 = 0;
        mTarget = nullptr;
    }
    return Vector(file, mVelocity, "vel", save) ? 1 : 0;
}

Behaviour* gVehBehaviourTable[8] = {&gSeek, &gArrival, &gSeparation, &gAvoidObstacle, &gRepelObstacle, &gAvoidWalls, &gFollowPath, nullptr};

// 0x10134f5c, 616 B. Ten allocated entries, but an end token eighty entries from data, as in the original.
void InitVehicleGlobals() {
    using Callback = VehicleRuntimeCallback;
    gVehZero = 0;
    gVehOne = 0x10000;
    void* raw = VehicleRuntimeAllocate(0x58);
    BehaviourEntry* data = VehicleRuntimeConstruct(raw, Callback::EntryConstructor, nullptr, 8, 10);
    gVehBehaviours.mData = data;
    gVehBehaviours.mTop = data;
    gVehBehaviours.mEnd = reinterpret_cast<BehaviourEntry*>(reinterpret_cast<uintptr_t>(data) + 80 * sizeof(BehaviourEntry));
    VehicleRuntimeRegister(&gVehBehaviours, Callback::ListDestructor, &gListNode);
    gVehSteerSum.x = 0;
    gVehSteerSum.y = 0;
    VehicleRuntimeRegister(&gVehSteerSum, Callback::VectorDestructor, &gSumNode);
    InitializeBehaviour(gSeek, "seek", Callback::SeekDestructor, 0);
    InitializeBehaviour(gArrival, "arrival", Callback::ArrivalDestructor, 1);
    InitializeBehaviour(gSeparation, "separation", Callback::SeparationDestructor, 2);
    InitializeBehaviour(gAvoidObstacle, "avoid_obstacle", Callback::AvoidObstacleDestructor, 3);
    InitializeBehaviour(gRepelObstacle, "repel_obstacle", Callback::RepelObstacleDestructor, 4);
    InitializeBehaviour(gAvoidWalls, "avoid_walls", Callback::AvoidWallsDestructor, 5);
    InitializeBehaviour(gFollowPath, "follow_path", Callback::FollowPathDestructor, 6);
}

#ifndef TPW_VEHICLE_UNIT_RUNTIME
namespace {
void* gEntryAllocation;
void CleanupVehicleEntries() { std::free(gEntryAllocation); }
}
// HOST SUPPORT: native array storage, default initialization, and cleanup. The original callbacks are not executed.
void* VehicleRuntimeAllocate(uint32_t originalBytes) {
    (void)originalBytes;
    gEntryAllocation = std::malloc(8 + 10 * sizeof(BehaviourEntry));
    return gEntryAllocation;
}
BehaviourEntry* VehicleRuntimeConstruct(void* raw, VehicleRuntimeCallback, const VehicleRuntimeCallback*, uint32_t, uint32_t count) {
    auto* entries = reinterpret_cast<BehaviourEntry*>(static_cast<unsigned char*>(raw) + 8);
    for (uint32_t i = 0; i < count; ++i) new (&entries[i]) BehaviourEntry{};
    return entries;
}
void VehicleRuntimeRegister(void*, VehicleRuntimeCallback destructor, VehicleRegistrationNode*) {
    if (destructor == VehicleRuntimeCallback::ListDestructor) std::atexit(CleanupVehicleEntries);
}
char* VehicleRuntimeCopyName(char* destination, const char* source) { return std::strcpy(destination, source); }
namespace {
struct VehicleBootstrap { VehicleBootstrap() { InitVehicleGlobals(); } } gVehicleBootstrap;
}
#endif

// vehicle.cpp (main app, SimTheme Park 2.04b3; unit w203): the ride vehicles' steering. The vector helpers perp, normalise and truncate, the Vehicle's
// geometry (point_to_local, vec_to_global, predict_pos, is_finished_path), clear_behaviours and the destructor, the five behaviours' calculate functions
// with the three callbacks they hand to the candidate walks, and the Pilot's thin members. Everything is 16.16 Fixed; adds, negations and multiplies
// wrap as the PowerPC's do (done in uint32_t). Vehicle::Serialise and __sinit_vehicle_cpp are in Vehicle_serialise.cpp.
// Follow_Path::calculate, recalc_path, next_step, Pilot::Pilot and Pilot::init_behaviours are unit w209's (Vehicle_path.cpp), Avoid_Walls::calculate and
// the candidate walks unit w218's (Vehicle_walls.cpp, VehicleCallbacks_nav.cpp).

// ---- storage of vehicle.cpp globals (initialized in order by InitVehicleGlobals, 0x10134f5c)
Fixed gVehZero = 0;                 // 0x1034ca90
Fixed gVehOne = 0x10000;            // 0x1034ca8c
Vector2d gVehSteerSum;              // 0x1034ca60
Fixed gVehAvoidRange = 0;           // 0x1034ca58
Fixed gVehAvoidNearest = 0;         // 0x1034ca5c
BehaviourList gVehBehaviours = {nullptr, nullptr, nullptr};   // 0x1034ca74

namespace {

// The Mac's add, subf, neg and mullw wrap around; a signed overflow would be undefined on the host.
Fixed Add(Fixed a, Fixed b) { return static_cast<Fixed>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
Fixed Sub(Fixed a, Fixed b) { return static_cast<Fixed>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
Fixed Neg(Fixed a) { return static_cast<Fixed>(0u - static_cast<uint32_t>(a)); }
Fixed Mul(Fixed a, Fixed b) { return static_cast<Fixed>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
Fixed Shl(Fixed a, uint32_t bits) { return static_cast<Fixed>(static_cast<uint32_t>(a) << bits); }

// cmpwi / neg: the absolute value; |INT32_MIN| stays INT32_MIN.
Fixed Abs(Fixed a) { return a < 0 ? Neg(a) : a; }

// labs of the Mac's glue (INT32_MIN stays negative), the idiom of src/Handyman.cpp.
Fixed Labs(Fixed a) { return static_cast<Fixed>(static_cast<uint32_t>(std::labs(static_cast<long>(a)))); }

// mullw / mullw / add: the wrapping sum of two squares.
uint32_t SumOfSquares(uint32_t a, uint32_t b) { return a * a + b * b; }

// The length of v as truncate, Arrival and Avoid_Obstacle compute it (the three of them inline the same code): the root of the sum of the squares of the
// absolute values; when a component reaches 0x1000 or 0x400000 both are shifted right (LOGICAL shifts) first and the root shifted back left. A root of
// 0x2000 or more in the last range (a length of 2^31 or more) is a negative Fixed.
Fixed Length(const Vector2d& v) {
    const uint32_t ax = static_cast<uint32_t>(Abs(v.x));
    const uint32_t ay = static_cast<uint32_t>(Abs(v.y));
    const uint32_t bits = ax | ay;
    if ((bits & 0xfffff000u) == 0) return static_cast<Fixed>(isqrt(SumOfSquares(ax, ay)));
    if ((bits & 0xffc00000u) == 0) return static_cast<Fixed>(isqrt(SumOfSquares(ax >> 8, ay >> 8)) << 8);
    return static_cast<Fixed>(isqrt(SumOfSquares(ax >> 18, ay >> 18)) << 18);
}

// The octagonal distance of is_finished_path, Separation and the obstacle callbacks: ax + ay - (the smaller >> 1), arithmetic shift, signed compares.
Fixed OctagonalDistance(const Vector2d& d) {
    const Fixed ax = Abs(d.x);
    const Fixed ay = Abs(d.y);
    const Fixed smaller = (ax > ay) ? ay : ax;
    return Sub(Add(ax, ay), smaller >> 1);
}

// The waypoint mWaypoint names, with no range check (the Mac addresses this + 0x64 + (mWaypoint << 3)).
const Vector2d& CurrentWaypoint(const Vehicle& vehicle) { return (&vehicle.mWaypoints[0])[vehicle.mWaypoint]; }

// The progress of the path, the code of Vehicle::is_finished_path and of Pilot::is_finished_path (the same instructions at two addresses).
Fixed PathProgress(const Vehicle& vehicle) {
    if (vehicle.mPathFlag == 1) return gVehOne;
    const Vector2d toWaypoint = CurrentWaypoint(vehicle) - vehicle.mPos;
    const Fixed remaining = Add(vehicle.mPathLegB, Add(vehicle.mPathLegA, OctagonalDistance(toWaypoint)));
    if (vehicle.mPathTotal == 0) return gVehOne;
    return Sub(gVehOne, FixDiv(remaining, vehicle.mPathTotal));
}

}  // namespace

// 0x10131788 (100 bytes): the target's direction, minus the velocity. The behaviour (`this`) is not read.
Vector2d Seek::calculate(Vehicle& vehicle) {
    const Vector2d desired = *vehicle.mTarget - vehicle.mPos;
    return desired - vehicle.mVelocity;
}

// 0x1013181c (404 bytes): seek that slows down near the target. off = target - mPos; when its length is zero (gVehZero, re-read) the answer is (0, 0);
// else the speed is mMaxSpeed, or len / 2.0 when that is smaller or equal (FixDiv is called a second time for it, as the original's MIN macro does), and
// the answer is off * (speed / len) - mVelocity.
Vector2d Arrival::calculate(Vehicle& vehicle) {
    const Vector2d offset = *vehicle.mTarget - vehicle.mPos;
    const Fixed length = Length(offset);
    if (length == gVehZero) return Vector2d(0, 0);
    Fixed speed = vehicle.mMaxSpeed;
    if (vehicle.mMaxSpeed >= FixDiv(length, 0x20000)) speed = FixDiv(length, 0x20000);
    return offset * FixDiv(speed, length) - vehicle.mVelocity;
}

// 0x10131a7c (276 bytes): one candidate of Separation. When `other` is another vehicle nearer than the two radii and gVehOne together (and not on top of
// this one), the unit vector away from it is added to gVehSteerSum. gVehOne is read first, before the identity test.
void do_separation(const Vehicle& me, const Vehicle* other) {
    const Fixed one = gVehOne;
    if (other == &me) return;
    const Vector2d away = me.mPos - other->mPos;
    const Fixed distance = OctagonalDistance(away);
    if (distance < Add(me.mRadius, Add(other->mRadius, one)) && distance != gVehZero) {
        const Vector2d unit = away / distance;
        gVehSteerSum.x = Add(gVehSteerSum.x, unit.x);
        gVehSteerSum.y = Add(gVehSteerSum.y, unit.y);
    }
}

// 0x10131c64 (308 bytes): on the last leg of the path (mWaypoint is the last index, or the flag byte is not zero) a vehicle within gVehOne + mRadius of
// the current waypoint answers (0, 0) without looking around; else the sum of do_separation over the vehicles within twice the radius.
Vector2d Separation::calculate(Vehicle& vehicle) {
    if (vehicle.mWaypoint == Sub(vehicle.mNumWaypoints, 1) || vehicle.mPathFlag != 0) {
        const Vector2d toVehicle = vehicle.mPos - CurrentWaypoint(vehicle);
        if (OctagonalDistance(toVehicle) <= Add(gVehOne, vehicle.mRadius)) return Vector2d(0, 0);
    }
    gVehSteerSum = Vector2d(0, 0);
    callback_collision_candidates(vehicle, Shl(vehicle.mRadius, 1), do_separation);
    return gVehSteerSum;
}

// 0x10131dcc (256 bytes): one candidate of Repel_Obstacle. Within the two radii and gVehOne, the direction away from the obstacle, as long as 1 over the
// root of the distance, is added to gVehSteerSum. gVehOne is read before the subtraction and again for the numerator.
void do_repel_obstacle(const Vehicle& vehicle, const Obstacle* obstacle) {
    const Fixed one = gVehOne;
    const Vector2d away = vehicle.mPos - obstacle->mPos;
    const Fixed distance = OctagonalDistance(away);
    if (distance < Add(vehicle.mRadius, Add(obstacle->mRadius, one))) {
        const Vector2d push = away * FixDiv(gVehOne, static_cast<Fixed>(isqrt(static_cast<uint32_t>(distance))));
        gVehSteerSum.x = Add(gVehSteerSum.x, push.x);
        gVehSteerSum.y = Add(gVehSteerSum.y, push.y);
    }
}

// 0x10131f08 (136 bytes): the sum of do_repel_obstacle over the obstacles within twice the radius.
Vector2d Repel_Obstacle::calculate(Vehicle& vehicle) {
    gVehSteerSum = Vector2d(0, 0);
    callback_obstacle_candidates(vehicle, Shl(vehicle.mRadius, 1), do_repel_obstacle);
    return gVehSteerSum;
}

// 0x10131fc8 (220 bytes): one candidate of Avoid_Obstacle. q is the obstacle in the vehicle's frame (kept in ONE local copy, which
// xaxis_circle_intersect takes by reference); it counts when it is ahead (q.x > gVehZero), within gVehAvoidRange, and the vehicle's and the obstacle's
// radii together reach over q.y (labs, so |INT32_MIN| stays negative); the hit t must not be negative and nearer than gVehAvoidNearest (all signed).
// Then gVehAvoidNearest = t and the sum's y becomes -q.y (q.y read from the copy after the call; the x is left alone).
void do_avoid_obstacle(const Vehicle& vehicle, const Obstacle* obstacle) {
    Vector2d q = vehicle.point_to_local(obstacle->mPos);
    if (q.x > gVehZero && Sub(q.x, obstacle->mRadius) <= gVehAvoidRange && Labs(q.y) < Add(vehicle.mRadius, obstacle->mRadius)) {
        const Fixed t = static_cast<Fixed>(xaxis_circle_intersect(q, obstacle->mRadius));
        if (t >= gVehZero && t < gVehAvoidNearest) {
            gVehAvoidNearest = t;
            gVehSteerSum.y = Neg(q.y);
        }
    }
}

// 0x101320e0 (384 bytes): looks 10.0 ahead (predict_pos(0xa0000)); the range is mRadius plus the length of that move. With no hit (gVehAvoidNearest still
// 0x7fffffff) the answer is (0, 0), else the summed force turned to the world frame (vec_to_global).
Vector2d Avoid_Obstacle::calculate(Vehicle& vehicle) {
    gVehSteerSum = Vector2d(0, 0);
    gVehAvoidNearest = 0x7fffffff;
    const Vector2d predicted = vehicle.predict_pos(0xa0000);
    const Fixed length = Length(predicted - vehicle.mPos);
    gVehAvoidRange = Add(vehicle.mRadius, length);
    callback_obstacle_candidates(vehicle, gVehAvoidRange, do_avoid_obstacle);
    if (gVehAvoidNearest == 0x7fffffff) return Vector2d(0, 0);
    return vehicle.vec_to_global(gVehSteerSum);
}

// vehicle.cpp (main app, SimTheme Park 2.04b3; unit w218): Avoid_Walls::calculate, the steering behaviour that keeps a vehicle off the walls of the map. It
// predicts where the vehicle will be (Vehicle::predict_pos by gVehOne), takes the map square the vehicle stands on and the direction of its step (the step to the
// prediction, normalised; a component is "negative" when it is below gVehZero, else "positive") and asks the map (MAP_wall_in_way) about the walls on the sides
// the step heads for. A wall the prediction has crossed or come within the radius of pushes straight back, twice the overlap. When no side does, the corners of
// the diagonal squares it heads for are tried: a corner counts when a wall of the diagonal square lies on it and the two squares beside the vehicle's do not
// shield it, and then, when the line of the step passes within the radius of the corner (line_circle_intersect) no farther along than the step is long, the
// push is the point reached there, taken from the corner and divided by the radius. Otherwise (0, 0). Everything is 16.16 Fixed; adds, subtractions, negations
// and shifts wrap as the PowerPC's do (done in uint32_t). Not translated here: __sinit_vehicle_cpp (no behaviour list it builds holds an Avoid_Walls) and
// Avoid_Walls' vtable and destructor.

namespace {

// MAP_wall_in_way for the map square (x, y) of the vehicle's map and the side `dir` (0 = -y, 1 = +x, 2 = +y, 3 = -x) in the vehicle's mode (read afresh for
// every call); any answer but 0 is a wall. The coordinates are bytes: the squares beside the vehicle's wrap in a byte.
bool Wall(const Vehicle& vehicle, uint32_t x, uint32_t y, uint8_t dir) {
    return MAP_wall_in_way(static_cast<uint8_t>(x), static_cast<uint8_t>(y), dir, vehicle.mNavMode) != 0;
}

// The push away from a corner: the line of the step (from the vehicle's position along d) must pass within the radius of it, at a distance along the step
// that is positive and no longer than the step (length); the push is the point reached there, taken from the corner and divided by the radius.
bool CornerPush(Vehicle& vehicle, Vector2d& d, Fixed length, Vector2d& corner, Vector2d* push) {
    const int32_t t = line_circle_intersect(vehicle.mPos, d, corner, vehicle.mRadius);
    if (t == 0x7fffffff || t <= 0 || t > length) return false;
    *push = ((vehicle.mPos + d * t) - corner) / vehicle.mRadius;
    return true;
}

}  // namespace

// 0x10132298 (2648 bytes). The behaviour object (`this`) is not read.
Vector2d Avoid_Walls::calculate(Vehicle& vehicle) {
    const Vector2d predicted = vehicle.predict_pos(gVehOne);
    const Vector2d cell = map_global_to_local(static_cast<uint32_t>(vehicle.mPos.x), static_cast<uint32_t>(vehicle.mPos.y));
    const uint32_t cx = static_cast<uint32_t>(cell.x);
    const uint32_t cy = static_cast<uint32_t>(cell.y);
    map_fn(cell.x, cell.y, gVehMapData);   // the square's number; the answer is not used

    Vector2d d = predicted - vehicle.mPos;
    const Fixed length = Length(d);
    if (length != gVehZero) d = d / length;

    const bool nx = d.x < gVehZero;
    const bool px = !nx;
    const bool ny = d.y < gVehZero;
    const bool py = !ny;

    // The walls on the sides of the vehicle's own square, in the order +x, -x, +y, -y: when the prediction is beyond the wall's line less the radius (or the
    // line plus the radius, on the -x and -y sides) the push is twice the overlap, along that axis only. A wall that is not reached falls through.
    if (px && Wall(vehicle, cx, cy, 1)) {
        const Vector2d edge = map_local_to_global(cx + 1, 0);
        const Fixed limit = Sub(edge.x, vehicle.mRadius);
        if (limit < predicted.x) return Vector2d(Shl(Sub(limit, predicted.x), 1), 0);
    }
    if (nx && Wall(vehicle, cx, cy, 3)) {
        const Vector2d edge = map_local_to_global(cx, 0);
        const Fixed limit = Add(edge.x, vehicle.mRadius);
        if (predicted.x < limit) return Vector2d(Shl(Sub(limit, predicted.x), 1), 0);
    }
    if (py && Wall(vehicle, cx, cy, 2)) {
        const Vector2d edge = map_local_to_global(0, cy + 1);
        const Fixed limit = Sub(edge.y, vehicle.mRadius);
        if (limit < predicted.y) return Vector2d(0, Shl(Sub(limit, predicted.y), 1));
    }
    if (ny && Wall(vehicle, cx, cy, 0)) {
        const Vector2d edge = map_local_to_global(0, cy);
        const Fixed limit = Add(edge.y, vehicle.mRadius);
        if (predicted.y < limit) return Vector2d(0, Shl(Sub(limit, predicted.y), 1));
    }

    // The corners of the diagonal squares, in the order (+x, +y), (-x, +y), (-x, -y), (+x, -y): the diagonal square has a wall on the corner and neither of the
    // two squares beside the vehicle's has a wall between it and the diagonal one. A corner whose line is not met within the step falls through.
    Vector2d push;
    if (py && px && (Wall(vehicle, cx + 1, cy + 1, 0) || Wall(vehicle, cx + 1, cy + 1, 3)) && !Wall(vehicle, cx, cy + 1, 0) && !Wall(vehicle, cx + 1, cy, 3)) {
        Vector2d corner = map_local_to_global(cx + 1, cy + 1);
        if (CornerPush(vehicle, d, length, corner, &push)) return push;
    }
    if (py && nx && (Wall(vehicle, cx - 1, cy + 1, 0) || Wall(vehicle, cx - 1, cy + 1, 1)) && !Wall(vehicle, cx, cy + 1, 0) && !Wall(vehicle, cx - 1, cy, 1)) {
        Vector2d corner = map_local_to_global(cx, cy + 1);
        if (CornerPush(vehicle, d, length, corner, &push)) return push;
    }
    if (ny && nx && (Wall(vehicle, cx - 1, cy - 1, 2) || Wall(vehicle, cx - 1, cy - 1, 1)) && !Wall(vehicle, cx - 1, cy, 1) && !Wall(vehicle, cx, cy - 1, 2)) {
        Vector2d corner = map_local_to_global(cx, cy);
        if (CornerPush(vehicle, d, length, corner, &push)) return push;
    }
    if (ny && px && (Wall(vehicle, cx + 1, cy - 1, 2) || Wall(vehicle, cx + 1, cy - 1, 3)) && !Wall(vehicle, cx + 1, cy, 3) && !Wall(vehicle, cx, cy - 1, 2)) {
        Vector2d corner = map_local_to_global(cx + 1, cy);
        if (CornerPush(vehicle, d, length, corner, &push)) return push;
    }
    return Vector2d(0, 0);
}

// vehicle.cpp (main app, SimTheme Park 2.04b3; unit w209): the vehicles' path following. Vehicle::recalc_path plans a walk with the wall hugger
// (wallhug_tricky), measures it and loads the first five waypoints; Vehicle::next_step sums the weighted behaviours' forces, steers, takes the step when the
// nav rules allow it and shifts into mStuckHistory whether the step was refused or made no progress; Vehicle::is_stuck counts that history; Follow_Path::calculate
// is the behaviour that replans (the ground changed under the path, or the vehicle is stuck), walks the waypoints and steers at the next one; Pilot::Pilot and
// Pilot::init_behaviours build a pilot and its behaviour list. Everything is 16.16 Fixed; adds, subtractions, negations and shifts wrap as the PowerPC's do
// (done in uint32_t). Not translated here: __sinit_vehicle_cpp (the behaviour objects), Vehicle::Serialise and CPeep's callers. The nav rules
// (implement_nav_rulesA2B) and the candidate walks are unit w218's (VehicleCallbacks_nav.cpp), as is Avoid_Walls::calculate (Vehicle_walls.cpp).

// ---- the globals of vehicle.cpp that unit w209 meets (Vehicle_serialise.cpp owns the typed behaviour table and startup)
wallhug_path gVehPath;                                           // 0x1034c708 (BSS)
void* gVehMapData = nullptr;                                     // 0x1034ca94 (BSS)
VehNavRulesFn gVehNavRules = &implement_nav_rulesA2B;            // 0x102a45a0 (the TVector 0x10266d88 of implement_nav_rulesA2B)

namespace {

// The centre of a map square in 16.16: ((x << 16) + 0x10000) - 0x8000 in each axis (addis 1, addi -0x8000); the square's coordinates are bytes.
Vector2d SquareCentre(const wallhug_waypoint& square) {
    return Vector2d(Sub(Add(Shl(square.mX, 16), 0x10000), 0x8000), Sub(Add(Shl(square.mY, 16), 0x10000), 0x8000));
}

// The Mac indexes the five waypoints and their leg lengths with no range check (the host layout equals the Mac's up to 0xc8, so an index of 5 or more reads the
// fields that follow, as the original does).
Vector2d WaypointAt(const Vehicle& vehicle, int32_t index) {
    Vector2d waypoint;
    std::memcpy(&waypoint, reinterpret_cast<const uint8_t*>(&vehicle) + offsetof(Vehicle, mWaypoints) + static_cast<ptrdiff_t>(index) * 8, sizeof(waypoint));
    return waypoint;
}

Fixed LegAt(const Vehicle& vehicle, int32_t index) {
    Fixed leg;
    std::memcpy(&leg, reinterpret_cast<const uint8_t*>(&vehicle) + offsetof(Vehicle, mLegLength) + static_cast<ptrdiff_t>(index) * 4, sizeof(leg));
    return leg;
}

// The three searches of init_behaviours are one loop: the first table entry whose name (at +4 on the Mac) equals `name`, at most the eight entries; the loop
// stops at the first match (the Mac keeps the index and a found flag, and takes the weight with the flag).
bool FindBehaviour(const char* name, Behaviour*& found) {
    int32_t i = 0;
    bool matched = false;
    while (i < 8 && !matched) {
        if (std::strcmp(gVehBehaviourTable[i]->mName, name) == 0) {
            found = gVehBehaviourTable[i];
            matched = true;
        } else {
            i++;
        }
    }
    return matched;
}

// The push of init_behaviours: the entry is appended unless the list is full (mTop == mEnd; the compare is for equality, so a list whose top is past its end
// never stops). The top moves before the entry is written.
void PushBehaviour(Fixed weight, Behaviour* behaviour) {
    if (gVehBehaviours.mTop != gVehBehaviours.mEnd) {
        BehaviourEntry* entry = gVehBehaviours.mTop;
        gVehBehaviours.mTop = entry + 1;
        entry->mWeight = weight;
        entry->mBehaviour = behaviour;
    }
}

}  // namespace

// 0x10132d7c (1184 bytes): 1. A time stamp of the ground under the vehicle newer than the path's replans (mode 1); no path found sets mUnreachable and traces.
// 2. A vehicle that is not at progress 1.0 and is stuck (is_stuck at 0x6666, 0.4, or more) replans too and always traces; no path sets mUnreachable and traces
// again. 3. With no waypoints or a finished path the answer is the brake, -velocity. Else, at the last waypoint the target is the destination and the path
// finishes when it is nearer than 0.1 * radius (0x19999 * radius); at any other the vehicle steps to the next waypoint when it is nearer than 2 * radius (the
// leg leaves mPathLegA; the step that reaches the loaded count replans with mode 0) and the target is the waypoint it then stands at (read again). 4. Before the
// last waypoint the force is (target - mPos) * 1.0 - velocity (Seek); at it, Arrival: the speed is min(mMaxSpeed, length / 2), scaled over the length; (0, 0)
// when the length is 0.
Vector2d Follow_Path::calculate(Vehicle& vehicle) {
    const uint32_t stamp = gGameWorld->GetMap()->GetTimeStampCell(static_cast<uint32_t>(vehicle.mPos.x >> 20), static_cast<uint32_t>(vehicle.mPos.y >> 20));
    if (stamp > vehicle.mPathStamp) {
        if (vehicle.recalc_path(vehicle.mDestination, static_cast<Vehicle::RecalcPathMode>(1)) == 0) {
            vehicle.mUnreachable = 1;
            CTRACE(0xc, "The ground changed under this peep, tried to renav, can't reach destination.");
        }
    }
    const Fixed progress = vehicle.is_finished_path();
    if (gVehOne != progress) {
        if (vehicle.is_stuck() >= 0x6666) {
            const int32_t found = vehicle.recalc_path(vehicle.mDestination, static_cast<Vehicle::RecalcPathMode>(1));
            CTRACE(1, "A peep became stuck and renavigated. <<<<< SO CHECK IT OUT, MAN!\n");
            if (found == 0) {
                vehicle.mUnreachable = 1;
                CTRACE(0xc, "Poople-flops!  That peep seems to be stuck good'n'proper now.  That's all I've got to say about that.");
            }
        }
    }

    Vector2d target;
    if (vehicle.mNumWaypoints == 0 || vehicle.mPathFlag == 1) return -vehicle.mVelocity;
    if (vehicle.mWaypoint == Sub(vehicle.mNumWaypoints, 1)) {
        target.x = vehicle.mDestination.x;
        target.y = vehicle.mDestination.y;
        const Vector2d toTarget = target - vehicle.mPos;
        if (OctagonalDistance(toTarget) < FixMul(0x19999, vehicle.mRadius)) vehicle.mPathFlag = 1;
    } else {
        target = WaypointAt(vehicle, vehicle.mWaypoint);
        const Vector2d toTarget = target - vehicle.mPos;
        if (OctagonalDistance(toTarget) < FixMul(0x20000, vehicle.mRadius)) {
            vehicle.mPathLegA = Sub(vehicle.mPathLegA, LegAt(vehicle, vehicle.mWaypoint));
            vehicle.mWaypoint = Add(vehicle.mWaypoint, 1);
            if (vehicle.mWaypoint == vehicle.mNumLoaded) vehicle.recalc_path(vehicle.mDestination, static_cast<Vehicle::RecalcPathMode>(0));
        }
        target = WaypointAt(vehicle, vehicle.mWaypoint);
    }

    if (vehicle.mWaypoint < Sub(vehicle.mNumWaypoints, 1)) {
        const Fixed one = gVehOne;
        const Vector2d toTarget = target - vehicle.mPos;
        return (toTarget * one) - vehicle.mVelocity;
    }
    const Vector2d toTarget = target - vehicle.mPos;
    const Fixed length = Length(toTarget);
    if (length == gVehZero) return Vector2d(0, 0);
    const Fixed speed = (vehicle.mMaxSpeed < FixDiv(length, 0x20000)) ? vehicle.mMaxSpeed : FixDiv(length, 0x20000);
    const Fixed scale = FixDiv(speed, length);
    return (toTarget * scale) - vehicle.mVelocity;
}

// 0x101332cc (104 bytes): the Mac's deleting destructor destroys the five waypoints (a trivial type here, so nothing) and frees the block itself.
Vehicle::~Vehicle() {}

// 0x10133358 (60 bytes): empties the list from the top down, two zero words per entry; the entry mData names is never cleared (the loop stops there).
void Vehicle::clear_behaviours() {
    while (gVehBehaviours.mTop > gVehBehaviours.mData) {
        gVehBehaviours.mTop->mWeight = 0;
        gVehBehaviours.mTop->mBehaviour = nullptr;
        gVehBehaviours.mTop -= 1;
    }
}

// 0x101333c4 (168 bytes): p - mPos in the frame (mForward, mSide): (d.x * f.x + d.y * f.y, d.x * s.x + d.y * s.y).
Vector2d Vehicle::point_to_local(const Vector2d& p) const {
    const Vector2d d = p - mPos;
    const Fixed sy = FixMul(d.y, mSide.y);
    const Fixed sx = FixMul(d.x, mSide.x);
    const Fixed fy = FixMul(d.y, mForward.y);
    const Fixed fx = FixMul(d.x, mForward.x);
    return Vector2d(Add(fx, fy), Add(sx, sy));
}

// 0x101334a4 (136 bytes): the transposed rotation, no translation: (v.x * f.x + v.y * s.x, v.x * f.y + v.y * s.y).
Vector2d Vehicle::vec_to_global(const Vector2d& v) const {
    const Fixed sy = FixMul(v.y, mSide.y);
    const Fixed fy = FixMul(v.x, mForward.y);
    const Fixed sx = FixMul(v.y, mSide.x);
    const Fixed fx = FixMul(v.x, mForward.x);
    return Vector2d(Add(fx, sx), Add(fy, sy));
}

// 0x10133564 (640 bytes): one step. The weighted forces of the list are summed (each behaviour's force capped at mMaxForce, then weighted) and capped again; the
// velocity gains the sum and is capped at mMaxSpeed. The step is taken when the nav rules allow it between the map squares of the old and the new position (the
// new square's number is looked up first); a refused step shifts a 1 into mStuckHistory. Then the forward and side axes follow the velocity and a step that made no
// progress (the progress did not grow) shifts a 1 in.
void Vehicle::next_step() {
    Vector2d sum;
    for (BehaviourEntry* entry = gVehBehaviours.mData; entry != gVehBehaviours.mTop; entry++) {
        const Vector2d force = entry->mBehaviour->calculate(*this);
        const Vector2d capped = truncate(force, mMaxForce);
        const Vector2d weighted = capped * entry->mWeight;
        sum.x = Add(sum.x, weighted.x);
        sum.y = Add(sum.y, weighted.y);
    }
    const Vector2d total = truncate(sum, mMaxForce);
    const Vector2d acceleration = total / gVehOne;
    mVelocity = truncate(mVelocity + acceleration, mMaxSpeed);
    const Vector2d next = mPos + mVelocity;

    const Vector2d here = map_global_to_local(static_cast<uint32_t>(mPos.x), static_cast<uint32_t>(mPos.y));
    const Vector2d there = map_global_to_local(static_cast<uint32_t>(next.x), static_cast<uint32_t>(next.y));
    const int32_t toSquare = map_fn(there.x, there.y, gVehMapData);
    const int32_t fromSquare = map_fn(here.x, here.y, gVehMapData);
    if (gVehNavRules(fromSquare, toSquare, mNavMode) == 0) {
        mStuckHistory <<= 1;
        mStuckHistory |= 1;
    } else {
        mPos = next;
    }

    mForward = normalise(mVelocity);
    mSide = perp(mForward);
    const Fixed progress = is_finished_path();
    mStuckHistory <<= 1;
    if (progress <= mLastProgress) mStuckHistory |= 1;
    mLastProgress = progress;
}

// 0x1013380c (24 bytes): the vector turned a quarter circle.
Vector2d perp(const Vector2d& v) { return Vector2d(Neg(v.y), v.x); }

// 0x1013384c (380 bytes): v scaled to about 1.0 long. The absolute values pick one of three ranges by their bits (below 0x1000, below 0x400000, the
// rest); the shifts are ARITHMETIC (and the divides signed), unlike truncate's. The first range multiplies with plain wrapping multiplies (no FixMul),
// the other two with FixMul (y first).
Vector2d normalise(const Vector2d& v) {
    const Fixed ax = Abs(v.x);
    const Fixed ay = Abs(v.y);
    const uint32_t bits = static_cast<uint32_t>(ax) | static_cast<uint32_t>(ay);
    if ((bits & 0xfffff000u) == 0) {
        const uint32_t root = isqrt(SumOfSquares(static_cast<uint32_t>(ax), static_cast<uint32_t>(ay)) << 6);
        const Fixed k = root != 0 ? 0x80000 / static_cast<Fixed>(root) : 0;
        return Vector2d(Mul(v.x, k), Mul(v.y, k));
    }
    if ((bits & 0xffc00000u) == 0) {
        const uint32_t root = isqrt(SumOfSquares(static_cast<uint32_t>(ax >> 8), static_cast<uint32_t>(ay >> 8)));
        const Fixed k = root != 0 ? 0x1000000 / static_cast<Fixed>(root) : 0;
        const Fixed y = FixMul(v.y, k);
        const Fixed x = FixMul(v.x, k);
        return Vector2d(x, y);
    }
    const uint32_t root = isqrt(SumOfSquares(static_cast<uint32_t>(ax >> 18), static_cast<uint32_t>(ay >> 18)));
    const Fixed k = root != 0 ? 0x4000 / static_cast<Fixed>(root) : 0;
    const Fixed y = FixMul(v.y, k);
    const Fixed x = FixMul(v.x, k);
    return Vector2d(x, y);
}

// 0x101339f4 (312 bytes): v itself when its length is below max (signed compare), else v scaled to max long.
Vector2d truncate(const Vector2d& v, int32_t max) {
    const Fixed length = Length(v);
    if (length < max) return v;
    const Fixed k = FixDiv(max, length);
    const Fixed y = FixMul(v.y, k);
    const Fixed x = FixMul(v.x, k);
    return Vector2d(x, y);
}

// 0x10133b58 (96 bytes): where the vehicle will be after t.
Vector2d Vehicle::predict_pos(int32_t t) const { return mPos + mVelocity * t; }

// 0x10133be4 (1392 bytes): the wall hugger fills gVehPath (the squares from the start to the destination: one byte per coordinate); 0x70000000 means no path (the
// vehicle is finished and mUnreachable is 1, answer 0). Mode 1 puts the vehicle's own square in front of the path (the count stops growing at 0xfc). The path is
// measured as octagonal distances between square centres: mPathTotal from the vehicle to the first square and on to the last, the first five squares become the
// waypoints with their legs (mPathLegA is the legs among them) and mPathLegB is what lies beyond the fifth. The Mac measures mWaypoints[0] - mPos once more here
// and never uses the result (omitted). The new path is stamped with a new time stamp of the map; answer 1.
int32_t Vehicle::recalc_path(const Vector2d& target, RecalcPathMode mode) {
    wallhug_path& path = gVehPath;
    path.mStartX = static_cast<uint8_t>(mPos.x >> 16);
    path.mStartY = static_cast<uint8_t>(mPos.y >> 16);
    path.mEndX = static_cast<uint8_t>(target.x >> 16);
    path.mEndY = static_cast<uint8_t>(target.y >> 16);
    if (wallhug_tricky(&path, mNavMode) == 0x70000000u) {
        mDestination.x = target.x;
        mDestination.y = target.y;
        mNumWaypoints = 0;
        mNumLoaded = 0;
        mPathTotal = 0;
        mPathLegA = 0;
        mPathLegB = 0;
        mLastProgress = 0;
        mStuckHistory = 0;
        mWaypoint = 0;
        mPathFlag = 1;
        mUnreachable = 1;
        return 0;
    }
    if (static_cast<int32_t>(mode) == 1) {
        for (int32_t i = 0xfb; i > 0; i--) path.mPoints[i] = path.mPoints[i - 1];
        path.mPoints[0].mX = static_cast<uint8_t>(mPos.x >> 16);
        path.mPoints[0].mY = static_cast<uint8_t>(mPos.y >> 16);
        if (path.mCount < 0xfc) path.mCount = static_cast<uint8_t>(path.mCount + 1);
    }
    mDestination.x = target.x;
    mDestination.y = target.y;
    mNumWaypoints = path.mCount;
    mNumLoaded = (mNumWaypoints < 5) ? mNumWaypoints : 5;

    mPathTotal = OctagonalDistance(SquareCentre(path.mPoints[0]) - mPos);
    for (int32_t i = 0; i < static_cast<int32_t>(path.mCount) - 1; i++) {
        mPathTotal = Add(mPathTotal, OctagonalDistance(SquareCentre(path.mPoints[i + 1]) - SquareCentre(path.mPoints[i])));
    }
    for (int32_t i = 0; i < mNumLoaded; i++) mWaypoints[i] = SquareCentre(path.mPoints[i]);
    mPathLegA = 0;
    for (int32_t i = 0; i < mNumLoaded - 1; i++) {
        mLegLength[i] = OctagonalDistance(mWaypoints[i + 1] - mWaypoints[i]);
        mPathLegA = Add(mPathLegA, mLegLength[i]);
    }
    mPathLegB = 0;
    for (int32_t i = mNumLoaded - 1; i < mNumWaypoints - 1; i++) {
        mPathLegB = Add(mPathLegB, OctagonalDistance(SquareCentre(path.mPoints[i + 1]) - SquareCentre(path.mPoints[i])));
    }
    mLastProgress = 0;
    mStuckHistory = 0;
    mWaypoint = 0;
    mPathFlag = 0;
    mUnreachable = 0;
    mPathStamp = gGameWorld->GetMap()->MakeNewTimeStamp();
    return 1;
}

// 0x101341a4 (216 bytes): a progress (1.0 when the path is finished).
Fixed Vehicle::is_finished_path() const { return PathProgress(*this); }

// 0x101342ac (304 bytes): the Mac inlines is_finished_path here. A progress of exactly 1.0 (finished, no path, or a path of zero length) answers 0; otherwise the
// share of the last 15 steps that were refused or made no progress: the set bits among 0 to 14 of mStuckHistory, divided by 15.
Fixed Vehicle::is_stuck() {
    const Fixed progress = is_finished_path();
    if (gVehOne == progress) return gVehZero;
    int32_t count = 0;
    uint32_t mask = 1;
    for (int32_t i = 0; i < 15; i++) {
        if (mStuckHistory & mask) count++;
        mask <<= 1;
    }
    return FixDiv(Shl(count, 16), 0xf0000);
}

// 0x10134404 (480 bytes): empties the list (the Mac inlines clear_behaviours) and appends the three behaviours it finds by name, in this order, each search
// followed by its own push: "avoid_walls" 0.9, "follow_path" 0.5, "separation" 0.1. A name not found appends nothing.
void Pilot::init_behaviours() {
    clear_behaviours();
    Behaviour* behaviour = nullptr;
    if (FindBehaviour("avoid_walls", behaviour)) PushBehaviour(0xe666, behaviour);
    if (FindBehaviour("follow_path", behaviour)) PushBehaviour(0x8000, behaviour);
    if (FindBehaviour("separation", behaviour)) PushBehaviour(0x1999, behaviour);
}

// 0x10134610 (328 bytes): a vehicle at rest at the origin. The Vector2d members are built to (0, 0) (here by Vehicle's own constructor); the first stores (radius
// 10, force 1.0, speed 5.0) are overwritten later (radius 0x3333, set_speed(1.0): force 0x6666, speed 0x3333). The Mac's word 0x30 (the target pointer) is
// zeroed: mTarget on the host. mPathStamp, mLegLength and the bytes at +0xbc stay as they were.
Pilot::Pilot() {
    mF00 = 1;
    mRadius = 10;
    mMaxForce = 0x10000;
    mMaxSpeed = 0x50000;
    mTarget = nullptr;
    mF34 = 0;
    mNumWaypoints = 0;
    mNumLoaded = 0;
    mWaypoint = 0;
    mPathFlag = 1;
    mPathTotal = 0;
    mPathLegA = 0;
    mPathLegB = 0;
    mLastProgress = 0;
    mStuckHistory = 0;
    mNavMode = static_cast<NavMode>(0);
    mUnreachable = 0;
    mRadius = 0x3333;
    mF00 = gVehOne;
    mPos = Vector2d(0, 0);
    set_speed(1.0f);
}

// 0x101347b0 (20 bytes): position and velocity.
void Pilot::reset(int32_t x, int32_t y, int32_t vx, int32_t vy) {
    mPos.x = x;
    mPos.y = y;
    mVelocity.x = vx;
    mVelocity.y = vy;
}

// 0x101347ec (48 bytes): a new path to (x, y), mode 0.
void Pilot::new_target(int32_t x, int32_t y) { recalc_path(Vector2d(x, y), static_cast<RecalcPathMode>(0)); }

// 0x10134844 (32 bytes): one step along the path.
void Pilot::move() { next_step(); }

// 0x10134888 (20 bytes): the position.
void Pilot::get_pos(int32_t& x, int32_t& y) {
    x = mPos.x;
    y = mPos.y;
}

// 0x101348c4 (216 bytes): Vehicle::is_finished_path's code at its own address.
Fixed Pilot::is_finished_path() const { return PathProgress(*this); }
// 0x101349cc (128 bytes): the speed (2.0 at most; a NaN stays a NaN, the original compares with ble) sets the force limit (0.4 * speed) and the speed
// limit (0.2 * speed) in 16.16, each at least 0x28f (about 0.01). The doubles are multiplied by the constant first, then by 65536.0, as the listing does.
void Pilot::set_speed(float speed) {
    if (speed > 2.0f) speed = 2.0f;
    mMaxForce = ppc_fctiwz(65536.0 * (0.4 * static_cast<double>(speed)));
    mMaxSpeed = ppc_fctiwz(65536.0 * (0.2 * static_cast<double>(speed)));
    if (mMaxForce < 0x28f) mMaxForce = 0x28f;
    if (mMaxSpeed < 0x28f) mMaxSpeed = 0x28f;
}
