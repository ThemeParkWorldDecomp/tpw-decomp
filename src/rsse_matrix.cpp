// The main application's matrix helpers. Its back substitution uses separate multiply
// and subtract instructions; its decomposition uses PPC fused negative subtracts.
#include "rsse.h"
#include "tb_matrix.h"
#include "m3d2.h"
#include "ppc_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void SetMatrixIdentity(sMatrix* matrix) {
    if (matrix != nullptr) std::memset(matrix, 0, sizeof *matrix);
    for (int i = 0; i < 4; ++i) matrix->m[i][i] = 1.0f;
}

float GetLMatrixRotationAroundZAxisYOnly(sMatrix* matrix) {
    return static_cast<float>(std::atan2(-static_cast<double>(matrix->m[1][0]), static_cast<double>(matrix->m[1][1])));
}

void LUBackSubst(sMatrix& matrix, int32_t* indices, float* values) {
    int32_t first = -1;
    for (int32_t i = 0; i < 4; ++i) {
        float sum = values[indices[i]];
        values[indices[i]] = values[i];
        if (first < 0) {
            if (sum != 0.0f) first = i;
        } else {
            for (int32_t j = first; j < i; ++j) sum = sum - values[j] * matrix.m[i][j];
        }
        values[i] = sum;
    }
    for (int32_t i = 3; i >= 0; --i) {
        float sum = values[i];
        for (int32_t j = i + 1; j < 4; ++j) sum = sum - values[j] * matrix.m[i][j];
        values[i] = sum / matrix.m[i][i];
    }
}

void LUDecomp(sMatrix& matrix, int32_t* indices, float* parity) {
    float scale[4];
    int32_t pivot = 0;
    *parity = 1.0f;
    for (int32_t i = 0; i < 4; ++i) {
        float largest = 0.0f;
        for (int32_t j = 0; j < 4; ++j)
            if (largest < std::fabs(matrix.m[i][j])) largest = std::fabs(matrix.m[i][j]);
        scale[i] = 1.0f / largest;
    }
    for (int32_t j = 0; j < 4; ++j) {
        for (int32_t i = 0; i < j; ++i) {
            float sum = matrix.m[i][j];
            for (int32_t k = 0; k < i; ++k) sum = ppc_fnmsubs(matrix.m[i][k], matrix.m[k][j], sum);
            matrix.m[i][j] = sum;
        }
        float largest = 0.0f;
        for (int32_t i = j; i < 4; ++i) {
            float sum = matrix.m[i][j];
            for (int32_t k = 0; k < j; ++k) sum = ppc_fnmsubs(matrix.m[i][k], matrix.m[k][j], sum);
            matrix.m[i][j] = sum;
            const float weighted = scale[i] * std::fabs(sum);
            if (largest <= weighted) { largest = weighted; pivot = i; }
        }
        if (j != pivot) {
            for (int32_t k = 0; k < 4; ++k) std::swap(matrix.m[pivot][k], matrix.m[j][k]);
            *parity = -*parity;
            scale[pivot] = scale[j];
        }
        indices[j] = pivot;
        if (matrix.m[j][j] == 0.0f) matrix.m[j][j] = 1.0e-20f;
        if (j != 3) {
            const float reciprocal = 1.0f / matrix.m[j][j];
            for (int32_t i = j + 1; i < 4; ++i) matrix.m[i][j] *= reciprocal;
        }
    }
}

void InvertMatrix(sMatrix* destination, const sMatrix* source) {
    sMatrix decomposed = *source;
    sMatrix inverse;
    int32_t indices[4];
    float parity;
    LUDecomp(decomposed, indices, &parity);
    for (int32_t j = 0; j < 4; ++j) {
        float column[4] = {};
        column[j] = 1.0f;
        LUBackSubst(decomposed, indices, column);
        for (int32_t i = 0; i < 4; ++i) inverse.m[i][j] = column[i];
    }
    *destination = inverse;
}

// The point through the matrix's upper 3x3 and its translation row; the point is rewritten
// in place. Each coordinate is one single-precision product, two fused multiply-adds and
// the translation added last (the original reads x, y and z into registers first).
void VectorThroughLMatrixNoW(sMatrix* matrix, s3DPoint* point) {
    const float x = point->x;
    const float y = point->y;
    const float z = point->z;
    float sum = y * matrix->m[1][0];
    sum = ppc_fmadds(x, matrix->m[0][0], sum);
    sum = ppc_fmadds(z, matrix->m[2][0], sum);
    point->x = matrix->m[3][0] + sum;
    sum = y * matrix->m[1][1];
    sum = ppc_fmadds(x, matrix->m[0][1], sum);
    sum = ppc_fmadds(z, matrix->m[2][1], sum);
    point->y = matrix->m[3][1] + sum;
    sum = y * matrix->m[1][2];
    sum = ppc_fmadds(x, matrix->m[0][2], sum);
    sum = ppc_fmadds(z, matrix->m[2][2], sum);
    point->z = matrix->m[3][2] + sum;
}

// destination = source * destination, without the fourth column: for each of the first
// three columns and each row, source[row][1] * old[1][col], then fused source[row][0] *
// old[0][col] and source[row][2] * old[2][col]; row 3 adds old[3][col] last. The
// destination is copied first, the results are stored column by column, and the source is
// read again for every element (so a source that is the destination changes under it).
void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination) {
    const sMatrix old = *destination;
    for (int32_t column = 0; column < 3; ++column) {
        for (int32_t row = 0; row < 4; ++row) {
            float sum = source->m[row][1] * old.m[1][column];
            sum = ppc_fmadds(source->m[row][0], old.m[0][column], sum);
            sum = ppc_fmadds(source->m[row][2], old.m[2][column], sum);
            if (row == 3) sum = old.m[3][column] + sum;
            destination->m[row][column] = sum;
        }
    }
}

// Each row of the 3 x 3 part times its axis' scale (x for row 0, y for row 1, z for row 2): the rotation part of a node's
// matrix, in single precision, one multiply each. The translation row is left alone.
void ScaleLMatrixNoTrans(sMatrix* matrix, s3DPoint* scale) {
    matrix->m[0][0] = matrix->m[0][0] * scale->x;
    matrix->m[0][1] = matrix->m[0][1] * scale->x;
    matrix->m[0][2] = matrix->m[0][2] * scale->x;
    matrix->m[1][0] = matrix->m[1][0] * scale->y;
    matrix->m[1][1] = matrix->m[1][1] * scale->y;
    matrix->m[1][2] = matrix->m[1][2] * scale->y;
    matrix->m[2][0] = matrix->m[2][0] * scale->z;
    matrix->m[2][1] = matrix->m[2][1] * scale->z;
    matrix->m[2][2] = matrix->m[2][2] * scale->z;
}
