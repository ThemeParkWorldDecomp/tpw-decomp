// TPCamera.cpp range of the main app (SimTheme Park 2.04b3): InitCamera (0x100334d4, 344 bytes) and Load1stPersonMesh
// (0x10032558, 4 bytes). The park camera's globals that these set are defined here.
#include <cstring>

#include "Clock.h"
#include "TPCamera.h"
#include "rsse.h"
#include "tb_hfield.h"
#include "tb_matrix.h"

// Initialised data (0x1029cc68..0x1029cc87) and BSS (0x102b5a90..0x102b5bf4).
float gCameraF68 = 110.0f;
float gCameraStartPosition[3] = {475.0f, 0.0f, 175.0f};
float gCameraPosition[3] = {0.0f, 0.0f, 0.0f};
float gCameraFov = 1.5707963705062866f;
uint32_t gCameraClockStamp;
uint32_t gCameraModeA;
uint32_t gCameraModeB;
uint32_t gCameraF5AAC;
sMatrix gCameraMatrixA;
sMatrix gCameraMatrixB;
sMatrix gCameraMatrix;
float gCameraF5BB4[3];
float gCameraF5BF4;

void Load1stPersonMesh() {}

// The home position is copied as raw words (lwz/stw) over the position, and from there into the row 3 of the camera matrix as
// floats (lfs/stfs). The field of view is 57.2957763671875 (degrees per radian, as a float) times gCameraFov, a single
// precision multiply; the projection gets it with the near plane 0.1 and the far plane 1000.0.
void InitCamera() {
    gCameraF68 = 110.0f;
    gCameraF5BF4 = 0.0f;
    gFirstPersonFlags = 0;
    std::memcpy(gCameraPosition, gCameraStartPosition, sizeof gCameraPosition);
    SetMatrixIdentity(&gCameraMatrix);
    SetMatrixIdentity(&gCameraMatrixB);
    SetMatrixIdentity(&gCameraMatrixA);
    gCameraMatrix.m[3][0] = gCameraPosition[0];
    gCameraMatrix.m[3][1] = gCameraPosition[1];
    gCameraMatrix.m[3][2] = gCameraPosition[2];
    gei_ViewMatrix->Load(gCameraMatrix);
    const float fov = 57.2957763671875f * gCameraFov;
    gei_ProjectionMatrix.Projection(0.1f, 1000.0f, fov);
    gei_Gamut->Init(fov);
    const uint32_t mode = (gCameraModeA & 0x200) | 0x80;
    gCameraModeA = mode;
    gCameraModeB = mode;
    gCameraClockStamp = static_cast<uint32_t>(gClock.ReadFixed());
    std::memset(gCameraF5BB4, 0, sizeof gCameraF5BB4);
    gCameraF5AAC = 1;
    gFirstPersonFlags = 0;
}
