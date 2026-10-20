// The original view-volume corners, with a single-precision value at each PPC step.
#include "tb_hfield.h"
#include "tb_resource_display.h"
#include "ppc_math.h"
#include <cmath>

void CGamut::Init(float fov) {
    const float angle = 0.5f * ((3.1415927410125732421875f * fov) / 180.0f);
    const float y = 0.5f * static_cast<float>(std::tan(static_cast<double>(angle)));
    const float x = y / gei_Display.mF04;
    mCorners[0][0] = -x; mCorners[0][1] = y;  mCorners[0][2] = 1.0f;
    mCorners[1][0] = x;  mCorners[1][1] = y;  mCorners[1][2] = 1.0f;
    mCorners[2][0] = x;  mCorners[2][1] = -y; mCorners[2][2] = 1.0f;
    mCorners[3][0] = -x; mCorners[3][1] = -y; mCorners[3][2] = 1.0f;
}

void CGamut::SetX(float low, float high, float cell) {
    mF3C[0] = low; mF3C[1] = high; mF3C[4] = cell;
    if (cell != 0.0f) { mF54[0] = ppc_fctiwz((high - low) / cell); mF54[2] = mF54[0] - 1; }
}
void CGamut::SetZ(float low, float high, float cell) {
    mF3C[2] = low; mF3C[3] = high; mF3C[5] = cell;
    if (cell != 0.0f) { mF54[1] = ppc_fctiwz((high - low) / cell); mF54[3] = mF54[1] - 1; }
}
void CGamut::CalcFurthestDistance() {
    const float dx = mF3C[1] - mF3C[0], dz = mF3C[3] - mF3C[2];
    mFurthestDistance = static_cast<float>(std::sqrt(static_cast<double>(ppc_fmadds(dx, dx, dz * dz))));
}
