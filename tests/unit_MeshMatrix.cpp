// Fakes for the engine's CMesh::SetLocalMatrix unit (src/engine/Mesh.cpp): the math unit
// is a real CMatrixPowerPC (src/engine/MatrixPowerPC.cpp is part of the unit), the mesh is one
// static object with a fake constructor, and nothing is recorded.
#include "MatrixPowerPC.h"
#include "tb_mesh.h"
#include "tb_resource_display.h"
#include "platform/interfacelib.h"
#include <cstring>
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::Prepare() { return 1; }
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }
CTransformBase::~CTransformBase() {}
CDisplay::CDisplay() {}
CDisplay gei_Display;
void DebugStr(const unsigned char*) {}
namespace {
CMatrixPowerPC unit;
CMesh mesh;
}
CTransformBase* gei_MathUnit = &unit;
// io: the mesh's flags word then its two local matrices (33 words), replaced by the state
// after the call. which 0 is the CMatrix& overload, 1 the sMatrix& one.
extern "C" void tpw_mm_run(int32_t which, int32_t flags, const uint32_t* matrixWords, uint32_t* io) {
    mesh.mFlags = io[0];
    std::memcpy(&mesh.mMatrix, io + 1, sizeof mesh.mMatrix);
    std::memcpy(&mesh.mMatrix2, io + 17, sizeof mesh.mMatrix2);
    CMatrix matrix;
    std::memcpy(&matrix, matrixWords, sizeof matrix);
    if (which == 0) {
        mesh.SetLocalMatrix(flags, matrix);
    } else {
        mesh.SetLocalMatrix(flags, static_cast<sMatrix&>(matrix));
    }
    io[0] = mesh.mFlags;
    std::memcpy(io + 1, &mesh.mMatrix, sizeof mesh.mMatrix);
    std::memcpy(io + 17, &mesh.mMatrix2, sizeof mesh.mMatrix2);
}
