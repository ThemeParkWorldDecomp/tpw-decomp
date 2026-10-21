// Test library for CalculateSkirtVertices (unit v5909; libtpw_unit_CalculateSkirtVertices.dylib, tests/test_CalculateSkirtVertices.py), main app:
//   src/game/park/SkirtDeform.cpp   CalculateSkirtVertices
//
// The callees outside the unit (DecideRideShapeDeformEdges, SetMatrixIdentity, DoOrSetupSkirtDeform, MReleaseAllocF, CalcSkirtTextureDirnFlags,
// ERRORBox) are fakes that record what the real ones read and answer from the test's queues. The skirt globals of Debug.cpp are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "tb_mesh.h"
#include "rsse.h"

using namespace calllog;

uint32_t gSkirtCellFlags[20][20];
uint32_t gSkirtRows;
uint32_t gSkirtColumns;

namespace {

uint64_t gMarkSet, gMarkShape, gMarkNodes, gMarkHField;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// A pointer as a small number the PowerPC side can compute too.
int64_t Tag(const void* p) {
    uint64_t v = reinterpret_cast<uintptr_t>(p);
    if (v == 0) return 0;
    if (v == gMarkSet) return 1;
    if (v == gMarkShape) return 2;
    if (v == gMarkHField) return 3;
    if (v >= gMarkNodes && v < gMarkNodes + 0x1000) return 16 + static_cast<int64_t>(v - gMarkNodes);
    return 4;
}

}  // namespace

void DecideRideShapeDeformEdges(RideShape* shape, int32_t width, int32_t height) {
    ev("Decide", {Tag(shape), width, height});
    gSkirtCellFlags[1][2] = static_cast<uint32_t>(width * 31 + height);
    gSkirtRows = static_cast<uint32_t>(height);
    gSkirtColumns = static_cast<uint32_t>(width);
}

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {matrix != nullptr});
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) matrix->m[i][j] = static_cast<float>(i * 4 + j + 1);
}

void DoOrSetupSkirtDeform(tag_sNode* a, tag_sNode* b, uint16_t** list, sMatrix matrix, sHField* hf, int32_t setup, int32_t* count,
                          float lift, tag_sNode** bestNode, int32_t* bestCount, uint32_t flags, tag_sNode* only, int32_t mode) {
    int64_t ptrs = (list != nullptr && *list != nullptr ? 1 : 0) | (count != nullptr ? 2 : 0) | (bestNode != nullptr ? 4 : 0) |
                   (bestCount != nullptr ? 8 : 0);
    ev("Do", {Tag(a), Tag(b), Tag(hf), setup, static_cast<int64_t>(flags), Tag(only), mode, CanonBits(lift), Tag(*bestNode), *bestCount,
              ptrs});
    int64_t words[16];
    for (int i = 0; i < 16; ++i) words[i] = fbits(matrix.m[i / 4][i % 4]);
    ev("DoMatrix", {words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7], words[8], words[9], words[10],
                    words[11]});
    ev("DoMatrix2", {words[12], words[13], words[14], words[15]});
    int32_t n = static_cast<int32_t>(ret("count"));
    *count = n;
    for (int32_t i = 0; i < n && i < 8; ++i) (*list)[i] = static_cast<uint16_t>((n + 1) * 257 + i * 3);
    if (only == nullptr) {
        int32_t sel = static_cast<int32_t>(ret("best"));
        *bestNode = sel == 99 ? nullptr : a + sel;
    }
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    if (flags == 1 && ret("alloc") != 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}

void CalcSkirtTextureDirnFlags(tag_sRideAnimSet* set, RideShape* shape, int32_t width, int32_t height, int32_t force) {
    ev("CalcSkirt", {Tag(set), Tag(shape), width, height, force});
}

void ERRORBox(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    va_end(args);
    ev("ERRORBox", {}, {format, name});
}

extern "C" {

uint64_t tpw_csv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gSkirtCellFlags[0][0]},
        {"rows", &gSkirtRows},
        {"cols", &gSkirtColumns},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_csv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_csv_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_csv_mark(uint64_t set, uint64_t shape, uint64_t nodes, uint64_t hfield) {
    gMarkSet = set;
    gMarkShape = shape;
    gMarkNodes = nodes;
    gMarkHField = hfield;
}

int32_t tpw_csv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mName));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mHField));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sNode, mF98));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mBlock14));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mRootNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    return n;
}

void tpw_csv_call(uint64_t set, uint64_t header, uint64_t a, int32_t w, int32_t h) {
    CalculateSkirtVertices(reinterpret_cast<tag_sRideAnimSet*>(set), reinterpret_cast<tag_sM3D2Header*>(header),
                           reinterpret_cast<void*>(a), w, h);
}

}  // extern "C"
