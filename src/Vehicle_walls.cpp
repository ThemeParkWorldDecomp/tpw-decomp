// vehicle.cpp (main app, SimTheme Park 2.04b3; unit w218): Avoid_Walls::calculate, the steering behaviour that keeps a vehicle off the walls of the map. It
// predicts where the vehicle will be (Vehicle::predict_pos by gVehOne), takes the map square the vehicle stands on and the direction of its step (the step to the
// prediction, normalised; a component is "negative" when it is below gVehZero, else "positive") and asks the map (MAP_wall_in_way) about the walls on the sides
// the step heads for. A wall the prediction has crossed or come within the radius of pushes straight back, twice the overlap. When no side does, the corners of
// the diagonal squares it heads for are tried: a corner counts when a wall of the diagonal square lies on it and the two squares beside the vehicle's do not
// shield it, and then, when the line of the step passes within the radius of the corner (line_circle_intersect) no farther along than the step is long, the
// push is the point reached there, taken from the corner and divided by the radius. Otherwise (0, 0). Everything is 16.16 Fixed; adds, subtractions, negations
// and shifts wrap as the PowerPC's do (done in uint32_t). Not translated here: __sinit_vehicle_cpp (no behaviour list it builds holds an Avoid_Walls) and
// Avoid_Walls' vtable and destructor.
#include "VehicleSteer.h"

#include "fixmath.h"
#include "TPCamera.h"
#include "vector2d.h"
#include "World.h"

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

// The length of v as the inline code of Follow_Path's Arrival tail, truncate and Avoid_Obstacle computes it (unit w203): the root of the sum of the squares of
// the absolute values; when a component reaches 0x1000 or 0x400000 both are shifted right (LOGICAL shifts) first and the root shifted back left.
Fixed Length(const Vector2d& v) {
    const uint32_t ax = static_cast<uint32_t>(Abs(v.x));
    const uint32_t ay = static_cast<uint32_t>(Abs(v.y));
    const uint32_t bits = ax | ay;
    if ((bits & 0xfffff000u) == 0) return static_cast<Fixed>(isqrt(SumOfSquares(ax, ay)));
    if ((bits & 0xffc00000u) == 0) return static_cast<Fixed>(isqrt(SumOfSquares(ax >> 8, ay >> 8)) << 8);
    return static_cast<Fixed>(isqrt(SumOfSquares(ax >> 18, ay >> 18)) << 18);
}

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
