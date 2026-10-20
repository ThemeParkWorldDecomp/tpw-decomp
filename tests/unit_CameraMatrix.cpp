#include "TPCamera.h"
#include "rsse.h"
#include "calllog.h"
#include <cstring>
using namespace calllog;
CMatrix view;
CMatrix* gei_ViewMatrix = &view;
void CMatrix::Load(sMatrix& source) { ev("Load"); std::memcpy(this, &source, sizeof source); }
extern "C" void tpw_cm_case(int32_t op, int32_t argument, uint32_t flags, const uint32_t* seed, uint32_t* out) {
    sMatrix matrix, destination;
    int32_t indices[4];
    float values[4], parity;
    std::memcpy(&matrix, seed, 64);
    std::memset(&destination, 0xa5, 64);
    std::memcpy(indices, seed + 16, 16);
    std::memcpy(values, seed + 20, 16);
    std::memcpy(&parity, seed + 24, 4);
    std::memset(&view, 0xa5, 64);
    std::memset(&gInverseCameraMatrix, 0xa5, 64);
    gExternalCameraMatrix = matrix;
    gCurrentCameraMatrix = nullptr;
    gFirstPersonFlags = flags;
    switch (op) {
    case 0: SetExternalCamera(argument); break;
    case 1: SetMatrixIdentity(&matrix); break;
    case 2: LUDecomp(matrix, indices, &parity); break;
    case 3: LUBackSubst(matrix, indices, values); break;
    case 4: InvertMatrix(argument ? &matrix : &destination, &matrix); break;
    case 5: HandleExternalCamera(); break;
    }
    std::memcpy(out, &matrix, 64);
    std::memcpy(out + 16, &destination, 64);
    std::memcpy(out + 32, &view, 64);
    std::memcpy(out + 48, &gInverseCameraMatrix, 64);
    std::memcpy(out + 64, indices, 16);
    std::memcpy(out + 68, values, 16);
    std::memcpy(out + 72, &parity, 4);
    out[73] = gFirstPersonFlags;
    out[74] = gCurrentCameraMatrix == &gExternalCameraMatrix;
}
