// TPPointer.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorQueue.h"
#include "Clock.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "m3d2.h"
#include "MapCell.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "tb_hfield.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "ui_main.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>


// Unit v51021: the ride pointer pick. RayPolyCheckOnRide (0x10065648) walks a node tree testing the ray (gRayOrigin, gRayDirection)
// against every face; FindPointedRide (0x10065b24) picks the ride in a height-field square and tests its mesh.

// Defined here: no other file names them.
s3DPoint gRayHitNormal = {0, 0, 0};     // 0x102c6ca8
s3DPoint gRayHitPoint = {0, 0, 0};      // 0x102c6cb4
s3DPoint* gRayOrigin = nullptr;         // 0x102c6cc0
s3DPoint* gRayDirection = nullptr;      // 0x102c6cc4
s3DPoint gPointedRideNormal = {0, 0, 0};   // 0x102c6cd8

int32_t RayPolyCheckOnRide(tag_sNode* node, sMatrix matrix) {
    int32_t result = 0;
    CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    if ((node->mFlags & 0x10) == 0 && (node->mFlags & 0x8210) == 0) {
        GetNodeWorldVertPosnsIntoGeomBuffer(node, &matrix);
        uint16_t* indices = node->mF94;
        sFaceSet* faceSet = node->mFaceSets;
        for (uint32_t setsLeft = node->mFaceSetCount; setsLeft != 0; setsLeft--, faceSet++) {
            sFace* face = node->mFaces.get() + faceSet->mFirstFace;
            for (uint32_t facesLeft = faceSet->mFaceCount; facesLeft != 0; facesLeft--, face++) {
                const s3DPoint* source = node->mPoints.get() + (face->mIndexAndFlag >> 1);
                char* geom = static_cast<char*>(gScratchGeomBuffer);
                const float* v1 = reinterpret_cast<const float*>(geom + indices[face->mF02[0]] * 12u);
                const float* v2 = reinterpret_cast<const float*>(geom + indices[face->mF02[1]] * 12u);
                const float* v3 = reinterpret_cast<const float*>(geom + indices[face->mF02[2]] * 12u);
                s3DPoint n = *source;
                VectorThroughLMatrixNoWNoTrans(&matrix, &n);
                {
                    double xx = static_cast<double>(n.x) * static_cast<double>(n.x);
                    double t = xx + static_cast<double>(n.y * n.y);
                    double sum = static_cast<double>(n.z * n.z) + t;
                    float len = static_cast<float>(std::sqrt(sum));
                    n.x = n.x / len;
                    n.y = n.y / len;
                    n.z = n.z / len;
                }
                const float* o = reinterpret_cast<const float*>(gRayOrigin);
                const float* d = reinterpret_cast<const float*>(gRayDirection);
                float a = n.y * v1[1];
                float b = ppc_fmadds(n.x, v1[0], a);
                float f0 = o[1] * n.y;
                float f1 = ppc_fmadds(o[0], n.x, f0);
                float f3 = ppc_fnmadds(n.z, v1[2], b);
                float f2 = ppc_fmadds(o[2], n.z, f1);
                float g0 = d[1] * n.y;
                float g1 = ppc_fmadds(d[0], n.x, g0);
                float num = f3 + f2;
                float den = ppc_fmadds(d[2], n.z, g1);
                float t = (-num) / den;
                float hx = ppc_fmadds(t, d[0], o[0]);
                float hy = ppc_fmadds(t, d[1], o[1]);
                float hz = ppc_fmadds(t, d[2], o[2]);
                float ay = v1[1] - hy, ax = v1[0] - hx, az = v1[2] - hz;
                float bx = v2[0] - hx, by = v2[1] - hy, bz = v2[2] - hz;
                float cx = v3[0] - hx, cy = v3[1] - hy, cz = v3[2] - hz;
                float ayy = ay * ay;
                double s1 = static_cast<double>(az * az) + ppc_fmadd(ax, ax, ayy);
                float len1 = static_cast<float>(std::sqrt(s1));
                float byy = by * by;
                float bzz = bz * bz;
                ax = ax / len1;
                ay = ay / len1;
                az = az / len1;
                double s2 = static_cast<double>(bzz) + ppc_fmadd(bx, bx, byy);
                float len2 = static_cast<float>(std::sqrt(s2));
                float cyy = cy * cy;
                float czz = cz * cz;
                bx = bx / len2;
                by = by / len2;
                bz = bz / len2;
                double s3 = static_cast<double>(czz) + ppc_fmadd(cx, cx, cyy);
                float len3 = static_cast<float>(std::sqrt(s3));
                cx = cx / len3;
                cy = cy / len3;
                cz = cz / len3;
                float h0 = ay * by;
                float h1 = ppc_fmadds(ax, bx, h0);
                float h2 = ppc_fmadds(az, bz, h1);
                float angle = static_cast<float>(std::acos(static_cast<double>(h2)));
                float k0 = by * cy;
                float k1 = ppc_fmadds(bx, cx, k0);
                float k2 = ppc_fmadds(bz, cz, k1);
                float angle2 = static_cast<float>(std::acos(static_cast<double>(k2)));
                float m0 = cy * ay;
                float m1 = ppc_fmadds(cx, ax, m0);
                angle = angle + angle2;
                float m2 = ppc_fmadds(cz, az, m1);
                float angle3 = static_cast<float>(std::acos(static_cast<double>(m2)));
                angle = angle + angle3;
                if (angle >= 6.2821855545043945f) {
                    gRayHitPoint.x = hx;
                    gRayHitPoint.y = hy;
                    gRayHitPoint.z = hz;
                    gRayHitNormal = n;
                    return 1;
                }
            }
            // the original steps the face-set record by 0x10 bytes (sFaceSet's size)
        }
    }
    if ((node->mFlags & 0x20) == 0) {
        tag_sNode* child = node->mF0C;
        while (child != nullptr && result == 0) {
            result = RayPolyCheckOnRide(child, matrix);
            child = child->mF08;
        }
    }
    return result;
}

tag_sRideAnimSet* FindPointedRide(uint32_t cellX, uint32_t cellZ, int32_t axis, float x, float y, float z, float yStep,
                                  s3DPoint* direction, s3DPoint* camera, float step) {
    uint32_t extra = 0;
    int32_t cx = static_cast<int32_t>(cellX);
    int32_t cz = static_cast<int32_t>(cellZ);
    if (cx < 0) cx = 0;
    if (cz < 0) cz = 0;
    if (cx >= gHeightfieldWidth) cx = gHeightfieldWidth - 1;
    if (cz >= gHeightfieldHeight) cz = gHeightfieldHeight - 1;
    float height = GetWorldGSquareHeight(cx, cz, &extra);
    if (extra == 0) return nullptr;
    tag_sRideAnimSet* ride = gRideAnimSets[extra];
    uint32_t count = 0;
    sBraceMeshList* pylon = GetPylonFromBraceMesh(extra, &count);
    if (pylon == nullptr) count = 1;
    for (uint32_t i = 0; i < count; i++) {
        if (pylon != nullptr) {
            uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(GetNthBraceMesh(pylon, count - i - 1)));
            uint32_t idCopy = id;
            ride = gRideAnimSets[id];
            height = GetWorldGSquareHeight(cx, cz, &idCopy);
        } else {
            tag_sM3D2Header* header = ride->mHeads->mHeader;
            uint32_t flags = header->mFlags;
            if ((flags & 0x40000) != 0 && (flags & 0x20000) != 0) RecalculateInvalidFaceNormals(header);
        }
        float frac;
        const float source = axis != 0 ? x : z;
        if (step > 0.0f) {
            double whole;
            frac = static_cast<float>(std::modf(static_cast<double>(source / 10.0f), &whole));
        } else {
            float rem = static_cast<float>(std::fmod(static_cast<double>(source), 10.0));
            frac = 1.0f - rem / 10.0f;
        }
        float low = ppc_fnmsubs(frac, yStep, y);
        float high = ppc_fmadds(1.0f - frac, yStep, y);
        const float lo = (high < low) ? high : low;
        if (lo <= height) {
            const float hi = (low < high) ? high : low;
            tag_sM3D2Header* header = ride->mHeads->mHeader;
            if (hi >= header->mBox.mMin[1]) {
                gRayDirection = direction;
                gRayOrigin = camera;
                sMatrix identity;
                SetMatrixIdentity(&identity);
                int32_t hit = RayPolyCheckOnRide(header->mF78, identity);
                if (hit != 0) {
                    gPointedRidePosition = gRayHitPoint;
                    gPointedRideNormal = gRayHitNormal;
                    return ride;
                }
            }
        }
    }
    return nullptr;
}

// TPRide.cpp (main app 0x10065f50, unit w134): CalculatePointerWorldPosn, the pointer's world position from the camera ray through
// the terrain and the rides.

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

// GetMapCellFromPointerWorldPosn (main app, SimTheme Park 2.04b3, TPEngine.cpp; unit w139), 0x10067084, 348 bytes.
//
// Turns the pointer's world position into the id of the map cell under it: with the height field's origin (mF08[0], mF08[1]) and cell size
// (mF08[2], mF08[3]), x = trunc((pos.x - origin x) / size x) and y = trunc((pos.z - origin y) / size y) (single precision subtract and
// divide, then fctiwz). Both are clamped, in the listing's order: negatives to 0, then (UNSIGNED compare) to the field's counts mF18 less one,
// negatives to 0 again, then (signed) to gHeightfieldWidth / Height less one. gIntrface.mF24 receives 0 when IsXYValid refuses the pair,
// else x + (y << 7) + 1 cut to 16 bits. Without a height field header or field nothing happens.

void GetMapCellFromPointerWorldPosn(s3DPoint* position) {
    const tag_sM3D2Header* const header = gHeightfieldHeader;
    if (header == nullptr) return;
    const sHField* const field = header->mHField;
    if (field == nullptr) return;

    const float dx = position->x - field->mF08[0];
    const float dz = position->z - field->mF08[1];
    int32_t x = ppc_fctiwz(static_cast<double>(dx / field->mF08[2]));
    int32_t y = ppc_fctiwz(static_cast<double>(dz / field->mF08[3]));
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (static_cast<uint32_t>(x) >= field->mF18[0]) x = static_cast<int32_t>(field->mF18[0]) - 1;
    if (static_cast<uint32_t>(y) >= field->mF18[1]) y = static_cast<int32_t>(field->mF18[1]) - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
    if (y >= gHeightfieldHeight) y = gHeightfieldHeight - 1;

    if (CMap::IsXYValid(x, y) == 0) {
        gIntrface.mF24 = 0;
    } else {
        gIntrface.mF24 = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
    }
}

// TPRide.cpp (main app), unit w143: GetPickupWorldPosn (0x10067220, 8 bytes): `lwz r3, TOC -> 0x102c6cec; blr`, the address of the world
// point the pointer picks up (gPickupWorldPosn, src/GameSys.cpp).

s3DPoint* GetPickupWorldPosn() {
    return &gPickupWorldPosn;
}

// TPRide.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: GetPointedAtRide (0x10067254, 12 bytes) and
// LockPointedAtRide (0x10067288, 52 bytes).

tag_sRideAnimSet* GetPointedAtRide() { return gHighlightedRide; }

// Keeps the highlight when asked to and a ride is highlighted; otherwise lets it go.
void LockPointedAtRide(int32_t flag) { gRideHighlightKeep = (flag != 0 && gHighlightedRide != nullptr) ? 1 : 0; }

// Unit v51049: std::locale::~locale 0x10070d44, std::basic_filebuf<char>::imbue 0x10070bf4 and pbackfail 0x100715b4,
// DisableRideHighlight 0x100672e4, ESP_GetRandomSpritesFrom 0x1007c900, ToggleProceduralTexturing 0x100634f8,
// DestroyDynamicMeshSetInstance 0x10063264, CAdvisor::SetPlayingFor 0x1000c7fc.

// 0x102d1f04: DisableRideHighlight's flag (zero at start, like the Mac's bss).
int32_t gRideHighlightDisabled;

// ---- DisableRideHighlight (0x100672e4)
void DisableRideHighlight(int32_t flag) {
    if (flag == 0) {
        gRideHighlightDisabled = 0;
    } else {
        if (gHighlightedRide != nullptr) UnhighlightRide();
        gRideHighlightDisabled = 1;
    }
}
