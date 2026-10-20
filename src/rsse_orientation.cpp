// rsse.cpp (main app of SimTheme Park 2.04b3): SetLMatrixOrientationHi (0x100c1db4, 192 bytes), unit w135.
#include <cmath>

#include "rsse.h"
#include "tb_matrix.h"

// The 3 x 3 part is built in two steps. The pitch about X first: row 0 is (1, 0, 0), row 1 (0, cos, sin) and row 2 (0, -sin, cos), each
// of the C library's sin and cos of the pitch (as doubles) rounded to single, the negation done on the single. Row 3 and the fourth
// column are not written. Then the yaw: YRotateLMatrixNoTrans(matrix, cos(yaw), sin(yaw)) with the same rounding.
void SetLMatrixOrientationHi(sMatrix* matrix, float pitch, float yaw) {
    const float sp = static_cast<float>(std::sin(static_cast<double>(pitch)));
    const float cp = static_cast<float>(std::cos(static_cast<double>(pitch)));
    matrix->m[2][0] = 0.0f;
    matrix->m[1][0] = 0.0f;
    matrix->m[0][2] = 0.0f;
    matrix->m[0][1] = 0.0f;
    matrix->m[0][0] = 1.0f;
    matrix->m[1][1] = cp;
    matrix->m[1][2] = sp;
    matrix->m[2][1] = -sp;
    matrix->m[2][2] = cp;
    const float sy = static_cast<float>(std::sin(static_cast<double>(yaw)));
    const float cy = static_cast<float>(std::cos(static_cast<double>(yaw)));
    YRotateLMatrixNoTrans(matrix, cy, sy);
}
