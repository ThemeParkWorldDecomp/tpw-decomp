// Direct3D data types used by game-side code (DirectX 6 layouts). Types only: the
// Direct3D objects belong to the platform layer (analysis/platform-layer-design.md).
#pragma once

// D3DVECTOR (d3dtypes.h): three floats, 12 bytes. Passed by value in several engine
// functions (CMatrix::View), where CodeWarrior spreads it over three GPR slots.
typedef struct _D3DVECTOR {
    float x;
    float y;
    float z;
} D3DVECTOR;
static_assert(sizeof(D3DVECTOR) == 12, "D3DVECTOR layout");
