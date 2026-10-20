// tb_shadow.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x10029364..0x10029940). ppc_* helpers mark fused instructions (ppc_math.h).
#include "tb_shadow.h"

#include <cmath>

#include "ppc_math.h"

CShadow gShadow;
float gShadowHeadingReference[2] = {0.0f, 1.0f};

CShadow::~CShadow() {
}

void CShadow::ResetLightSources() {
    mCurrent = 0;
    mLightCount = 0;
}

int32_t CShadow::AddLightSource(D3DVECTOR dir) {
    if (mLightCount >= 4) return 0;

    float lenSq = dir.z * dir.z + (dir.x * dir.x + dir.y * dir.y);
    float inv = 1.0f;
    if (lenSq != 0.0f) inv = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(lenSq)));
    dir.x *= inv;
    dir.y *= inv;
    dir.z *= inv;
    mLightDirections[mLightCount] = dir;

    // Project onto the ground (y = 0) along the light: x' = x - y*dx/dy, z' = z - y*dz/dy.
    CMatrix& m = mShadowMatrices[mLightCount];
    m.SetIdentity();
    m.m[0][0] = 1.0f;
    m.m[1][1] = 0.0f;
    m.m[2][2] = 1.0f;
    m.m[3][3] = 1.0f;
    m.m[1][0] = -(dir.x / dir.y);
    m.m[1][2] = -(dir.z / dir.y);

    D3DVECTOR& cross = mLightCross[mLightCount];
    cross.x = ppc_fmsubs(dir.z, gUpVector.y, dir.y * gUpVector.z);
    cross.y = ppc_fmsubs(dir.x, gUpVector.z, dir.z * gUpVector.x);
    cross.z = ppc_fmsubs(dir.y, gUpVector.x, dir.x * gUpVector.y);

    float shade =
        127.0f * ppc_fnmadds(gUpVector.z, dir.z, ppc_fmadds(gUpVector.x, dir.x, gUpVector.y * dir.y));
    if (shade <= 0.0f) return 1;   // the light does not reach the ground: slot not kept
    mLightShades[mLightCount] = cw_cvt_fp2unsigned(shade);

    // The original also measures the view's horizontal axis and drops the result.
    CMatrix* view = gei_ViewMatrix;
    float viewLenSq = ppc_fmadds(view->m[1][0], view->m[1][0], view->m[1][2] * view->m[1][2]);
    if (viewLenSq != 0.0f) (void)std::sqrt(static_cast<double>(viewLenSq));

    // Heading on the ground plane. The length is taken over x and y but x and z are
    // scaled (as in the original).
    float horizSq = ppc_fmadds(dir.y, dir.y, dir.x * dir.x);
    float hinv = 1.0f;
    if (horizSq != 0.0f) hinv = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(horizSq)));
    dir.x *= hinv;
    dir.z *= hinv;

    float ax = dir.x, ay = dir.z;
    float bx = gShadowHeadingReference[0], by = gShadowHeadingReference[1];
    float dot = ppc_fmadds(bx, ax, by * ay);
    float lenB = static_cast<float>(std::sqrt(static_cast<double>(ppc_fmadds(bx, bx, by * by))));
    float lenA = static_cast<float>(std::sqrt(static_cast<double>(ppc_fmadds(ax, ax, ay * ay))));
    float sinCross = ppc_fmsubs(bx, ay, by * ax);
    double denom = static_cast<double>(lenB) * static_cast<double>(lenA);
    double s = std::asin(static_cast<double>(sinCross) / denom);
    float angle;
    if (s >= 0.0) {
        angle = static_cast<float>(std::acos(static_cast<double>(dot) / denom));
    } else {
        angle = static_cast<float>(static_cast<double>(6.2831855f) - std::acos(static_cast<double>(dot) / denom));
    }
    mLightAngles[mLightCount] = angle;
    mLightCount++;
    return 1;
}

CMatrix* CShadow::GetFirstShadowMatrix() {
    CMatrix* result = nullptr;
    mCurrent = 0;
    if (mCurrent < mLightCount) result = &mShadowMatrices[0];
    return result;
}

CMatrix* CShadow::GetNextShadowMatrix() {
    mCurrent++;
    if (mCurrent < mLightCount) return &mShadowMatrices[mCurrent];
    return nullptr;
}
