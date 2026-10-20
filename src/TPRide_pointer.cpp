// TPRide.cpp (main app 0x10065f50, unit w134): CalculatePointerWorldPosn, the pointer's world position from the camera ray through
// the terrain and the rides.
#include <cmath>
#include <cstring>

#include "TPCamera.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "ppc_math.h"

uint8_t gPointerFovOnce;      // 0x102c6ca0
float gPointerFovFactor;      // 0x102c6ca4

namespace {

// pi / 2 as the Mac keeps it for the rotations: the single 0x3fc90fdb widened (0x10264a88; the negative at 0x10264a90).
constexpr double kHalfPi = 0x1.921fb6p+0;

// Rotates (x, y) of a point by `angle`: sin and cos in double, each cast to float (frsp), then fmsubs and fmadds.
void RotateXY(s3DPoint& p, double angle) {
    const float s = static_cast<float>(std::sin(angle));
    const float c = static_cast<float>(std::cos(angle));
    const float x = p.x;
    const float y = p.y;
    p.x = ppc_fmsubs(x, c, y * s);
    p.y = ppc_fmadds(x, s, y * c);
}

// The Mac's clamp of a cell index: below 0 to 0, then (unsigned compare) count or more to count - 1.
void ClampCells(int32_t& a, int32_t& b, uint32_t count) {
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (static_cast<uint32_t>(a) >= count) a = static_cast<int32_t>(count - 1);
    if (static_cast<uint32_t>(b) >= count) b = static_cast<int32_t>(count - 1);
}

// Word copy of a point (the Mac copies with lwz and stw).
void CopyPoint(s3DPoint* to, const s3DPoint* from) {
    uint32_t w[3];
    std::memcpy(w, from, sizeof w);
    std::memcpy(to, w, sizeof w);
}

}  // namespace

// The pointer's world position: builds the ray through the pointer from the camera matrix (three corner points of the view, rotated
// by the field of view and by atan(0.75), sent through the camera matrix), intersects it with the height field's quads (two
// triangles each, the quad's flag bit 2 picks the diagonal) and walks it across the map in 10 unit steps asking FindPointedRide for a
// ride. *result is 1 when the terrain was hit; *position (and *pickup when not NULL) gets the ride's place when a ride is nearer than
// the terrain hit (or there is no hit), else the terrain hit; nothing is written when neither exists. `distance` is overwritten at
// the start and never read. Quirks kept: the cell's z extent uses the x size (mF08[2]) where the rest of the function uses mF08[3];
// when the height field is absent gPointedRide keeps its old value and still decides the answer.
void CalculatePointerWorldPosn(s3DPoint* position, float /*distance*/, s3DPoint* pickup, int32_t* result) {
    s3DPoint hit = {0.0f, 0.0f, 0.0f};   // 0xd0, the terrain hit
    int32_t hitFound = 0;                // r17

    // ---- A: the ray
    s3DPoint a = {0.0f, 0.0f, 1.0f};     // 0x13c
    if (static_cast<int8_t>(gPointerFovOnce) == 0) {
        gPointerFovFactor = 0.885f;
        gPointerFovOnce = 1;
    }
    const float angle = gCameraFov * 0.5f * gPointerFovFactor;
    const float scale = gTPGlobals.mSinTableScale;
    const uint32_t mask = gTPGlobals.mSinTableMask;
    const uint32_t sinIndex = mask & static_cast<uint32_t>(ppc_fctiwz(angle * scale));
    const uint32_t cosIndex = mask & static_cast<uint32_t>(ppc_fctiwz((1.5707964f + angle) * scale));
    const float sinA = gTPGlobals.mSinTable[sinIndex];
    const float cosA = gTPGlobals.mSinTable[cosIndex];
    const float ay = a.y;
    const float az = a.z;
    a.y = ppc_fmsubs(ay, cosA, az * sinA);
    a.z = ppc_fmadds(ay, sinA, az * cosA);
    s3DPoint b = a;                      // 0x130
    s3DPoint c = a;                      // 0x124
    const float atanFloat = static_cast<float>(std::atan(0.75));
    RotateXY(a, -kHalfPi - static_cast<double>(atanFloat));
    RotateXY(b, kHalfPi + static_cast<double>(atanFloat));
    RotateXY(c, -kHalfPi + static_cast<double>(atanFloat));
    VectorThroughLMatrixNoWNoTrans(&gCameraMatrix, &a);
    VectorThroughLMatrixNoWNoTrans(&gCameraMatrix, &b);
    VectorThroughLMatrixNoWNoTrans(&gCameraMatrix, &c);
    b.x = b.x - a.x;
    b.y = b.y - a.y;
    b.z = b.z - a.z;
    c.x = c.x - a.x;
    c.y = c.y - a.y;
    c.z = c.z - a.z;
    const float sx = gPointerScreenXY[0] / (2.0f * gViewCentreX);
    const float sy = gPointerScreenXY[1] / (2.0f * gViewCentreY);
    s3DPoint dir = a;                    // 0x118 (built in 0x10c)
    dir.x = ppc_fmadds(b.x, sx, dir.x);
    dir.y = ppc_fmadds(b.y, sx, dir.y);
    dir.z = ppc_fmadds(b.z, sx, dir.z);
    dir.x = ppc_fmadds(c.x, sy, dir.x);
    dir.y = ppc_fmadds(c.y, sy, dir.y);
    dir.z = ppc_fmadds(c.z, sy, dir.z);

    if (gHeightfieldHeader != nullptr && (gHeightfieldHeader->mFlags & 2) != 0) {
        const sHField* const field = gHeightfieldHeader->mHField;
        const float dx = dir.x;
        const float dy = dir.y;
        const float dz = dir.z;
        const s3DPoint cam = gCameraViewPoint;
        const float orgX = field->mF08[0];
        const float orgZ = field->mF08[1];
        const float sizeX = field->mF08[2];
        const float sizeZ = field->mF08[3];

        // ---- B: the terrain. The cell ranges the ray covers between the field's lowest and highest heights.
        const float hiY = field->mF20[1] - cam.y;
        const float loY = field->mF20[0] - cam.y;
        const float tHi = hiY / dy;
        const float xHi = ppc_fmadds(tHi, dx, cam.x);
        const float zHi = ppc_fmadds(tHi, dz, cam.z);
        int32_t cx0 = ppc_fctiwz((xHi - orgX) / sizeX);
        const float tLo = loY / dy;
        const float xLo = ppc_fmadds(tLo, dx, cam.x);
        const float zLo = ppc_fmadds(tLo, dz, cam.z);
        int32_t cx1 = ppc_fctiwz((xLo - orgX) / sizeX);
        int32_t cz0 = ppc_fctiwz((zHi - orgZ) / sizeZ);
        int32_t cz1 = ppc_fctiwz((zLo - orgZ) / sizeZ);
        ClampCells(cx0, cx1, field->mF18[0]);
        ClampCells(cz0, cz1, field->mF18[1]);
        const int32_t xMax = cx0 < cx1 ? cx1 : cx0;
        const int32_t xMin = cx1 < cx0 ? cx1 : cx0;
        const int32_t zMax = cz0 < cz1 ? cz1 : cz0;
        const int32_t zMin = cz1 < cz0 ? cz1 : cz0;

        // flagXZ (r15): 0 when x and z of the ray have the same strict sign, else 1; flagY (r16) the same for x and y.
        const bool flagXZ = !((dx < 0.0f && dz < 0.0f) || (dx > 0.0f && dz > 0.0f));
        const bool flagY = !((dx < 0.0f && dy < 0.0f) || (dx > 0.0f && dy > 0.0f));
        const float nd = -dx;
        const float w154 = ppc_fnmadds(dz, cam.x, nd * cam.z);
        const float w150 = ppc_fnmadds(dy, cam.x, nd * cam.y);
        float best = 3.4028234663852886e+38f;   // FLT_MAX, 0x10264aac

        const int32_t width = static_cast<int32_t>(field->mF18[0]);
        for (int32_t zi = zMin; zi <= zMax; ++zi) {
            for (int32_t xi = xMin; xi <= xMax; ++xi) {
                // the cell's x extent against the ray
                const float x0 = ppc_fmadds(sizeX, static_cast<float>(xi), orgX);
                const float x1 = x0 + sizeX;
                float zA;
                float zB;
                if (flagXZ) {
                    zA = ppc_fmadds(sizeZ, static_cast<float>(zi), orgZ);
                    zB = zA + sizeZ;
                } else {
                    zA = ppc_fmadds(sizeZ, static_cast<float>(zi + 1), orgZ);
                    zB = zA - sizeZ;
                }
                const float v0 = w154 + ppc_fmadds(x0, dz, zA * nd);
                const float v1 = w154 + ppc_fmadds(x1, dz, zB * nd);
                if (!((v0 < 0.0f && v1 > 0.0f) || (v0 > 0.0f && v1 < 0.0f))) continue;

                // the four corner heights
                const int32_t row = width + 1;
                const int32_t i00 = zi * row + xi;
                const int32_t i10 = (zi + 1) * row + xi;
                const float h00 = field->mHeights[i00];
                const float h01 = field->mHeights[i00 + 1];
                const float h10 = field->mHeights[i10];
                const float h11 = field->mHeights[i10 + 1];
                float lowH;
                float highH;
                if (h00 < h01) {
                    lowH = h00;
                    highH = h01;
                } else {
                    lowH = h01;
                    highH = h00;
                }
                if (h10 < lowH) lowH = h10;
                if (h10 > highH) highH = h10;
                if (h11 < lowH) lowH = h11;
                if (h11 > highH) highH = h11;
                float hA;
                float hB;
                if (flagY) {
                    hA = lowH;
                    hB = highH;
                } else {
                    hA = highH;
                    hB = lowH;
                }
                const float u0 = w150 + ppc_fmadds(x0, dy, hA * nd);
                const float u1 = w150 + ppc_fmadds(x1, dy, hB * nd);
                if (!((u0 < 0.0f && u1 > 0.0f) || (u0 > 0.0f && u1 < 0.0f))) continue;

                // the cell's two triangles
                const int32_t quad = xi + zi * width;
                for (int32_t j = 0; j < 2; ++j) {
                    float y0;       // f22: the height at the triangle's corner
                    float da;       // f0
                    float db;       // f1
                    float ex;       // f2
                    float ez;       // f3
                    float px;       // f24
                    float pz;       // f23
                    if ((field->mQuads[quad].mF00 & 2) != 0) {
                        if (j == 0) {
                            y0 = h10;
                            da = h11 - h10;
                            db = h00 - h10;
                            ex = sizeX;
                            ez = -sizeZ;
                            px = ppc_fmadds(sizeX, static_cast<float>(xi), orgX);
                            pz = ppc_fmadds(sizeZ, static_cast<float>(zi + 1), orgZ);
                        } else {
                            y0 = h01;
                            da = h00 - h01;
                            db = h11 - h01;
                            ex = -sizeX;
                            ez = sizeZ;
                            px = ppc_fmadds(sizeX, static_cast<float>(xi + 1), orgX);
                            pz = ppc_fmadds(sizeZ, static_cast<float>(zi), orgZ);
                        }
                    } else {
                        if (j == 0) {
                            y0 = h00;
                            da = h01 - h00;
                            db = h10 - h00;
                            ex = sizeX;
                            ez = sizeZ;
                            px = ppc_fmadds(sizeX, static_cast<float>(xi), orgX);
                            pz = ppc_fmadds(sizeZ, static_cast<float>(zi), orgZ);
                        } else {
                            y0 = h11;
                            da = h10 - h11;
                            db = h01 - h11;
                            ex = -sizeX;
                            ez = -sizeZ;
                            px = ppc_fmadds(sizeX, static_cast<float>(xi + 1), orgX);
                            pz = ppc_fmadds(sizeZ, static_cast<float>(zi + 1), orgZ);
                        }
                    }
                    // the plane normal: (da, 0, ex) x (0, db, ez) written out with the zero terms the Mac multiplies
                    float normalY = ppc_fnmsubs(ex, ez, 0.0f);
                    const float zeroTermA = da * 0.0f;
                    const float zeroTermB = 0.0f * db;
                    float normalX = ppc_fmsubs(da, ez, zeroTermB);
                    float normalZ = ppc_fmsubs(ex, db, zeroTermA);
                    // the cell's extent (the z extent uses the x size)
                    const float cellX0 = ppc_fmadds(sizeX, static_cast<float>(xi), orgX);
                    const float cellZ0 = ppc_fmadds(sizeX, static_cast<float>(zi), orgZ);
                    const float cellX1 = ppc_fmadds(sizeX, static_cast<float>(xi + 1), orgX);
                    const float cellZ1 = ppc_fmadds(sizeX, static_cast<float>(zi + 1), orgZ);
                    const float ySquared = normalY * normalY;
                    const float zSquared = normalZ * normalZ;
                    const double lengthSquared =
                        static_cast<double>(zSquared) + ppc_fmadd(static_cast<double>(normalX), static_cast<double>(normalX),
                                                                  static_cast<double>(ySquared));
                    const float length = static_cast<float>(std::sqrt(lengthSquared));
                    normalY = normalY / length;
                    normalX = normalX / length;
                    normalZ = normalZ / length;
                    const float k7 = normalY * y0;
                    const float k8 = normalY * cam.y;
                    const float k9 = normalY * dy;
                    float num = ppc_fmadds(normalX, px, k7);
                    float camTerm = ppc_fmadds(normalX, cam.x, k8);
                    float dirTerm = ppc_fmadds(normalX, dx, k9);
                    num = ppc_fnmadds(normalZ, pz, num);
                    camTerm = ppc_fmadds(normalZ, cam.z, camTerm);
                    dirTerm = ppc_fmadds(normalZ, dz, dirTerm);
                    num = num + camTerm;
                    const float t = num / dirTerm;
                    const float hx = ppc_fnmsubs(t, dx, cam.x);
                    const float hy = ppc_fnmsubs(t, dy, cam.y);
                    const float hz = ppc_fnmsubs(t, dz, cam.z);
                    if (hx < cellX0) continue;
                    if (hz < cellZ0) continue;
                    if (hx > cellX1) continue;
                    if (hz > cellZ1) continue;
                    const float distanceAlong = static_cast<float>(std::fabs(t));
                    if (!(distanceAlong < best)) continue;
                    hit.x = hx;
                    best = distanceAlong;
                    hitFound = 1;
                    hit.y = hy;
                    hit.z = hz;
                }
            }
        }

        // ---- C: the rides. Walk the ray in 10 unit steps along the axis it moves most on.
        float rx = cam.x;   // f18
        float ry = cam.y;   // f16
        float rz = cam.z;   // f17
        const bool alongX = std::fabs(dx) > std::fabs(dz);
        float step;         // f14
        float otherStep;    // f23
        float yStep;        // f15
        if (alongX) {
            step = dx > 0.0f ? 10.0f : -10.0f;
            const float ratioZ = dz / dx;
            const float ratioY = dy / dx;
            otherStep = step * ratioZ;
            yStep = step * ratioY;
        } else {
            step = dz > 0.0f ? 10.0f : -10.0f;
            const float ratioX = dx / dz;
            const float ratioY = dy / dz;
            otherStep = step * ratioX;
            yStep = step * ratioY;
        }
        const float limit = hitFound != 0 ? hit.y + yStep : 0.0f;
        tag_sRideAnimSet* ride = nullptr;
        while (ry >= limit) {
            uint32_t cellX = cw_cvt_fp2unsigned(rx / 10.0f);
            uint32_t cellZ = cw_cvt_fp2unsigned(rz / 10.0f);
            ride = FindPointedRide(cellX, cellZ, alongX ? 1 : 0, rx, ry, rz, yStep, &dir, &gCameraViewPoint, step);
            if (ride != nullptr) break;
            const float oldX = rx;
            const float oldY = ry;
            const float oldZ = rz;
            if (alongX) {
                rx = rx + step;
                ry = ry + yStep;
                rz = rz + otherStep;
            } else {
                rx = rx + otherStep;
                ry = ry + yStep;
                rz = rz + step;
            }
            bool crossed = false;
            float fraction = 0.0f;   // 0x14c
            double whole;
            if (alongX) {
                if (cw_cvt_fp2unsigned(rz / 10.0f) != cellZ) {
                    const float before = oldZ / 10.0f;
                    const float after = rz / 10.0f;
                    if (before < after) {
                        const float span = after - before;
                        const float part = static_cast<float>(std::modf(static_cast<double>(after), &whole));
                        fraction = 1.0f - part / span;
                    } else {
                        const float span = before - after;
                        const float part = static_cast<float>(std::modf(static_cast<double>(before), &whole));
                        fraction = part / span;
                    }
                    crossed = true;
                }
            } else {
                if (cw_cvt_fp2unsigned(rx / 10.0f) != cellX) {
                    const float before = oldX / 10.0f;
                    const float after = rx / 10.0f;
                    if (before < after) {
                        const float span = after - before;
                        const float part = static_cast<float>(std::modf(static_cast<double>(after), &whole));
                        fraction = 1.0f - part / span;
                    } else {
                        const float span = before - after;
                        const float part = static_cast<float>(std::modf(static_cast<double>(before), &whole));
                        fraction = part / span;
                    }
                    crossed = true;
                }
            }
            if (rx >= 0.0f && rz >= 0.0f && crossed) {
                fraction = fraction * 1.01f;
                const float rest = 1.0f - fraction;
                const float ix = ppc_fmadds(oldX, rest, rx * fraction);
                const float iy = ppc_fmadds(oldY, rest, ry * fraction);
                const float iz = ppc_fmadds(oldZ, rest, rz * fraction);
                cellX = cw_cvt_fp2unsigned(ix / 10.0f);
                cellZ = cw_cvt_fp2unsigned(iz / 10.0f);
                ride = FindPointedRide(cellX, cellZ, alongX ? 1 : 0, ix, iy, iz, yStep, &dir, &gCameraViewPoint, step);
                if (ride != nullptr) break;
            }
        }
        gPointedRide = ride;
        SetPotentiallyHighlightedRide(gPointedRide);
    }

    // ---- D: the answer
    if (result != nullptr) *result = hitFound;
    if (position == nullptr) return;
    bool useRide;
    if (gPointedRide == nullptr) {
        useRide = false;
    } else if (hitFound == 0) {
        useRide = true;
    } else {
        const float rideDistance = DistanceBetweenPoints(&gPointedRidePosition, &gCameraViewPoint);
        const float terrainDistance = DistanceBetweenPoints(&hit, &gCameraViewPoint);
        useRide = !(terrainDistance <= rideDistance);
    }
    if (useRide) {
        CopyPoint(position, &gPointedRidePosition);
        if (pickup != nullptr) CopyPoint(pickup, &gPointedRidePosition);
    } else if (hitFound != 0) {
        CopyPoint(position, &hit);
        if (pickup != nullptr) CopyPoint(pickup, &hit);
    }
}
