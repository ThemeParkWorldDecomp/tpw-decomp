// tb_matrix.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x1001a770..0x1001c9c8, plus the out-of-line CMatrix::CMatrix at 0x1001c9f0 and
// sMatrix::Byteswap at 0x1004a57c).
//
// Every a*b+c below that is written with a ppc_* helper is a fused instruction in the
// original (see ppc_math.h); plain operators are separate fmuls/fadds/fsubs.
#include "tb_matrix.h"

#include "platform/msl.h"

#include <cmath>
#include <cstring>

#include "MatrixPowerPC.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"

// Declaration order follows __sinit_tb_matrix_cpp.
float g_100b00fc = 1.0f;
CMatrix gei_LightMatrix;
CMatrix gei_ProjectionMatrix;
CMatrix gei_InternalViewMatrix[2];
CMatrix gei_InternalInvViewMatrix[2];
CMatrix gei_InternalFinalMatrix[2];
CMatrix gScreenMatrix;

CTransformBase* gei_MathUnit;
float gViewAngle;
float gViewDir2D[4];
float* gViewDir2DPtr;
CMatrix* gei_FinalMatrix;
CMatrix* gei_InvViewMatrix;
CMatrix* gei_ViewMatrix;
float gProjectionFOV;
float gProjectionNear;
float gProjectionFar = 1.0f;
D3DVECTOR gUpVector = {0.0f, 1.0f, 0.0f};

namespace {

const float kDegToRad = 0.017453292f;   // TOC constant 0x3c8efa35
const float kTiny = 1.0e-20f;           // ludcmp's TINY, 0x1e3ce508

// Square-root length of an xyz vector and the in-place normalisation used by View: the
// sum is x*x fused into y*y, then z*z added; a zero length leaves the vector unchanged.
float Length(const D3DVECTOR& v) {
    return static_cast<float>(std::sqrt(static_cast<double>(v.z * v.z + ppc_fmadds(v.x, v.x, v.y * v.y))));
}

void Normalise(D3DVECTOR& v) {
    float len = Length(v);
    if (len != 0.0f) {
        float inv = 1.0f / len;
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
}

// a x b, each component one fmuls and one fmsubs.
D3DVECTOR Cross(const D3DVECTOR& a, const D3DVECTOR& b) {
    D3DVECTOR r;
    r.x = ppc_fmsubs(a.y, b.z, a.z * b.y);
    r.y = ppc_fmsubs(a.z, b.x, a.x * b.z);
    r.z = ppc_fmsubs(a.x, b.y, a.y * b.x);
    return r;
}

float Dot(const D3DVECTOR& a, const D3DVECTOR& b) {
    return ppc_fmadds(a.z, b.z, ppc_fmadds(a.x, b.x, a.y * b.y));
}

}  // namespace

int32_t Matrix_Init() {
    gScreenMatrix.m[0][0] = 2.0f * gei_Display.mF04;
    gScreenMatrix.m[1][1] = -2.0f;
    gei_ViewMatrix = gei_InternalViewMatrix;
    gei_InvViewMatrix = gei_InternalInvViewMatrix;
    gei_FinalMatrix = gei_InternalFinalMatrix;
    gViewDir2DPtr = gViewDir2D;
    CMatrixPowerPC* unit = new CMatrixPowerPC;
    gei_MathUnit = unit;   // the CTransformBase (virtual base) subobject, null if new failed
    gei_System.mF20 = 5;
    return 1;
}

void Matrix_Close() {
    if (gei_MathUnit != nullptr) delete gei_MathUnit;
    gei_MathUnit = nullptr;
}

int32_t PreprocessMatrices() {
    gei_InternalInvViewMatrix[0] = gei_InternalViewMatrix[0];
    gei_InternalInvViewMatrix[0].Inverse();
    gei_InternalFinalMatrix[0] = gei_InternalInvViewMatrix[0] * gei_ProjectionMatrix;
    gei_InternalFinalMatrix[0] *= gScreenMatrix;
    gViewDir2D[0] = gei_InternalInvViewMatrix[0].m[0][2];
    gViewDir2D[1] = gei_InternalInvViewMatrix[0].m[2][2];
    float lenSq = gViewDir2D[0] * gViewDir2D[0] + gViewDir2D[1] * gViewDir2D[1];
    if (lenSq == 0.0f) lenSq = 1.0f;
    float inv = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(lenSq)));
    gViewDir2D[0] *= inv;
    gViewDir2D[1] *= inv;

    if (gei_System.mFlags & CSystem::kSecondViewport) {
        gei_InternalInvViewMatrix[1] = gei_InternalViewMatrix[1];
        gei_InternalInvViewMatrix[1].Inverse();
        gei_InternalFinalMatrix[1] = gei_InternalInvViewMatrix[1] * gei_ProjectionMatrix;
        gei_InternalFinalMatrix[1] *= gScreenMatrix;
        gViewDir2D[2] = gei_InternalInvViewMatrix[1].m[0][2];
        gViewDir2D[3] = gei_InternalInvViewMatrix[1].m[2][2];
        lenSq = gViewDir2D[2] * gViewDir2D[2] + gViewDir2D[3] * gViewDir2D[3];
        if (lenSq == 0.0f) lenSq = 1.0f;
        inv = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(lenSq)));
        gViewDir2D[2] *= inv;
        gViewDir2D[3] *= inv;
    }

    gei_LightMatrix.SetIdentity();

    // Heading of the camera: angle of (m01, m11) of the view matrix, as a fraction of a turn.
    float s = gei_ViewMatrix->m[0][1];
    float c = gei_ViewMatrix->m[1][1];
    float scale = 1.0f;
    float sqLen = ppc_fmadds(s, s, c * c);
    if (sqLen == 0.0f) {
        c = 1.0f;
    } else {
        scale = 1.0f / static_cast<float>(std::sqrt(static_cast<double>(sqLen)));
    }
    float sn = s * scale;
    float cs = c * scale;
    float a = static_cast<float>(std::asin(-static_cast<double>(sn)));
    float angle = static_cast<float>(std::acos(static_cast<double>(cs)));
    if (!(a > 0.0f)) angle = -angle;
    gViewAngle = angle;
    gViewAngle = gViewAngle / 6.2831855f;
    return 1;
}

CTransformBase::~CTransformBase() {
}

float CTransformBase::TestMathSpeed() {
    float in[4] = {1.0f, 1.0f, 1.0f, 1.0f};   // copy of the constant at 0x10051c58
    CMatrix m;
    float out[4];
    int32_t start = gProfiler.GetClockCycleCount10h();
    for (int32_t i = 999; ; i--) {
        Transform(&m, out, in, 0, 0, 1, 0);
        Project(out, 0, 1, 0);
        if (i == 0) break;
    }
    int32_t end = gProfiler.GetClockCycleCount10h();
    return static_cast<float>((end - start) * 16) / 1000.0f;
}

void CMatrix::RotateX(float angle) {
    float c = static_cast<float>(std::cos(static_cast<double>(angle)));
    float s = static_cast<float>(std::sin(static_cast<double>(angle)));
    SetIdentity();
    m[1][1] = c;
    m[2][2] = c;
    m[1][2] = -s;
    m[2][1] = s;
}

void CMatrix::RotateY(float angle) {
    float c = static_cast<float>(std::cos(static_cast<double>(angle)));
    float s = static_cast<float>(std::sin(static_cast<double>(angle)));
    SetIdentity();
    m[0][0] = c;
    m[2][2] = c;
    m[0][2] = s;
    m[2][0] = -s;
}

void CMatrix::RotateZ(float angle) {
    float c = static_cast<float>(std::cos(static_cast<double>(angle)));
    float s = static_cast<float>(std::sin(static_cast<double>(angle)));
    SetIdentity();
    m[0][0] = c;
    m[1][1] = c;
    m[0][1] = -s;
    m[1][0] = s;
}

void lubksb(sMatrix& a, int32_t* indx, float* b) {
    int32_t ii = -1;
    for (int32_t i = 0; i < 4; i++) {
        int32_t ip = indx[i];
        float sum = b[ip];
        b[ip] = b[i];
        if (ii >= 0) {
            for (int32_t j = ii; j <= i - 1; j++) sum = ppc_fnmsubs(b[j], a.m[i][j], sum);
        } else if (sum != 0.0f) {
            ii = i;
        }
        b[i] = sum;
    }
    for (int32_t i = 3; i >= 0; i--) {
        float sum = b[i];
        for (int32_t j = i + 1; j < 4; j++) sum = ppc_fnmsubs(b[j], a.m[i][j], sum);
        b[i] = sum / a.m[i][i];
    }
}

void ludcmp(sMatrix& a, int32_t* indx, float* d) {
    float vv[4];
    // The original leaves imax uninitialised (it is whatever r0 held; from Inverse that is
    // the bit pattern of a[3][3]). It is only read before being set when every candidate
    // pivot of column 0 compares false (NaNs, or the all-zero matrix), where the original
    // indexes out of range unless that pattern is 0..3; 0 matches the zero matrix.
    int32_t imax = 0;
    *d = 1.0f;
    for (int32_t i = 0; i < 4; i++) {
        float big = 0.0f;
        for (int32_t j = 0; j < 4; j++) {
            float temp = std::fabs(a.m[i][j]);
            if (temp > big) big = temp;
        }
        vv[i] = 1.0f / big;
    }
    for (int32_t j = 0; j < 4; j++) {
        for (int32_t i = 0; i < j; i++) {
            float sum = a.m[i][j];
            for (int32_t k = 0; k < i; k++) sum = ppc_fnmsubs(a.m[i][k], a.m[k][j], sum);
            a.m[i][j] = sum;
        }
        float big = 0.0f;
        for (int32_t i = j; i < 4; i++) {
            float sum = a.m[i][j];
            for (int32_t k = 0; k < j; k++) sum = ppc_fnmsubs(a.m[i][k], a.m[k][j], sum);
            a.m[i][j] = sum;
            float dum = vv[i] * std::fabs(sum);
            if (dum >= big) {
                big = dum;
                imax = i;
            }
        }
        if (j != imax) {
            for (int32_t k = 0; k < 4; k++) {
                float dum = a.m[imax][k];
                a.m[imax][k] = a.m[j][k];
                a.m[j][k] = dum;
            }
            *d = -*d;
            vv[imax] = vv[j];
        }
        indx[j] = imax;
        if (a.m[j][j] == 0.0f) a.m[j][j] = kTiny;
        if (j != 3) {
            float dum = 1.0f / a.m[j][j];
            for (int32_t i = j + 1; i < 4; i++) a.m[i][j] *= dum;
        }
    }
}

void CMatrix::Inverse() {
    sMatrix lu = *this;
    int32_t indx[4];
    float d;
    ludcmp(lu, indx, &d);
    sMatrix inv;
    for (int32_t j = 0; j < 4; j++) {
        float col[4];
        col[0] = 0.0f;
        col[1] = 0.0f;
        col[2] = 0.0f;
        col[3] = 0.0f;
        col[j] = 1.0f;
        lubksb(lu, indx, col);
        inv.m[0][j] = col[0];
        inv.m[1][j] = col[1];
        inv.m[2][j] = col[2];
        inv.m[3][j] = col[3];
    }
    static_cast<sMatrix&>(*this) = inv;
}

void CMatrix::Load(sMatrix& src) {
    std::memcpy(static_cast<sMatrix*>(this), &src, sizeof(sMatrix));
}

void CMatrix::View(D3DVECTOR from, D3DVECTOR at, D3DVECTOR up, float roll) {
    SetIdentity();
    D3DVECTOR viewDir;
    viewDir.x = at.x - from.x;
    viewDir.y = at.y - from.y;
    viewDir.z = at.z - from.z;
    Normalise(viewDir);
    D3DVECTOR right = Cross(up, viewDir);
    D3DVECTOR newUp = Cross(viewDir, right);   // from the not yet normalised right vector
    Normalise(right);
    Normalise(newUp);
    m[0][0] = right.x;
    m[1][0] = right.y;
    m[2][0] = right.z;
    m[0][1] = newUp.x;
    m[1][1] = newUp.y;
    m[2][1] = newUp.z;
    m[0][2] = viewDir.x;
    m[1][2] = viewDir.y;
    m[2][2] = viewDir.z;
    m[3][0] = ppc_fneg(Dot(right, from));
    m[3][1] = ppc_fneg(Dot(newUp, from));
    m[3][2] = ppc_fneg(Dot(viewDir, from));
    if (roll != 0.0f) {
        CMatrix rotation;
        rotation.RotateZ(-roll);   // inlined in the original
        *this *= rotation;
    }
}

void CMatrix::Projection(float nearPlane, float farPlane, float fov) {
    double halfFov = 0.5 * static_cast<double>(kDegToRad * fov);
    float c = static_cast<float>(std::cos(halfFov));
    float s = static_cast<float>(std::sin(halfFov));
    float q = s / (1.0f - nearPlane / farPlane);
    std::memset(static_cast<sMatrix*>(this), 0, sizeof(sMatrix));
    m[0][0] = c;
    m[1][1] = c;
    m[2][2] = q;
    m[3][2] = -q * nearPlane;
    if (gei_Display.mFlags & CDisplay::kFlag200) {
        m[3][3] = 1.0f;
    } else {
        m[2][3] = s;
    }
    gProjectionNear = nearPlane;
    gProjectionFar = farPlane;
    gProjectionFOV = fov;
}

void CMatrix::AdjustProjection(float farPlane) {
    Projection(gProjectionNear, farPlane, gProjectionFOV);
    float z = gei_Display.mZ4C * ((farPlane - m[3][2]) / m[2][2]);
    if (z < gProjectionNear) z = gProjectionNear;
    gei_Display.ZSetup(farPlane, ppc_fmadds(z, m[2][2], m[3][2]), ppc_fmadds(z, m[2][3], m[3][3]));
}

void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    float* o = static_cast<float*>(out);
    const float* v = static_cast<const float*>(in);
    switch (type) {
    case 0: {
        float y = v[1], x = v[0], z = v[2];
        o[0] = m[3][0] + ppc_fmadds(z, m[2][0], ppc_fmadds(x, m[0][0], y * m[1][0]));
        o[1] = m[3][1] + ppc_fmadds(z, m[2][1], ppc_fmadds(x, m[0][1], y * m[1][1]));
        o[2] = m[3][2] + ppc_fmadds(z, m[2][2], ppc_fmadds(x, m[0][2], y * m[1][2]));
        o[3] = m[3][3] + ppc_fmadds(z, m[2][3], ppc_fmadds(x, m[0][3], y * m[1][3]));
        break;
    }
    case 1: {
        float y = v[1], x = v[0], z = v[2], w = v[3];
        o[0] = ppc_fmadds(w, m[3][0], ppc_fmadds(z, m[2][0], ppc_fmadds(x, m[0][0], y * m[1][0])));
        o[1] = ppc_fmadds(w, m[3][1], ppc_fmadds(z, m[2][1], ppc_fmadds(x, m[0][1], y * m[1][1])));
        o[2] = ppc_fmadds(w, m[3][2], ppc_fmadds(z, m[2][2], ppc_fmadds(x, m[0][2], y * m[1][2])));
        o[3] = ppc_fmadds(w, m[3][3], ppc_fmadds(z, m[2][3], ppc_fmadds(x, m[0][3], y * m[1][3])));
        break;
    }
    case 2: {
        float y = v[1], x = v[0], z = v[2];
        o[0] = m[3][0] + ppc_fmadds(z, m[2][0], ppc_fmadds(x, m[0][0], y * m[1][0]));
        o[1] = m[3][1] + ppc_fmadds(z, m[2][1], ppc_fmadds(x, m[0][1], y * m[1][1]));
        o[2] = m[3][2] + ppc_fmadds(z, m[2][2], ppc_fmadds(x, m[0][2], y * m[1][2]));
        break;
    }
    case 3: {
        float y = v[1], x = v[0], z = v[2];
        o[0] = ppc_fmadds(z, m[2][0], ppc_fmadds(x, m[0][0], y * m[1][0]));
        o[1] = ppc_fmadds(z, m[2][1], ppc_fmadds(x, m[0][1], y * m[1][1]));
        o[2] = ppc_fmadds(z, m[2][2], ppc_fmadds(x, m[0][2], y * m[1][2]));
        break;
    }
    case 4: {
        float y = v[1], x = v[0], z = v[2];
        o[0] = ppc_fmadds(z, m[0][2], ppc_fmadds(x, m[0][0], y * m[0][1]));
        o[1] = ppc_fmadds(z, m[1][2], ppc_fmadds(x, m[1][0], y * m[1][1]));
        o[2] = ppc_fmadds(z, m[2][2], ppc_fmadds(x, m[2][0], y * m[2][1]));
        break;
    }
    default:
        msl_assertion_failed("0", "tb_matrix.cpp", 601);
        break;
    }
}

void sMatrix::Byteswap() {
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&m[0][0]), 16);
}
