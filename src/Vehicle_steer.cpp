// vehicle.cpp (main app, SimTheme Park 2.04b3; unit w203): the ride vehicles' steering. The vector helpers perp, normalise and truncate, the Vehicle's
// geometry (point_to_local, vec_to_global, predict_pos, is_finished_path), clear_behaviours and the destructor, the five behaviours' calculate functions
// with the three callbacks they hand to the candidate walks, and the Pilot's thin members. Everything is 16.16 Fixed; adds, negations and multiplies
// wrap as the PowerPC's do (done in uint32_t). Vehicle::Serialise and __sinit_vehicle_cpp are in Vehicle_serialise.cpp.
// Follow_Path::calculate, recalc_path, next_step, Pilot::Pilot and Pilot::init_behaviours are unit w209's (Vehicle_path.cpp), Avoid_Walls::calculate and
// the candidate walks unit w218's (Vehicle_walls.cpp, VehicleCallbacks_nav.cpp).
#include "VehicleSteer.h"

#include <cstdlib>

#include "fixmath.h"
#include "ppc_math.h"
#include "vector2d.h"
#include "World.h"

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

// 0x10133358 (60 bytes): empties the list from the top down, two zero words per entry; the entry mData names is never cleared (the loop stops there).
void Vehicle::clear_behaviours() {
    while (gVehBehaviours.mTop > gVehBehaviours.mData) {
        gVehBehaviours.mTop->mWeight = 0;
        gVehBehaviours.mTop->mBehaviour = nullptr;
        gVehBehaviours.mTop -= 1;
    }
}

// 0x101332cc (104 bytes): the Mac's deleting destructor destroys the five waypoints (a trivial type here, so nothing) and frees the block itself.
Vehicle::~Vehicle() {}

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

// 0x10133b58 (96 bytes): where the vehicle will be after t.
Vector2d Vehicle::predict_pos(int32_t t) const { return mPos + mVelocity * t; }

// 0x101341a4 (216 bytes): a progress (1.0 when the path is finished).
Fixed Vehicle::is_finished_path() const { return PathProgress(*this); }

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

// 0x101349cc (128 bytes): the speed (2.0 at most; a NaN stays a NaN, the original compares with ble) sets the force limit (0.4 * speed) and the speed
// limit (0.2 * speed) in 16.16, each at least 0x28f (about 0.01). The doubles are multiplied by the constant first, then by 65536.0, as the listing does.
void Pilot::set_speed(float speed) {
    if (speed > 2.0f) speed = 2.0f;
    mMaxForce = ppc_fctiwz(65536.0 * (0.4 * static_cast<double>(speed)));
    mMaxSpeed = ppc_fctiwz(65536.0 * (0.2 * static_cast<double>(speed)));
    if (mMaxForce < 0x28f) mMaxForce = 0x28f;
    if (mMaxSpeed < 0x28f) mMaxSpeed = 0x28f;
}

// 0x101348c4 (216 bytes): Vehicle::is_finished_path's code at its own address.
Fixed Pilot::is_finished_path() const { return PathProgress(*this); }
