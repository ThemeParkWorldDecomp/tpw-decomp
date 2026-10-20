// TPCamera.cpp (main app of SimTheme Park 2.04b3): the camera's small callees, unit w140: GetNextGridsquare (0x10033e14, 1,948 bytes),
// MatrixDifferenceTiny (0x10034784), ApplyHeadsOnRidesBumpage (0x100336f4), RoundRotationToNearest90Degrees (0x100346a0), SetCameraPOI
// (0x10033650), RideRideIfRequired (0x100345f8) and AllowFullCameraZoomout (0x10032c08). The two height lookups are in
// src/TPRide_height.cpp.
//
// Every float is a single in the listing's order. The fused multiply-adds of the listing are ppc_fmadds (GetNextGridsquare 10,
// ApplyHeadsOnRidesBumpage 9) and ppc_fnmsubs (GetNextGridsquare 4: `x -= x * t`). GetNextGridsquare calls __cvt_fp2unsigned 18 times
// (cw_cvt_fp2unsigned: a negative number gives 0, a huge one 0xFFFFFFFF), modf 4 times (only the fraction is used) and MAP_wall_in_way
// up to twice (the translation: once per call). The callees MAP_wall_in_way, GetThingAtPosnIfEntrance, CObject::GoFirstPerson and
// CRideDatabase::GetRideInfo are not translated.
#include <cmath>
#include <cstring>

#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "keytable_actions.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "tb_matrix.h"

namespace {
constexpr float Bits(uint32_t b) { return __builtin_bit_cast(float, b); }
constexpr float kHalfPi = Bits(0x3fc90fdbu);       // 0x102646d0
constexpr float kQuarterPi = Bits(0x3f490fdbu);    // 0x10264680, and its negative 0xbf490fdb
constexpr double kHalfPiD = __builtin_bit_cast(double, 0x3ff921fb60000000ull);   // 0x10264688: the double of the float pi / 2
constexpr float kCell = 10.0f;                      // 0x102646dc: the grid square's size
constexpr float kNoStep = 2.0f;                     // 0x10264698: "this axis never reaches a square edge"
}  // namespace

// Initialised data of the camera (0x1029cc5c..0x1029cc98; names invented).
float gCameraMatrixEpsilon = Bits(0x3727c5acu);   // 0x1029cc5c  1e-5
float gCameraPOIValue = Bits(0x3f800000u);        // 0x1029cc88  1.0
float gGridEdgeOffset = Bits(0x411ffbe7u);        // 0x1029cc90  9.999
float gGridStepScale = Bits(0x3f8147aeu);         // 0x1029cc94  1.01
float gGridNudge = Bits(0x3a83126fu);             // 0x1029cc98  0.001

int32_t AllowFullCameraZoomout() {
    if ((gCameraModeA & 0x200) != 0) gCameraModeA = gCameraModeA & ~0x200u;
    else gCameraModeA = gCameraModeA | 0x200u;
    return 1;
}

// The camera looks at a place: the velocity stops; each non-null input is copied. The listing tests the first pointer a second time
// where it reads the second one (so a null z with a non-null x dereferences null, as on the Mac).
void SetCameraPOI(float* x, float* z, float* zoom, float* yaw, float* value) {
    gCameraF5BB4[0] = 0.0f;
    gCameraF5BB4[1] = 0.0f;
    gCameraF5BB4[2] = 0.0f;
    if (x != nullptr) gCameraPosition[0] = *x;
    if (x != nullptr) gCameraPosition[2] = *z;
    if (zoom != nullptr) gCameraF68 = *zoom;
    if (yaw != nullptr) gCameraF5BF4 = *yaw;
    if (value != nullptr) gCameraPOIValue = *value;
}

// Nonzero when the entrance of a ride stands at the point: the camera then rides it in first person. The ride database's answer is
// not used (the Mac passes the type id in the high half of r4).
int32_t RideRideIfRequired(s3DPoint* point) {
    CObject* const object = GetThingAtPosnIfEntrance(point->x, point->z);
    if (object == nullptr) return 0;
    gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType});
    object->GoFirstPerson(1);
    return 1;
}

// Rounds the key yaw (gCameraF5BF4) to a multiple of a quarter turn: the remainder of fmod(yaw, pi / 2) is taken off, or topped up to
// the next multiple when it is past pi / 4 (and mirrored for a negative one).
void RoundRotationToNearest90Degrees() {
    const float rest = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kHalfPiD));
    if (rest >= 0.0f) {
        if (rest > kQuarterPi) gCameraF5BF4 = gCameraF5BF4 + (kHalfPi - rest);
        else gCameraF5BF4 = gCameraF5BF4 - rest;
    } else if (rest >= -kQuarterPi) {
        gCameraF5BF4 = gCameraF5BF4 - rest;
    } else {
        gCameraF5BF4 = gCameraF5BF4 - (kHalfPi + rest);
    }
}

// 1 when the two matrices are almost the same: the sum of the absolute differences of the 12 words of rows 0 to 3 (x y z each; the w
// column is not read), each difference taken in double and rounded to a single, is below 1e-5. A NaN sum answers 1 as well.
int32_t MatrixDifferenceTiny(sMatrix* a, sMatrix* b) {
    float sum = 0.0f;
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            const double difference = static_cast<double>(a->m[row][column]) - static_cast<double>(b->m[row][column]);
            const float magnitude = static_cast<float>(std::fabs(difference));
            sum = sum + magnitude;
        }
    }
    return !(sum >= gCameraMatrixEpsilon) ? 1 : 0;
}

// out = the ride matrix * (1 - w) + the previous one * w with w = (1000 - dt clamped to 1..1000) / 1000 / 2, for the nine rotation
// words; the position (row 3, x y z) is the ride's own.
void ApplyHeadsOnRidesBumpage(sMatrix* ride, sMatrix* previous, float dt, sMatrix* out) {
    float t = dt;
    if (!(t <= 1000.0f)) t = 1000.0f;
    if (!(t >= 1.0f)) t = 1.0f;
    const float fraction = (1000.0f - t) / 1000.0f;
    const float w = 0.5f * fraction;
    const float rest = 1.0f - w;
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            const float scaled = previous->m[row][column] * w;
            out->m[row][column] = ppc_fmadds(ride->m[row][column], rest, scaled);
        }
    }
    out->m[3][0] = ride->m[3][0];
    out->m[3][1] = ride->m[3][1];
    out->m[3][2] = ride->m[3][2];
}

namespace {
// A point pushed to the edge of square `index` on the side it walks to: 10 * index when it walks down, 10 * index + 9.999 otherwise.
float EdgeOf(float step, uint32_t index) {
    const float first = static_cast<float>(index);
    if (step >= 0.0f) return ppc_fmadds(kCell, first, gGridEdgeOffset);
    return kCell * first;
}

// The time (in steps) an axis takes to the next square edge: the fraction of the square left (1 - fraction when walking up) over the step per
// square, as a positive number; an axis with no step takes 2.0 (more than any square).
float TimeToEdge(float position, float step) {
    if (step == 0.0f) return kNoStep;
    double whole = 0.0;
    const float fraction = static_cast<float>(std::modf(static_cast<double>(position / kCell), &whole));
    const float perSquare = step / kCell;
    if (step >= 0.0f) {
        const float left = 1.0f - fraction;
        return std::fabs(left / perSquare);
    }
    return std::fabs(fraction / perSquare);
}
}  // namespace

// Walks the point `from` by the step `step` to the next grid square edge of 10 units (the squares are 10 by 10). `out` is the point
// reached (y 0); `step` is left with what remains to walk (the whole step when nothing is in the way, less when an edge was reached
// first, 0 on an axis stopped by a wall). The axis that reaches an edge first (tx and tz are the times to the edges, 2.0 for a zero
// step) is moved to the edge; if a wall (MAP_wall_in_way, mode 2) stands on that side the point stops at the edge and that axis' step
// is cleared, else it is pushed 0.001 over the edge. The other axis moves the same fraction and is snapped to its square when that
// leaves it. A step that reaches no edge (both times 1.0 or more) is applied whole. Answers 1 while something remains to walk.
// A tie (tx == tz) with both steps nonzero makes the x step 1.01 times smaller and its time 1.01 times larger, so the axes differ.
int32_t GetNextGridsquare(s3DPoint* from, s3DPoint* step, s3DPoint* out) {
    uint32_t ix = cw_cvt_fp2unsigned(static_cast<double>(from->x / kCell));
    uint32_t iz = cw_cvt_fp2unsigned(static_cast<double>(from->z / kCell));
    out->y = 0.0f;
    float tx = TimeToEdge(from->x, step->x);
    float tz = TimeToEdge(from->z, step->z);
    if (tx == tz) {
        const float stepX = step->x;
        if (0.0f != stepX) {
            const float stepZ = step->z;
            if (0.0f != stepZ) {
                if (stepX == 0.0f) {
                    step->z = stepZ / gGridStepScale;
                    tz = tz * gGridStepScale;
                } else {
                    step->x = stepX / gGridStepScale;
                    tx = tx * gGridStepScale;
                }
            }
        }
    }
    uint32_t cx = 0;
    uint32_t cz = 0;
    if (tx >= tz) {
        // z reaches its edge first
        if (!(tz >= 1.0f)) {
            const uint8_t direction = step->z >= 0.0f ? 2 : 0;
            if (MAP_wall_in_way(static_cast<uint8_t>(ix), static_cast<uint8_t>(iz), direction, static_cast<NavMode>(2)) != 0) {
                out->z = EdgeOf(step->z, iz);
                step->z = 0.0f;
                cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
            } else {
                out->z = ppc_fmadds(step->z, tz, from->z);
                step->z = ppc_fnmsubs(step->z, tz, step->z);
                cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
                if (iz == cz) {
                    if (step->z >= 0.0f) out->z = out->z + gGridNudge;
                    else out->z = out->z - gGridNudge;
                    cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
                }
            }
            out->x = ppc_fmadds(step->x, tz, from->x);
            step->x = ppc_fnmsubs(step->x, tz, step->x);
            cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            if (ix != cx) {
                out->x = EdgeOf(step->x, ix);
                cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            }
            return (step->x != 0.0f || step->z != 0.0f) ? 1 : 0;
        }
    } else if (!(tx >= 1.0f)) {
        // x reaches its edge first
        const uint8_t direction = step->x >= 0.0f ? 1 : 3;
        if (MAP_wall_in_way(static_cast<uint8_t>(ix), static_cast<uint8_t>(iz), direction, static_cast<NavMode>(2)) != 0) {
            out->x = EdgeOf(step->x, ix);
            step->x = 0.0f;
            cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
        } else {
            out->x = ppc_fmadds(step->x, tx, from->x);
            step->x = ppc_fnmsubs(step->x, tx, step->x);
            cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            if (ix == cx) {
                if (step->x >= 0.0f) out->x = out->x + gGridNudge;
                else out->x = out->x - gGridNudge;
                cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
            }
        }
        out->z = ppc_fmadds(step->z, tx, from->z);
        step->z = ppc_fnmsubs(step->z, tx, step->z);
        cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
        if (iz != cz) {
            out->z = EdgeOf(step->z, iz);
            cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
        }
        return (step->x != 0.0f || step->z != 0.0f) ? 1 : 0;
    }
    // no edge is reached: the whole step is taken, and a coordinate that left its square is put on the edge it crossed
    out->x = from->x + step->x;
    out->z = from->z + step->z;
    cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
    cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
    if (ix != cx) out->x = EdgeOf(step->x, ix);
    if (iz != cz) out->z = EdgeOf(step->z, iz);
    cx = cw_cvt_fp2unsigned(static_cast<double>(out->x / kCell));
    cz = cw_cvt_fp2unsigned(static_cast<double>(out->z / kCell));
    (void)cx;
    (void)cz;
    return 0;
}
