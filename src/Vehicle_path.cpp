// vehicle.cpp (main app, SimTheme Park 2.04b3; unit w209): the vehicles' path following. Vehicle::recalc_path plans a walk with the wall hugger
// (wallhug_tricky), measures it and loads the first five waypoints; Vehicle::next_step sums the weighted behaviours' forces, steers, takes the step when the
// nav rules allow it and shifts into mStuckHistory whether the step was refused or made no progress; Vehicle::is_stuck counts that history; Follow_Path::calculate
// is the behaviour that replans (the ground changed under the path, or the vehicle is stuck), walks the waypoints and steers at the next one; Pilot::Pilot and
// Pilot::init_behaviours build a pilot and its behaviour list. Everything is 16.16 Fixed; adds, subtractions, negations and shifts wrap as the PowerPC's do
// (done in uint32_t). Not translated here: __sinit_vehicle_cpp (the behaviour objects), Vehicle::Serialise and CPeep's callers. The nav rules
// (implement_nav_rulesA2B) and the candidate walks are unit w218's (VehicleCallbacks_nav.cpp), as is Avoid_Walls::calculate (Vehicle_walls.cpp).
#include "VehicleSteer.h"

#include <cstddef>
#include <cstring>

#include "Debug.h"
#include "fixmath.h"
#include "TPCamera.h"
#include "vector2d.h"
#include "World.h"

// ---- the globals of vehicle.cpp that unit w209 meets (Vehicle_serialise.cpp owns the typed behaviour table and startup)
wallhug_path gVehPath;                                           // 0x1034c708 (BSS)
void* gVehMapData = nullptr;                                     // 0x1034ca94 (BSS)
VehNavRulesFn gVehNavRules = &implement_nav_rulesA2B;            // 0x102a45a0 (the TVector 0x10266d88 of implement_nav_rulesA2B)

namespace {

// The Mac's add, subf, neg and shifts wrap around; a signed overflow would be undefined on the host.
Fixed Add(Fixed a, Fixed b) { return static_cast<Fixed>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
Fixed Sub(Fixed a, Fixed b) { return static_cast<Fixed>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
Fixed Neg(Fixed a) { return static_cast<Fixed>(0u - static_cast<uint32_t>(a)); }
Fixed Shl(Fixed a, uint32_t bits) { return static_cast<Fixed>(static_cast<uint32_t>(a) << bits); }

// cmpwi / neg: the absolute value; |INT32_MIN| stays INT32_MIN.
Fixed Abs(Fixed a) { return a < 0 ? Neg(a) : a; }

// mullw / mullw / add: the wrapping sum of two squares.
uint32_t SumOfSquares(uint32_t a, uint32_t b) { return a * a + b * b; }

// The length of v as Follow_Path's Arrival tail computes it (the same inline code as truncate, Arrival and Avoid_Obstacle, unit w203): the root of the sum of
// the squares of the absolute values; when a component reaches 0x1000 or 0x400000 both are shifted right (LOGICAL shifts) first and the root shifted back
// left.
Fixed Length(const Vector2d& v) {
    const uint32_t ax = static_cast<uint32_t>(Abs(v.x));
    const uint32_t ay = static_cast<uint32_t>(Abs(v.y));
    const uint32_t bits = ax | ay;
    if ((bits & 0xfffff000u) == 0) return static_cast<Fixed>(isqrt(SumOfSquares(ax, ay)));
    if ((bits & 0xffc00000u) == 0) return static_cast<Fixed>(isqrt(SumOfSquares(ax >> 8, ay >> 8)) << 8);
    return static_cast<Fixed>(isqrt(SumOfSquares(ax >> 18, ay >> 18)) << 18);
}

// The octagonal distance: ax + ay - (the smaller >> 1), arithmetic shift, signed compares (the code of is_finished_path, Separation and the obstacle callbacks).
Fixed OctagonalDistance(const Vector2d& d) {
    const Fixed ax = Abs(d.x);
    const Fixed ay = Abs(d.y);
    const Fixed smaller = (ax > ay) ? ay : ax;
    return Sub(Add(ax, ay), smaller >> 1);
}

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

// 0x10134404 (480 bytes): empties the list (the Mac inlines clear_behaviours) and appends the three behaviours it finds by name, in this order, each search
// followed by its own push: "avoid_walls" 0.9, "follow_path" 0.5, "separation" 0.1. A name not found appends nothing.
void Pilot::init_behaviours() {
    clear_behaviours();
    Behaviour* behaviour = nullptr;
    if (FindBehaviour("avoid_walls", behaviour)) PushBehaviour(0xe666, behaviour);
    if (FindBehaviour("follow_path", behaviour)) PushBehaviour(0x8000, behaviour);
    if (FindBehaviour("separation", behaviour)) PushBehaviour(0x1999, behaviour);
}

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
