// Lightning subdivision and quads. The complete rain layout is built and dumped after every call.
// sqrt and the three functions are real. Random, projection and polygon submission are recording fakes.
#include <cstdint>
#include <cstring>
#include <new>
#define TPW_CALLLOG_EVENTS 16384
#include "calllog.h"
#include "tb_rain.h"
#include "tb_matrix.h"
#include "tb_drawprimitive.h"

#define TPW_CALLLOG_QUEUE_VALUES 4096
#include "calllog.cpp"

using namespace calllog;

namespace {
constexpr int32_t kRainBytes = 0xa1cc;
alignas(CRain) unsigned char gRainStorage[sizeof(CRain)];
CMatrix gMatrix;
alignas(CDrawPrimitive) unsigned char gDrawStorage[sizeof(CDrawPrimitive)];
int32_t gRandomCalls;
int32_t gNewCalls;
void* gNewFunction;

uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) ? 0x7fc00000u : word;
}
float FromBits(uint32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}
CRain* Rain() { return reinterpret_cast<CRain*>(gRainStorage); }
unsigned char* RainWord(int32_t offset) {
    return gRainStorage + (offset < 0x3354 ? offset : offset + 12);
}
bool FloatWord(int32_t offset) {
    return (offset >= 0x9de4 && offset < 0xa164) ||
           (offset >= 0x3008 && offset < 0x3308 && (offset - 0x3008) % 0x30 < 0x24);
}

// An exact affine projection. Input words and every argument are logged separately.
void ProjectPoint(float* out, const float* in) {
    out[0] = in[0] * 2.0f + 1.0f;
    out[1] = in[1] * -0.5f + 2.0f;
    out[2] = in[2] + 3.0f;
    out[3] = 1.0f;
}
void RecordPoint(const float* in) {
    ev("Input", {Canon(fbits(in[0])), Canon(fbits(in[1])), Canon(fbits(in[2]))});
}
class FakeMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix* matrix, void* out, void* in, int32_t outStride, int32_t inStride,
                   int32_t count, int32_t type) override {
        ev("Transform", {lab(this), lab(matrix), out == in, outStride, inStride, count, type});
        for (int32_t i = 0; i < count; ++i) {
            const float* p = reinterpret_cast<const float*>(static_cast<uint8_t*>(in) + i * inStride);
            RecordPoint(p);
            ProjectPoint(reinterpret_cast<float*>(static_cast<uint8_t*>(out) + i * outStride), p);
        }
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMath gMath;
}  // namespace

// Test-only function instrumentation counts real recursion without replacing any call site.
extern "C" __attribute__((no_instrument_function)) void __cyg_profile_func_enter(void* function, void*) {
    if (function == gNewFunction) ++gNewCalls;
}
extern "C" __attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*, void*) {}

CTransformBase::~CTransformBase() {}
CTransformBase* gei_MathUnit = &gMath;
CMatrix* gei_FinalMatrix = &gMatrix;
CDrawPrimitive* gei_DrawPrimitive = reinterpret_cast<CDrawPrimitive*>(gDrawStorage);

// The real TransformVertex performs matrix arithmetic directly; this unit fakes it with the same
// affine map as FakeMath, rather than pulling the whole matrix translation into the library.
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    ev("TransformVertex", {lab(this), out == in, type});
    RecordPoint(static_cast<const float*>(in));
    ProjectPoint(static_cast<float*>(out), static_cast<const float*>(in));
}
uint32_t GetCycleCount() { return 0; }  // Link-only callee of w64's unused SLightning::Init.
int32_t Random() {
    const uint32_t value = static_cast<uint32_t>(ret("Random", 511));
    ++gRandomCalls;
    ev("Random", {value});
    return static_cast<int32_t>(value);
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}
int32_t CDrawPrimitive::AddPoly_TLCTEX(SFVF_TLCTEX* vertices, int32_t count, int32_t stride,
                                     ClipCodeType* clip, int32_t flags) {
    ev("AddPoly", {lab(this), count, stride, lab(clip), flags});
    for (int32_t i = 0; i < count; ++i) {
        uint32_t w[8];
        std::memcpy(w, reinterpret_cast<uint8_t*>(vertices) + i * stride, 32);
        ev("Vertex", {Canon(w[0]), Canon(w[1]), Canon(w[2]), Canon(w[3]), w[4], w[5], Canon(w[6]), Canon(w[7])});
    }
    return static_cast<int32_t>(ret("AddPoly", 1));
}

extern "C" int32_t tpw_rb_run(const int64_t* description, uint32_t* out) {
    int32_t at = 0;
    const auto next = [&]() { return static_cast<uint32_t>(description[at++]); };
    const int32_t op = next();
    std::memset(gRainStorage, 0, sizeof gRainStorage);
    for (int32_t offset = 0; offset < kRainBytes; offset += 4) {
        const uint32_t word = next();
        if (offset == 0x3348 || offset == 0x334c || offset == 0x3350) continue;
        std::memcpy(RainWord(offset), &word, 4);
    }
    gRandomCalls = 0;
    gNewCalls = 0;
    const auto member = &CRain::NewRenderLightning;
    std::memcpy(&gNewFunction, &member, sizeof gNewFunction);
    tpw_cl_label(1, Rain());
    tpw_cl_label(2, gei_MathUnit);
    tpw_cl_label(3, gei_DrawPrimitive);
    tpw_cl_label(4, gei_FinalMatrix);
    int32_t nout = 0;
    const int32_t count = next();
    for (int32_t step = 0; step < count; ++step) {
        const bool fromNull = next() != 0;
        const bool toNull = next() != 0;
        D3DVECTOR a = {FromBits(next()), FromBits(next()), FromBits(next())};
        D3DVECTOR b = {FromBits(next()), FromBits(next()), FromBits(next())};
        const float scale = FromBits(next());
        D3DVECTOR point = {FromBits(0xa5a5a5a5), FromBits(0xa5a5a5a5), FromBits(0xa5a5a5a5)};
        int32_t result = 0;
        if (op == 1) GeneratePoint(&point, &a, &b, scale);
        else if (op == 2) result = Rain()->AddLightningSection(fromNull ? nullptr : &a, toNull ? nullptr : &b);
        else result = Rain()->NewRenderLightning(&a, &b);
        out[nout++] = static_cast<uint32_t>(result);
        out[nout++] = Canon(fbits(point.x));
        out[nout++] = Canon(fbits(point.y));
        out[nout++] = Canon(fbits(point.z));
        out[nout++] = static_cast<uint32_t>(gRandomCalls);
        out[nout++] = static_cast<uint32_t>(gNewCalls);
        for (int32_t offset = 0; offset < kRainBytes; offset += 4) {
            uint32_t word = 0;
            if (offset != 0x3348 && offset != 0x334c && offset != 0x3350) std::memcpy(&word, RainWord(offset), 4);
            out[nout++] = FloatWord(offset) ? Canon(word) : word;
        }
    }
    return nout;
}
