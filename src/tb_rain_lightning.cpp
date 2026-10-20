// tb_rain.cpp, the lightning bolts' own members (engine library):
//   0x100243cc CRain::AddLightning  (144)
//   0x100244a0 SLightning::Init     (368)
//   0x10021f48 GeneratePoint        (1092)
//   0x100223d8 CRain::NewRenderLightning (1324)
//   0x1002163c CRain::AddLightningSection (2240)
//
// The lobby's weather tick calls AddLightning for each strike; __RenderLightning
// (src/tb_rain_render.cpp) ages the bolts and draws them from the 16 slots filled here.
//
// Floating point. Every operation follows the listing: single-precision subtracts and
// multiplies are plain, fused multiply-adds and subtracts go through ppc_math.h, and the
// length is the double square root rounded to single (frsp).
#include <cmath>
#include <cstring>

#include "ppc_math.h"
#include "tb_rain.h"
#include "tb_matrix.h"
#include "tb_drawprimitive.h"
#include "platform/msl.h"

// Adds a bolt from `start` to `end` if there is room (the slots are 16): the bolt is set up
// by SLightning::Init and counted. `a` is the bolt's life in milliseconds and `b` asks for a
// seed. Always answers 1, a full table included (the strike is dropped silently).
int32_t CRain::AddLightning(D3DVECTOR start, D3DVECTOR end, int32_t a, int32_t b) {
    if (mF3308 < 16) {
        mLightning[mF3308].Init(start, end, a, b);
        mF3308 = mF3308 + 1;
    }
    return 1;
}

// Sets a bolt up. The ends and the time are stored as given. The seed (an integer word) is the
// cycle counter ORed with 1 when `seeded` is not 0, else 0; mF28 is not written. The direction
// is start - end, and then normalised unless its length is exactly 0.0f (a NaN length is
// unequal, so the division runs and the NaNs stay NaNs; an infinite length gives a scale of
// 0, so the components become zeros, or NaN where a component is infinite). The length is
// taken as sqrt(z*z + (x*x + y*y)) with the square of x fused into the sum:
// f0 = y*y, f2 = z*z (single), f0 = x*x + f0 (one rounding), sum = f2 + f0.
void SLightning::Init(D3DVECTOR start, D3DVECTOR end, int32_t time, int32_t seeded) {
    mStart = start;
    mEnd = end;
    mTime = time;
    mSeed = seeded != 0 ? (GetCycleCount() | 1u) : 0u;

    mDirection.x = start.x - end.x;
    mDirection.y = start.y - end.y;
    mDirection.z = start.z - end.z;

    float x = mDirection.x;
    float y = mDirection.y;
    float z = mDirection.z;
    const float yy = y * y;
    const float zz = z * z;
    const float partial = ppc_fmadds(x, x, yy);
    const float sum = zz + partial;
    const float length = static_cast<float>(std::sqrt(static_cast<double>(sum)));
    if (length != 0.0f) {
        const float scale = 1.0f / length;
        x = x * scale;
        y = y * scale;
        z = z * scale;
    }
    mDirection.x = x;
    mDirection.y = y;
    mDirection.z = z;
}

namespace {

// The room a polygon's work array needs: its vertices plus what the clipper's six planes can add.
// CClipping::ClipPoly alternates between the primitive's memory and the caller's array, so a quad
// clipped against two or more planes is written back here with more than four vertices (the Mac
// gives this array 21 vertices, 0x2a0 bytes of its 0x3f0-byte frame).
constexpr size_t kWorkVertices = 16;

float SingleSqrt(float sum) {
    return static_cast<float>(std::sqrt(static_cast<double>(sum)));
}

// Subdivision lengths fuse x into y, then z into that sum.
float SubdivisionLengthSquared(D3DVECTOR d) {
    return ppc_fmadds(d.z, d.z, ppc_fmadds(d.x, d.x, d.y * d.y));
}

void MathUnitTransform(sMatrix* matrix, void* out, void* in, int32_t outStride,
                       int32_t inStride, int32_t count, int32_t type) {
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(matrix, out, in, outStride, inStride, count, type);
}

}  // namespace

void GeneratePoint(D3DVECTOR* out, D3DVECTOR* a, D3DVECTOR* b, float scale) {
    const D3DVECTOR d = {b->x - a->x, b->y - a->y, b->z - a->z};
    const D3DVECTOR mid = {0.5f * (b->x + a->x), 0.5f * (b->y + a->y), 0.5f * (b->z + a->z)};
    const float m2 = mid.z * mid.z + (mid.x * mid.x + mid.y * mid.y);
    const float t1 = static_cast<float>(static_cast<int16_t>(Random()) % 0x1fff) / 8192.0f;
    const float t1Squared = t1 * t1;
    const float t2 = static_cast<float>(static_cast<int16_t>(Random()) % 0x1fff) / 8192.0f;
    float t = t2 * t1Squared;
    if (Random() & 1) t = -t;
    Random();
    D3DVECTOR p = mid;
    const int32_t component = static_cast<int16_t>(Random()) % 3;
    if (component == 0) {
        if (d.x != 0.0f) p.x = mid.x - m2 / d.x;
        else if (d.y != 0.0f) p.y = mid.y - m2 / d.y;
        else p.z = mid.z - m2 / d.z;
    } else if (component == 1) {
        if (d.y != 0.0f) p.y = mid.y - m2 / d.y;
        else if (d.z != 0.0f) p.z = mid.z - m2 / d.z;
        else p.x = mid.x - m2 / d.x;
    } else {
        if (d.z != 0.0f) p.z = mid.z - m2 / d.z;
        else if (d.x != 0.0f) p.x = mid.x - m2 / d.x;
        else p.y = mid.y - m2 / d.y;
    }
    const D3DVECTOR cross = {ppc_fmsubs(d.y, p.z, d.z * p.y),
                             ppc_fmsubs(d.z, p.x, d.x * p.z),
                             ppc_fmsubs(d.x, p.y, d.y * p.x)};
    const float lenD = SingleSqrt(SubdivisionLengthSquared(d));
    const float lenP = SingleSqrt(ppc_fmadds(p.z, p.z, ppc_fmadds(p.y, p.y, p.x * p.x)));
    const float factor = (t * (scale * lenD)) / lenP;
    p.x = p.x * factor;
    p.y = p.y * factor;
    p.z = p.z * factor;
    const float lenCross = SingleSqrt(ppc_fmadds(cross.z, cross.z,
                                               ppc_fmadds(cross.x, cross.x, cross.y * cross.y)));
    const float crossFactor = 0.0f / lenCross;
    out->x = cross.x * crossFactor + (mid.x + p.x);
    out->y = cross.y * crossFactor + (mid.y + p.y);
    out->z = cross.z * crossFactor + (mid.z + p.z);
}

int32_t CRain::NewRenderLightning(D3DVECTOR* from, D3DVECTOR* to) {
    D3DVECTOR mid;
    GeneratePoint(&mid, to, from, mF3310);
    if ((Random() & 0x1ff) < 9 && mFA188 < mLightningDepth) {
        ++mFA188;
        D3DVECTOR dir = {to->x - from->x, to->y - from->y, to->z - from->z};
        const float scale = 1.0f / static_cast<float>(static_cast<double>(mFA188 * 0x60));
        const float length = SingleSqrt(dir.z * dir.z + ppc_fmadds(dir.x, dir.x, dir.y * dir.y));
        if (length != 0.0f) {
            const float reciprocal = 1.0f / length;
            dir.x = dir.x * reciprocal;
            dir.y = dir.y * reciprocal;
            dir.z = dir.z * reciprocal;
        }
        const float f = 2.5f * (scale * static_cast<float>((Random() & 0x3ff) + 0x200));
        D3DVECTOR branch = {dir.x * f, dir.y * f, dir.z * f};
        float g = scale * static_cast<float>((Random() & 0x3ff) + 0x200);
        const D3DVECTOR direction = mLightning[mCurrentLightning].mDirection;
        branch.x = ppc_fmadds(direction.x, g, branch.x);
        branch.y = ppc_fmadds(direction.y, g, branch.y);
        branch.z = ppc_fmadds(direction.z, g, branch.z);
        const float dot = ppc_fmadds(direction.z, branch.z,
                                     ppc_fmadds(direction.x, branch.x, direction.y * branch.y));
        if (dot < 0.0f) {
            g = g * 0.5f;
            branch.x = ppc_fmadds(0.5f, branch.x, direction.x * g);
            branch.y = ppc_fmadds(0.5f, branch.y, direction.y * g);
            branch.z = ppc_fmadds(0.5f, branch.z, direction.z * g);
        }
        D3DVECTOR point = {branch.x + from->x, branch.y + from->y, branch.z + from->z};
        mLightningCounts[mFA188] = 0;
        if (NewRenderLightning(&point, from) == 0) return 0;
        if (AddLightningSection(nullptr, &point) == 0) return 0;
        --mFA188;
    }
    const float jitter = mFA184 * (0.00048828125f * static_cast<float>((Random() & 0xfff) - 0x800));
    D3DVECTOR delta = {mid.x - to->x, mid.y - to->y, mid.z - to->z};
    // bge tests that LT is clear, so an unordered comparison also subdivides.
    if (!(SubdivisionLengthSquared(delta) < mFA180 + jitter)) {
        if (NewRenderLightning(&mid, to) == 0) return 0;
    } else {
        if (AddLightningSection(&mid, to) == 0) return 0;
    }
    delta = {mid.x - from->x, mid.y - from->y, mid.z - from->z};
    if (!(SubdivisionLengthSquared(delta) < mFA180 + jitter)) {
        if (NewRenderLightning(from, &mid) == 0) return 0;
    } else {
        if (AddLightningSection(from, &mid) == 0) return 0;
    }
    return 1;
}

int32_t CRain::AddLightningSection(D3DVECTOR* from, D3DVECTOR* to) {
    if (from != nullptr && to != nullptr && from->x == to->x && from->y == to->y && from->z == to->z) return 1;
    const int32_t n = mLightningCounts[mFA188];
    mLightningCounts[mFA188] = n + 1;
    if (n == 0) {
        if (from == nullptr) return 1;
        D3DVECTOR input[2] = {*from, *to};
        float projected[2][4];
        MathUnitTransform(reinterpret_cast<sMatrix*>(gei_FinalMatrix), projected, input, 0x10, 0xc, 2, 0);
        float* point = mLightningPoints[mFA188];
        float* normal = mLightningNormals[mFA188];
        std::memcpy(point, projected[1], 16);
        normal[0] = projected[0][0] - projected[1][0];
        normal[1] = projected[0][1] - projected[1][1];
        const float sum = normal[0] * normal[0] + normal[1] * normal[1];
        if (static_cast<double>(sum) == 0.0) return 1;
        const float k = mFA16C / (static_cast<float>(static_cast<double>(mFA188 + 1)) * SingleSqrt(sum));
        normal[0] = normal[0] * k;
        normal[1] = normal[1] * k;
        float* edges = mLightningEdges[mFA188];
        std::memcpy(edges + 4, projected[1], 16);
        std::memcpy(edges, edges + 4, 16);
        edges[0] = edges[0] + normal[1];
        edges[1] = edges[1] - normal[0];
        edges[4] = edges[4] - normal[1];
        edges[5] = edges[5] + normal[0];
        return 1;
    }
    float p[4];
    gei_FinalMatrix->TransformVertex(p, to, 0);
    float* point = mLightningPoints[mFA188];
    float* edges = mLightningEdges[mFA188];
    float* normal = mLightningNormals[mFA188];
    float dx = p[0] - point[0];
    float dy = p[1] - point[1];
    const float sum = from == nullptr ? ppc_fmadds(dx, dx, dy * dy) : dx * dx + dy * dy;
    if (static_cast<double>(sum) == 0.0) return 1;
    const float k = mFA16C / (static_cast<float>(static_cast<double>(mFA188 + 1)) * SingleSqrt(sum));
    dx = dx * k;
    dy = dy * k;
    float v0[4], v1[4], v2[4], v3[4];
    if (from == nullptr) {
        std::memcpy(v0, edges, 16);
        std::memcpy(v1, edges + 4, 16);
        std::memcpy(v2, p, 16);
        std::memcpy(v3, p, 16);
        v3[0] = v3[0] + dy;
        v3[1] = v3[1] - dx;
        v2[0] = v2[0] - dy;
        v2[1] = v2[1] + dx;
    } else {
        float ex = dx + normal[0];
        float ey = dy + normal[1];
        const float eSum = ex * ex + ey * ey;
        if (static_cast<double>(eSum) == 0.0) return 1;
        const float k2 = mFA16C / (static_cast<float>(static_cast<double>(mFA188 + 1)) * SingleSqrt(eSum));
        ex = ex * k2;
        ey = ey * k2;
        std::memcpy(v0, edges, 16);
        std::memcpy(v1, edges + 4, 16);
        std::memcpy(v2, point, 16);
        std::memcpy(v3, point, 16);
        v3[0] = v3[0] + ey;
        v3[1] = v3[1] - ex;
        v2[0] = v2[0] - ey;
        v2[1] = v2[1] + ex;
        normal[0] = dx;
        normal[1] = dy;
        std::memcpy(point, p, 16);
        std::memcpy(edges, v3, 16);
        std::memcpy(edges + 4, v2, 16);
    }
    // The Mac leaves specular as stack contents; the host starts it at zero.
    SFVF_TLCTEX quad[kWorkVertices] = {};
    const float* vertices[4] = {v0, v1, v2, v3};
    const float u[4] = {mFA170, mFA178, mFA178, mFA170};
    const float v[4] = {mFA174, mFA174, mFA17C, mFA17C};
    for (int32_t i = 0; i < 4; ++i) {
        std::memcpy(&quad[i], vertices[i], 16);
        quad[i].colour = mLightningColor;
        quad[i].u = u[i];
        quad[i].v = v[i];
    }
    if (gei_DrawPrimitive->AddPoly_TLCTEX(quad, 4, 0x20, nullptr, 0) == 0) return 0;
    return 1;
}
