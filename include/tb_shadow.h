// tb_shadow.h: projected shadows for up to four light sources (engine library, original
// tb_shadow.cpp, functions at 0x10029364..0x10029940).
#pragma once
#include <cstdint>

#include "platform/d3d_types.h"
#include "tb_matrix.h"

class CShadow {
public:
    // Inline in the original (__sinit_tb_shadow_cpp): the CMatrix members construct to
    // identity, then the body is ResetLightSources().
    CShadow() { ResetLightSources(); }
    ~CShadow();

    void ResetLightSources();
    // Adds a light shining along `direction`; returns 0 when all four slots are used,
    // otherwise 1 (also when the light points away from the ground and is not kept).
    int32_t AddLightSource(D3DVECTOR direction);
    CMatrix* GetFirstShadowMatrix();
    CMatrix* GetNextShadowMatrix();

    CMatrix mMatrix;                  // 0x000  identity after construction; unused by these methods
    CMatrix mShadowMatrices[4];       // 0x040  projection onto the y = 0 plane along each light
    D3DVECTOR mLightCross[4];         // 0x140  gUpVector x direction (sprite shadows orient by it)
    D3DVECTOR mLightDirections[4];    // 0x170  normalised light directions
    float mLightAngles[4];            // 0x1a0  heading of each light on the ground plane, radians
    uint32_t mLightShades[4];         // 0x1b0  127 * -(gUpVector . direction)
    int32_t mLightCount;              // 0x1c0
    int32_t mCurrent;                 // 0x1c4  GetFirst/GetNextShadowMatrix position
};
static_assert(sizeof(CShadow) == 0x1c8, "CShadow layout");

extern CShadow gShadow;   // engine data 0x10166018 (not exported; name invented)

// (0, 1): the ground-plane direction AddLightSource measures light headings from
// (initialised data 0x10051da8; static in the original, name invented).
extern float gShadowHeadingReference[2];
