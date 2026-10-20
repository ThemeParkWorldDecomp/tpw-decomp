// Only the external-camera path is needed by the lobby. HandleCamera's park branches
// remain visibly pending in the launcher; they are not counted as translated code.
#include "TPCamera.h"
#include "rsse.h"

uint32_t gFirstPersonFlags;
sMatrix gExternalCameraMatrix;
sMatrix gInverseCameraMatrix;
sMatrix* gCurrentCameraMatrix;

void SetExternalCamera(int32_t enabled) {
    if (enabled) gFirstPersonFlags |= 0x80;
    else gFirstPersonFlags &= ~0x80u;
}

bool HandleExternalCamera() {
    if ((gFirstPersonFlags & 0x80) == 0) return false;
    gCurrentCameraMatrix = &gExternalCameraMatrix;
    gei_ViewMatrix->Load(gExternalCameraMatrix);
    InvertMatrix(&gInverseCameraMatrix, gCurrentCameraMatrix);
    return true;
}
