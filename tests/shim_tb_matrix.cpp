// extern "C" wrappers for tests/test_tb_matrix.py (ctypes): tb_matrix.cpp, tb_light.cpp,
// MatrixPowerPC.cpp and the pieces of tb_profiler.cpp / tb_resource_display.cpp that
// were translated with them.
#include <cstring>
#include <new>

#include "MatrixPowerPC.h"
#include "tb_light.h"
#include "tb_matrix.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"

namespace {
CMatrixPowerPC& Unit() {
    static CMatrixPowerPC unit;
    return unit;
}

struct Field {
    const char* name;
    void* addr;
};

const Field kFields[] = {
    {"gei_Display.mFlags", &gei_Display.mFlags},
    {"gei_Display.mF04", &gei_Display.mF04},
    {"gei_Display.mZ48", &gei_Display.mZ48},
    {"gei_Display.mZ4C", &gei_Display.mZ4C},
    {"gei_Display.mZ54", &gei_Display.mZ54},
    {"gei_Display.mZ58", &gei_Display.mZ58},
    {"gei_Display.mZ5C", &gei_Display.mZ5C},
    {"gei_Display.mZ60", &gei_Display.mZ60},
    {"gei_Display.mZ68", &gei_Display.mZ68},
    {"gei_Display.mZ6C", &gei_Display.mZ6C},
    {"gei_Display.mZ70", &gei_Display.mZ70},
    {"gei_Display.mZ74", &gei_Display.mZ74},
    {"gei_Display.mZ78", &gei_Display.mZ78},
    {"gei_Display.mZ7C", &gei_Display.mZ7C},
    {"gei_System.mFlags", &gei_System.mFlags},
    {"gei_System.mF20", &gei_System.mF20},
    {"gProfiler.mF1100", &gProfiler.mF1100},
    {"gProfiler.mF1104", &gProfiler.mF1104},
    {"gProfiler.mF1108", &gProfiler.mF1108},
    {"gProfiler.mF110C", &gProfiler.mF110C},
};
}  // namespace

extern "C" {
void* tpw_matrix_field(const char* name) {
    for (const Field& f : kFields)
        if (std::strcmp(f.name, name) == 0) return f.addr;
    return nullptr;
}

// ---- CMatrix and friends
void tpw_CMatrix_ctor(void* p) { new (p) CMatrix(); }
float tpw_CMatrix_Return(CMatrix* m, int32_t row, int32_t col) { return m->Return(row, col); }
void tpw_CMatrix_RotateX(CMatrix* m, float a) { m->RotateX(a); }
void tpw_CMatrix_RotateY(CMatrix* m, float a) { m->RotateY(a); }
void tpw_CMatrix_RotateZ(CMatrix* m, float a) { m->RotateZ(a); }
void tpw_lubksb(sMatrix* a, int32_t* indx, float* b) { lubksb(*a, indx, b); }
void tpw_ludcmp(sMatrix* a, int32_t* indx, float* d) { ludcmp(*a, indx, d); }
void tpw_CMatrix_Inverse(CMatrix* m) { m->Inverse(); }
void tpw_CMatrix_Load(CMatrix* m, sMatrix* src) { m->Load(*src); }
void tpw_CMatrix_View(CMatrix* m, const D3DVECTOR* from, const D3DVECTOR* at, const D3DVECTOR* up, float roll) {
    m->View(*from, *at, *up, roll);
}
void tpw_CMatrix_Projection(CMatrix* m, float n, float f, float fov) { m->Projection(n, f, fov); }
void tpw_CMatrix_AdjustProjection(CMatrix* m, float f) { m->AdjustProjection(f); }
void tpw_CMatrix_TransformVertex(CMatrix* m, void* out, void* in, int32_t type) { m->TransformVertex(out, in, type); }
void tpw_CMatrix_mul(CMatrix* out, const CMatrix* a, const CMatrix* b) { *out = *a * *b; }
void tpw_sMatrix_Byteswap(sMatrix* m) { m->Byteswap(); }
void tpw_CDisplay_ZSetup(float a, float b, float c) { gei_Display.ZSetup(a, b, c); }
int32_t tpw_GetClockCycleCount10h() { return gProfiler.GetClockCycleCount10h(); }

// ---- the math unit: gei_MathUnit points at Unit() unless a test replaced it
void tpw_matrix_use_unit() { gei_MathUnit = &Unit(); }
void tpw_mppc_Transform4(sMatrix* m, void* out, sVertexPosnSet* in, int32_t outStride, int32_t first, int32_t count) {
    Unit().Transform(m, out, in, outStride, first, count);
}
void tpw_mppc_Transform(sMatrix* m, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                        int32_t type) {
    Unit().Transform(m, out, in, outStride, inStride, count, type);
}
float tpw_mppc_Project(void* verts, int32_t stride, int32_t count, int32_t type) {
    return Unit().Project(verts, stride, count, type);
}
void tpw_mppc_Light() { Unit().Light(nullptr, nullptr, nullptr, 0); }
void tpw_mppc_TransformUV() { Unit().TransformUV(nullptr, nullptr, 0, 0, 0); }
void tpw_mppc_MultMatrix(const float* a, const float* b, float* out) { Unit().MultMatrix(a, b, out); }
float tpw_TestMathSpeed() { return Unit().TestMathSpeed(); }

// ---- the file-level functions
int32_t tpw_Matrix_Init() { return Matrix_Init(); }
void tpw_Matrix_Close() { Matrix_Close(); }
int32_t tpw_PreprocessMatrices() { return PreprocessMatrices(); }

// Pointer globals described as codes, so both sides compare: 0 null, 1 the expected
// target, 2 anything else. Order: gei_ViewMatrix, gei_InvViewMatrix, gei_FinalMatrix,
// gViewDir2DPtr, gei_MathUnit (1 = a live CMatrixPowerPC).
void tpw_matrix_pointer_codes(int32_t* out) {
    auto code = [](const void* p, const void* want) { return p == nullptr ? 0 : (p == want ? 1 : 2); };
    out[0] = code(gei_ViewMatrix, gei_InternalViewMatrix);
    out[1] = code(gei_InvViewMatrix, gei_InternalInvViewMatrix);
    out[2] = code(gei_FinalMatrix, gei_InternalFinalMatrix);
    out[3] = code(gViewDir2DPtr, gViewDir2D);
    out[4] = gei_MathUnit == nullptr ? 0 : (dynamic_cast<CMatrixPowerPC*>(gei_MathUnit) != nullptr ? 1 : 2);
}
void tpw_matrix_set_pointers() {
    gei_ViewMatrix = gei_InternalViewMatrix;
    gei_InvViewMatrix = gei_InternalInvViewMatrix;
    gei_FinalMatrix = gei_InternalFinalMatrix;
    gViewDir2DPtr = gViewDir2D;
}
void tpw_matrix_clear_pointers() {
    gei_ViewMatrix = nullptr;
    gei_InvViewMatrix = nullptr;
    gei_FinalMatrix = nullptr;
    gViewDir2DPtr = nullptr;
}
// Matrix_Init allocates a new unit each call; free it again (the test owns it).
void tpw_matrix_drop_unit() {
    if (gei_MathUnit != &Unit()) delete gei_MathUnit;
    gei_MathUnit = nullptr;
}
void tpw_matrix_new_unit() { gei_MathUnit = new CMatrixPowerPC; }
int32_t tpw_matrix_unit_is_null() { return gei_MathUnit == nullptr; }

// Constructor convention check: a CMatrixPowerPC dispatches each CTransformBase virtual
// to its own override. Bits: 1 MultMatrix, 2 Light, 4 TransformUV, 8 Project, 16 the
// 7-argument Transform, 32 the sVertexPosnSet Transform, 64 virtual destructor.
extern int32_t tpw_debugstr_count;
int32_t tpw_mppc_ctor_check() {
    int32_t bits = 0;
    alignas(CMatrixPowerPC) unsigned char storage[sizeof(CMatrixPowerPC)];
    CMatrixPowerPC* obj = new (storage) CMatrixPowerPC();
    CTransformBase* base = obj;
    CMatrix a, b, out;
    a.m[3][0] = 2.0f;
    b.m[3][1] = 3.0f;
    out.m[0][0] = 9.0f;
    base->MultMatrix(&a.m[0][0], &b.m[0][0], &out.m[0][0]);
    if (out.m[0][0] == 1.0f && out.m[3][0] == 2.0f && out.m[3][1] == 3.0f) bits |= 1;
    int32_t before = tpw_debugstr_count;
    base->Light(nullptr, nullptr, nullptr, 0);
    if (tpw_debugstr_count == before + 1) bits |= 2;
    base->TransformUV(nullptr, nullptr, 0, 0, 0);
    if (tpw_debugstr_count == before + 2) bits |= 4;
    float v[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    base->Project(v, 16, 1, 2);
    if (v[1] == -1.0f) bits |= 8;
    float in[4] = {1.0f, 2.0f, 3.0f, 1.0f}, o[4] = {0, 0, 0, 0};
    base->Transform(&a, o, in, 16, 16, 1, 2);
    if (o[0] == 3.0f && o[1] == 2.0f && o[2] == 3.0f) bits |= 16;
    sVertexPosnSet set = {{1.0f, 0, 0, 0}, {2.0f, 0, 0, 0}, {3.0f, 0, 0, 0}};
    float o4[4] = {0, 0, 0, 0};
    base->Transform(&b, o4, &set, 16, 0, 1);
    if (o4[0] == 1.0f && o4[1] == 5.0f && o4[2] == 3.0f && o4[3] == 1.0f) bits |= 32;
    base->~CTransformBase();
    bits |= 64;
    return bits;
}

// Static initialisers: construct the globals in place again.
void tpw_sinit_tb_matrix() {
    g_100b00fc = 1.0f;
    new (&gei_LightMatrix) CMatrix();
    new (&gei_ProjectionMatrix) CMatrix();
    for (CMatrix& m : gei_InternalViewMatrix) new (&m) CMatrix();
    for (CMatrix& m : gei_InternalInvViewMatrix) new (&m) CMatrix();
    for (CMatrix& m : gei_InternalFinalMatrix) new (&m) CMatrix();
    new (&gScreenMatrix) CMatrix();
}
void tpw_sinit_tb_light() { new (&gLightInverseWorld) CMatrix(); }
void tpw_sinit_tb_profiler() { new (&gProfiler) CProfiler(); }
}
