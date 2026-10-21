// Fakes and entry points for the clipper (library unit_Clipping: src/engine/Clipping.cpp).
//
// Everything the unit calls is a fake that records its arguments through tests/calllog.h: the
// six edge clippers (VertexClip_*), the math unit's strided Transform and the assertion. The
// edge clippers record the places of their three vertices and the first eight words of the two
// they read, and write the next words of the case's edge queue over the vertex they make; the
// math unit records the matrix, the box corners it is given and writes the queue "Transformed"
// (eight vertices of four floats). The assertion records and then leaves the call, as the
// original does in the test (the code after it would call through a null pointer).
//
// The test owns four areas of words: 1 is the polygon's input buffer, 2 its output buffer, 3 the
// vertex area of the list overload and 4 the area of its lists and index fans. A pointer into one
// is recorded as a place: the area's number in the high half and the byte offset in the low half,
// so the PowerPC side (which has its own addresses) records the same numbers.
// tests/test_Clipping.py builds the same memory on the PowerPC side and compares the call logs,
// the results and the areas.
#include <csetjmp>
#include <cstring>

#include "calllog.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "tb_clipping.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"

using namespace calllog;

namespace {

enum Region : int32_t { kIn = 1, kOut = 2, kVertices = 3, kIndices = 4, kRegions = 5 };
constexpr int32_t kAreaWords = 2048;

alignas(16) uint32_t gArea[kRegions][kAreaWords];
uint32_t gEdgeQueue[4096];   // the words the edge clippers write (the call log's queues hold 64 values)
int32_t gEdgeCount;
int32_t gEdgeNext;
CClipping gClipping;
std::jmp_buf gLeave;

// Region number and byte offset of a pointer into one of the areas, 0 for NULL, -1 for any other.
int64_t Place(const void* pointer) {
    if (pointer == nullptr) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    for (int32_t region = 1; region < kRegions; ++region) {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(gArea[region]);
        if (p >= base && p < base + sizeof gArea[region]) return (static_cast<int64_t>(region) << 32) + (p - base);
    }
    return -1;
}

// The next word of the edge queue; the last one repeats, an empty queue gives 0 (as the call log's).
uint32_t NextEdgeWord() {
    if (gEdgeCount == 0) return 0;
    return gEdgeQueue[gEdgeNext < gEdgeCount ? gEdgeNext++ : gEdgeCount - 1];
}

// The first eight words of a vertex (fewer when the stride is shorter, -1 for the rest).
void WordsOf(const char* name, const void* vertex, int32_t stride) {
    int64_t w[8];
    uint32_t raw[8];
    const int32_t n = stride / 4 < 8 ? stride / 4 : 8;
    std::memcpy(raw, vertex, static_cast<size_t>(n) * 4);
    for (int32_t i = 0; i < 8; ++i) w[i] = i < n ? static_cast<int64_t>(raw[i]) : -1;
    ev(name, {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
}

// An edge clipper: records, then writes the words of the queue over the vertex it makes.
void Edge(int32_t which, SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    ev("Edge", {which, Place(out), Place(outside), Place(inside), stride});
    WordsOf("EdgeOutside", outside, stride);
    WordsOf("EdgeInside", inside, stride);
    uint32_t* words = reinterpret_cast<uint32_t*>(out);
    for (int32_t i = 0; i < stride / 4; ++i) words[i] = NextEdgeWord();
}

// A NaN (as float bits) as one value: its sign and payload depend on the processor.
uint32_t Canonical(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}

// The math unit: records the matrix and the corners it is given and answers with the queue.
class FakeMathUnit : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix* matrix, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                   int32_t type) override {
        ev("Transform", {lab(matrix), out != nullptr, in != nullptr, outStride, inStride, count, type});
        uint32_t m[16];
        std::memcpy(m, matrix, sizeof m);
        ev("Matrix", {m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]});
        ev("Matrix", {m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]});
        uint32_t c[24];
        std::memcpy(c, in, sizeof c);
        for (uint32_t& word : c) word = Canonical(word);
        ev("Corners", {c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
        ev("Corners", {c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15]});
        ev("Corners", {c[16], c[17], c[18], c[19], c[20], c[21], c[22], c[23]});
        uint32_t* words = static_cast<uint32_t*>(out);
        for (int32_t i = 0; i < 32; ++i) words[i] = static_cast<uint32_t>(ret("Transformed"));
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMathUnit gMathUnit;

}  // namespace

CTransformBase::~CTransformBase() {}
CTransformBase* gei_MathUnit = &gMathUnit;
CClipping* gei_Clipping = &gClipping;

void VertexClip_Bottom(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    Edge(0, out, outside, inside, stride);
}
void VertexClip_Top(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    Edge(1, out, outside, inside, stride);
}
void VertexClip_Right(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    Edge(2, out, outside, inside, stride);
}
void VertexClip_Left(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    Edge(3, out, outside, inside, stride);
}
void VertexClip_Back(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    Edge(4, out, outside, inside, stride);
}
void VertexClip_Front(SFVF_TLCTEX* out, SFVF_TLCTEX* outside, SFVF_TLCTEX* inside, int32_t stride) {
    Edge(5, out, outside, inside, stride);
}

// Records the assertion and leaves the call that made it.
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
    std::longjmp(gLeave, 1);
}

extern "C" {

// Fills the four areas with a pattern (the original's buffers hold whatever they held) and
// empties the edge queue.
void tpw_cp_reset() {
    for (int32_t region = 1; region < kRegions; ++region) {
        for (int32_t i = 0; i < kAreaWords; ++i) gArea[region][i] = 0xa5a5a5a5u;
    }
    gEdgeCount = 0;
    gEdgeNext = 0;
}

// The words the edge clippers write, one vertex's worth per call, in order.
void tpw_cp_edge_queue(const uint32_t* words, int32_t count) {
    gEdgeCount = count < 4096 ? count : 4096;
    gEdgeNext = 0;
    std::memcpy(gEdgeQueue, words, static_cast<size_t>(gEdgeCount) * 4);
}

void tpw_cp_load(int32_t region, const void* data, int32_t bytes) {
    std::memcpy(gArea[region], data, static_cast<size_t>(bytes));
}

void tpw_cp_store(int32_t region, void* data, int32_t bytes) {
    std::memcpy(data, gArea[region], static_cast<size_t>(bytes));
}

// CPP_TestData over the caller's vertices; the buffers are the caller's.
void tpw_cp_testdata(uint8_t* result, uint8_t* codes, const uint32_t* vertices, int32_t count, int32_t stride,
                     uint32_t mask) {
    CClipping::CPP_TestData(result, &gClipping, codes, vertices, count, stride, mask);
}

// ClipPoly(planes, out, in, count, stride): the polygon is in area 1, the output goes to area 2.
int32_t tpw_cp_clip1(int32_t planes, int32_t outOffset, int32_t inOffset, int32_t* count, int32_t stride) {
    SFVF_TLCTEX* out = reinterpret_cast<SFVF_TLCTEX*>(reinterpret_cast<uint8_t*>(gArea[kOut]) + outOffset);
    SFVF_TLCTEX* in = reinterpret_cast<SFVF_TLCTEX*>(reinterpret_cast<uint8_t*>(gArea[kIn]) + inOffset);
    return gClipping.ClipPoly(planes, out, in, *count, stride);
}

// ClipPoly(planes, vertices, list, count, stride): the vertices are in area 3, the list's numbers
// in area 4 at listOffset; the list's count goes in and out through listCount.
void tpw_cp_clip2(int32_t planes, int32_t vertexOffset, int32_t listOffset, int32_t* listCount, int32_t* count,
                  int32_t stride) {
    SFVF_TLCTEX* vertices =
        reinterpret_cast<SFVF_TLCTEX*>(reinterpret_cast<uint8_t*>(gArea[kVertices]) + vertexOffset);
    VertexClipList list;
    list.mCount = *listCount;
    list.mList = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(gArea[kIndices]) + listOffset);
    gClipping.ClipPoly(planes, vertices, &list, *count, stride);
    *listCount = list.mCount;
}

// ClipPoly(planes, indices, vertices, count, stride): the triangle's numbers are the three
// halfwords before indexOffset in area 4. Returns the place of the result.
int64_t tpw_cp_clip3(int32_t planes, int32_t indexOffset, int32_t vertexOffset, int32_t* count, int32_t stride) {
    uint16_t* indices = reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(gArea[kIndices]) + indexOffset);
    SFVF_TLCTEX* vertices =
        reinterpret_cast<SFVF_TLCTEX*>(reinterpret_cast<uint8_t*>(gArea[kVertices]) + vertexOffset);
    return Place(gClipping.ClipPoly(planes, indices, vertices, *count, stride));
}

// CheckBoundingBox on a box (six floats), a matrix (16 words) and the object's two words, with
// or without a math unit. Returns the box result, or 0x100 when the assertion left the call.
uint32_t tpw_cp_box(const uint32_t* box, const uint32_t* matrix, uint32_t* object, int32_t mathUnit) {
    sAlignedBox aligned;
    std::memcpy(&aligned, box, sizeof aligned);
    CMatrix m;
    std::memcpy(static_cast<sMatrix*>(&m), matrix, sizeof(sMatrix));
    std::memcpy(&gClipping, object, sizeof gClipping);
    tpw_cl_label(5, &m);
    gei_MathUnit = mathUnit ? &gMathUnit : nullptr;
    uint32_t result = 0x100;
    if (setjmp(gLeave) == 0) result = gClipping.CheckBoundingBox(aligned, m);
    gei_MathUnit = &gMathUnit;
    std::memcpy(object, &gClipping, sizeof gClipping);
    return result;
}

}  // extern "C"
