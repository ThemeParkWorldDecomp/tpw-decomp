// MatrixPowerPC.h: the Mac build's transform unit (engine library, functions at
// 0x100418ac..0x10042ba8). The original file name is unknown (the code follows the last
// static initialiser of the engine, tb_printf_opengl.cpp); the name here is the class's.
#pragma once
#include "tb_matrix.h"

// CodeWarrior layout (0x14 bytes): +0 pointer to the CTransformBase virtual base
// (this + 8), +4 vtable, +8 the CTransformBase subobject.
class CMatrixPowerPC : public virtual CTransformBase {
public:
    CMatrixPowerPC() {}
    ~CMatrixPowerPC() override;

    void Transform(sMatrix* m, void* out, sVertexPosnSet* in, int32_t outStride, int32_t first,
                   int32_t count) override;
    void Transform(sMatrix* m, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                   int32_t type) override;
    float Project(void* verts, int32_t stride, int32_t count, int32_t type) override;
    void Light(SFVF_TLCTEX* verts, tag_sNode* node, sFaceSet* faces, int32_t count) override;
    void TransformUV(STMCoordinateSet* out, sUVSet* in, int32_t a, int32_t b, int32_t c) override;
    void MultMatrix(const float* a, const float* b, float* out) override;
};
