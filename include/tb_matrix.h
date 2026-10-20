// tb_matrix.h: engine matrices and the transform ("math unit") interface (engine library,
// original tb_matrix.cpp, functions at 0x1001a770..0x1001c9c8).
//
// Row-vector convention, as Direct3D: a point p transforms as p * M, so the translation
// lives in row 3 (m[3][0..2]) and MultMatrix(a, b) computes a * b.
#pragma once
#include <cstdint>
#include <cstring>

#include "platform/d3d_types.h"

struct SFVF_TLCTEX;
struct tag_sNode;
struct sFaceSet;
struct STMCoordinateSet;
struct sUVSet;

// Four vertex positions stored as a structure of arrays (0x30 bytes); vertex i of a set
// array lives in block i/4, lane i%4 (CMatrixPowerPC::Transform walks it that way).
struct sVertexPosnSet {
    float x[4];   // 0x00
    float y[4];   // 0x10
    float z[4];   // 0x20
};
static_assert(sizeof(sVertexPosnSet) == 0x30, "sVertexPosnSet layout");

// 4x4 float matrix, row-major (D3DMATRIX order: m[r][c] is _<r+1><c+1>).
struct sMatrix {
    float m[4][4];

    void Byteswap();   // 0x1004a57c (emitted at the end of the engine, file unknown)
};
static_assert(sizeof(sMatrix) == 0x40, "sMatrix layout");

class CMatrix : public sMatrix {
public:
    // Inline in the original: zero, then 1.0 on the diagonal. The out-of-line copy at
    // 0x1001c9f0 is what __construct_array calls for the global matrix arrays.
    CMatrix() { SetIdentity(); }

    // Inline helper (the original inlines this memset + diagonal in the constructor,
    // RotateX/Y/Z and View; its name, if it had one, is unknown).
    void SetIdentity() {
        std::memset(static_cast<sMatrix*>(this), 0, sizeof(sMatrix));
        m[3][3] = 1.0f;
        m[2][2] = 1.0f;
        m[1][1] = 1.0f;
        m[0][0] = 1.0f;
    }

    // Inline accessor; out-of-line copy at 0x10025d24.
    float Return(int32_t row, int32_t col) { return m[row][col]; }

    void RotateX(float angle);
    void RotateY(float angle);
    void RotateZ(float angle);
    void Inverse();
    void Load(sMatrix& src);
    void View(D3DVECTOR from, D3DVECTOR at, D3DVECTOR up, float roll);
    void Projection(float nearPlane, float farPlane, float fov);
    void AdjustProjection(float farPlane);
    // type 0: xyz -> xyzw with translation; 1: xyzw -> xyzw; 2: xyz -> xyz with translation;
    // 3: xyz -> xyz, rotation only; 4: xyz -> xyz through the transposed rotation.
    // Anything else asserts (tb_matrix.cpp line 601) and writes nothing.
    void TransformVertex(void* out, void* in, int32_t type);

    // Inline in the original; each keeps one static result matrix (constructed on first
    // use) and multiplies through the math unit. operator* has an out-of-line copy at
    // 0x10032528 (static at 0x100524a8); operator*= is always inlined (static at
    // 0x10052464).
    CMatrix operator*(const CMatrix& b) const;
    CMatrix& operator*=(const CMatrix& b);
};
static_assert(sizeof(CMatrix) == 0x40, "CMatrix layout");

// The transform unit interface (the PC build had one implementation per CPU; the Mac
// build has CMatrixPowerPC). CodeWarrior layout as a virtual base: +0 unknown word,
// +4 vtable pointer, +8 offset back to the complete object; gei_MathUnit points at it.
class CTransformBase {
public:
    virtual ~CTransformBase() = 0;
    // Transform positions from sVertexPosnSet blocks starting at vertex `first`.
    virtual void Transform(sMatrix* m, void* out, sVertexPosnSet* in, int32_t outStride, int32_t first,
                           int32_t count) = 0;
    // Transform strided vertices; `type` as CMatrix::TransformVertex (0..3).
    virtual void Transform(sMatrix* m, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                           int32_t type) = 0;
    // Perspective divide and depth mapping of transformed vertices in place; returns the
    // sum of the depths before the divide.
    virtual float Project(void* verts, int32_t stride, int32_t count, int32_t type) = 0;
    virtual void Light(SFVF_TLCTEX* verts, tag_sNode* node, sFaceSet* faces, int32_t count) = 0;
    virtual void TransformUV(STMCoordinateSet* out, sUVSet* in, int32_t a, int32_t b, int32_t c) = 0;
    // out = a * b (4x4, row-major); out may alias a or b.
    virtual void MultMatrix(const float* a, const float* b, float* out) = 0;

    float TestMathSpeed();
};

// Globals (engine data; the exported names are the originals).
extern CTransformBase* gei_MathUnit;           // 0x100afe94
extern float gViewAngle;                       // 0x100afe98  camera heading / (2 pi)
extern CMatrix gScreenMatrix;                  // 0x100afe9c  maps clip space to the screen
extern float gViewDir2D[4];                    // 0x100afedc  ground-plane view direction per viewport
extern float* gViewDir2DPtr;                   // 0x100afeec  = gViewDir2D after Matrix_Init
extern CMatrix gei_InternalFinalMatrix[2];     // 0x100afef0
extern CMatrix* gei_FinalMatrix;               // 0x100aff70
extern CMatrix gei_InternalInvViewMatrix[2];   // 0x100aff74
extern CMatrix* gei_InvViewMatrix;             // 0x100afff4
extern CMatrix gei_InternalViewMatrix[2];      // 0x100afff8
extern CMatrix* gei_ViewMatrix;                // 0x100b0078
extern CMatrix gei_ProjectionMatrix;           // 0x100b007c
extern CMatrix gei_LightMatrix;                // 0x100b00bc
extern float g_100b00fc;                       // 0x100b00fc  1.0; only the static initialiser touches it
extern float gProjectionFOV;                   // 0x100afe8c  degrees, last CMatrix::Projection
extern float gProjectionNear;                  // 0x100afe90
extern float gProjectionFar;                   // 0x10051c54  initialised data (1.0)
// (0, 1, 0) in initialised data just before gProjectionFar; used as the ground-plane
// normal by CShadow::AddLightSource and SSpriteGameData::CreateShadow. Name invented.
extern D3DVECTOR gUpVector;                    // 0x10051c48

// Turns the x and y of four vertices (0x10 bytes apart) in place about a centre by `angle`,
// a fraction of a turn looked up in the engine's 256-entry sine table. A null centre means
// the middle of the quad: x halfway between vertices 0 and 1, y halfway between vertices 0
// and 2. Used by the sky's added sprites and the particles, and by the sprites' quads
// (SSpriteGameData::SetupVertices). A given centre is used as it is (a copy of the first
// vertex when SetupVertices makes the second turn). (0x10038cd0, src/tb_rotatevertex.cpp)
struct SVertex;
void RotateVertex_Y(SVertex* vertices, float angle, SVertex* centre);
// The same about the middle of the quad in x and z (x halfway between vertices 0 and 1, z
// halfway between 0 and 2), `angle` in radians through the C library's cosine and sine (not
// the table): the turn of a sprite lying on the ground. (0x10038e9c, src/tb_rotatevertex.cpp)
void RotateVertex_Z(SVertex* vertices, float angle);

int32_t Matrix_Init();
void Matrix_Close();
int32_t PreprocessMatrices();

// Numerical Recipes LU decomposition and back substitution for n = 4, 0-based.
void ludcmp(sMatrix& a, int32_t* indx, float* d);
void lubksb(sMatrix& a, int32_t* indx, float* b);

inline CMatrix CMatrix::operator*(const CMatrix& b) const {
    static CMatrix result;
    gei_MathUnit->MultMatrix(&m[0][0], &b.m[0][0], &result.m[0][0]);
    return result;
}

inline CMatrix& CMatrix::operator*=(const CMatrix& b) {
    static CMatrix result;
    gei_MathUnit->MultMatrix(&m[0][0], &b.m[0][0], &result.m[0][0]);
    *this = result;
    return *this;
}
