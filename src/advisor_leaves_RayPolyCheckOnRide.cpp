// Unit v51021: the ride pointer pick. RayPolyCheckOnRide (0x10065648) walks a node tree testing the ray (gRayOrigin, gRayDirection)
// against every face; FindPointedRide (0x10065b24) picks the ride in a height-field square and tests its mesh.
#include <cmath>

#include "MapCell.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"

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
