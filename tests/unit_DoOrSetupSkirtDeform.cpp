// Test library for DoOrSetupSkirtDeform (unit v5221; libtpw_unit_DoOrSetupSkirtDeform.dylib, tests/test_DoOrSetupSkirtDeform.py), main app:
//   src/game/park/SkirtDeform.cpp   DoOrSetupSkirtDeform
//
// The callees outside the unit are fakes that record the matrices and points they read and answer from the test's queues:
// CombineMatricesNoWDest2nd (records both matrices, changes nothing), InvertMatrix (records the source, copies it to the destination),
// VectorThroughLMatrixNoW (records the matrix and the point, replaces the point by the queued x, y, z). modf and the float to unsigned
// conversion are real on the host side. The globals of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"
#include "tb_matrix.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
uint32_t gSkirtCellFlags[20][20];
uint32_t gSkirtRows;
uint32_t gSkirtColumns;

namespace {

int64_t B(float value) { return static_cast<int64_t>(fbits(value)); }

float FromBits(int64_t bits) {
    uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

void LogMatrix(const char* low, const char* high, const float* m) {
    ev(low, {B(m[0]), B(m[1]), B(m[2]), B(m[3]), B(m[4]), B(m[5]), B(m[6]), B(m[7]), B(m[8]), B(m[9]), B(m[10]), B(m[11])});
    ev(high, {B(m[12]), B(m[13]), B(m[14]), B(m[15])});
}

}  // namespace

// ---- the fakes
void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination) {
    LogMatrix("CombineSrc0", "CombineSrc3", reinterpret_cast<const float*>(source));
    LogMatrix("CombineDst0", "CombineDst3", &destination->m[0][0]);
}

void InvertMatrix(sMatrix* destination, const sMatrix* source) {
    LogMatrix("InvertSrc0", "InvertSrc3", &source->m[0][0]);
    std::memcpy(destination, source, sizeof(sMatrix));
}

void VectorThroughLMatrixNoW(sMatrix* matrix, s3DPoint* point) {
    LogMatrix("VecM0", "VecM3", &matrix->m[0][0]);
    ev("VecP", {B(point->x), B(point->y), B(point->z)});
    point->x = FromBits(ret("vx"));
    point->y = FromBits(ret("vy"));
    point->z = FromBits(ret("vz"));
}

extern "C" {

uint64_t tpw_skirt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"table", &gSkirtCellFlags[0][0]},
        {"rows", &gSkirtRows},
        {"cols", &gSkirtColumns},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_skirt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word of a block address (an offset from the arena base).
uint32_t tpw_skirt_offset(uint64_t address) {
    return address != 0 ? static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address)))) : 0u;
}

int32_t tpw_skirt_layout(int64_t* out) {
    alignas(16) static char nodeBuf[sizeof(tag_sNode)];
    alignas(16) static char fieldBuf[sizeof(sHField)];
    tag_sNode* node = reinterpret_cast<tag_sNode*>(nodeBuf);
    sHField* field = reinterpret_cast<sHField*>(fieldBuf);
    auto off = [](const void* member, const char* base) { return static_cast<int64_t>(reinterpret_cast<const char*>(member) - base); };
    int32_t n = 0;
    out[n++] = off(&node->mFlags, nodeBuf);
    out[n++] = off(&node->mF08, nodeBuf);
    out[n++] = off(&node->mF0C, nodeBuf);
    out[n++] = off(&node->mMatrix, nodeBuf);
    out[n++] = off(&node->mVertexCount, nodeBuf);
    out[n++] = off(&node->mVertices, nodeBuf);
    out[n++] = off(&node->mBox, nodeBuf);
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = off(&field->mF08, fieldBuf);
    out[n++] = off(&field->mF18, fieldBuf);
    out[n++] = off(&field->mHeights, fieldBuf);
    out[n++] = off(&field->mQuads, fieldBuf);
    out[n++] = static_cast<int64_t>(sizeof(sMatrix));
    return n;
}

void tpw_skirt_run(uint64_t a, uint64_t b, uint64_t list, uint64_t matrix, uint64_t field, int32_t setup, uint64_t count, float lift,
                   uint64_t bestNode, uint64_t bestCount, uint32_t flags, uint64_t only, int32_t mode) {
    sMatrix m;
    std::memcpy(&m, reinterpret_cast<const void*>(static_cast<uintptr_t>(matrix)), sizeof(sMatrix));
    DoOrSetupSkirtDeform(reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(a)), reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(b)),
                         reinterpret_cast<uint16_t**>(static_cast<uintptr_t>(list)), m,
                         reinterpret_cast<sHField*>(static_cast<uintptr_t>(field)), setup,
                         reinterpret_cast<int32_t*>(static_cast<uintptr_t>(count)), lift,
                         reinterpret_cast<tag_sNode**>(static_cast<uintptr_t>(bestNode)),
                         reinterpret_cast<int32_t*>(static_cast<uintptr_t>(bestCount)), flags,
                         reinterpret_cast<tag_sNode*>(static_cast<uintptr_t>(only)), mode);
}

}  // extern "C"
