#include "tb_mesh.h"

void CMesh::SetLocalMatrix(int32_t flags, CMatrix& matrix) {
    CMatrix flipY;
    flipY.m[1][1] = -1.0f;
    if ((flags & 1) != 0) {
        mMatrix = matrix * flipY;
        mFlags |= 1;
    }
    if ((flags & 2) != 0) {
        mMatrix2 = matrix * flipY;
        mFlags |= 2;
    }
}

void CMesh::SetLocalMatrix(int32_t flags, sMatrix& matrix) {
    CMatrix copy;
    std::memcpy(&copy, &matrix, sizeof(matrix));
    SetLocalMatrix(flags, copy);
}
