// Test library for the flags of the park (unit w128; libtpw_unit_DynamicFlags.dylib, tests/test_DynamicFlags.py), main app:
//   src/DynamicFlags.cpp   DC_AnimateFlag, DC_DrawFlag (and gDynamicFlagClamp)
//
// The only callee outside the file is DC_AddPolyFaceToList, a fake that records the arguments the real one reads: the texture and the type,
// the twelve words of the positions, the u and v of the four texture coordinates (the z words are never written by DC_DrawFlag and the real
// function does not read them) and the twelve words of the points, NaNs made one value. The two grids (defined by src/DynamicFaces.cpp in the
// program) and the engine's sine table are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "DynamicMesh.h"
#include "TPGlobals.h"
#include "calllog.h"
#include "m3d2.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read: Format (the only member src/TbString.cpp leaves to the test) does nothing
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files, defined here
s3DPoint gDynamicGridNormals[16][19];
s3DPoint gDynamicGridPositions[16][19];
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
static_assert(sizeof(TPGlobals::mSinTable) == 0x4000, "the table's size");

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// One event of the twelve words (the unused ones 0).
void RecordFloats(const char* name, const float* words, int32_t count) {
    int64_t ints[12] = {};
    for (int32_t i = 0; i < count; ++i) ints[i] = CanonBits(words[i]);
    ev(name, {ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], ints[8], ints[9], ints[10], ints[11]});
}

}  // namespace

// ---- the fake
void DC_AddPolyFaceToList(s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points, DYN_Type type) {
    ev("DC_AddPolyFaceToList", {texture, static_cast<int64_t>(type)});
    float words[12];
    for (int32_t k = 0; k < 4; ++k) {
        words[k * 3] = positions[k].x;
        words[k * 3 + 1] = positions[k].y;
        words[k * 3 + 2] = positions[k].z;
    }
    RecordFloats("positions", words, 12);
    for (int32_t k = 0; k < 4; ++k) {
        words[k * 2] = uvs[k].x;
        words[k * 2 + 1] = uvs[k].y;
    }
    RecordFloats("uvs", words, 8);
    for (int32_t k = 0; k < 4; ++k) {
        words[k * 3] = points[k].x;
        words[k * 3 + 1] = points[k].y;
        words[k * 3 + 2] = points[k].z;
    }
    RecordFloats("points", words, 12);
}

extern "C" {

uint64_t tpw_dfl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"tpg", &gTPGlobals}, {"gridA", gDynamicGridNormals}, {"gridB", gDynamicGridPositions}, {"clamp", &gDynamicFlagClamp},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module needs: gTPGlobals' mask, scale and table (the header's 0x4, 0x8, 0xc), the table's length, a grid's size.
int32_t tpw_dfl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = 4;
    out[n++] = 8;
    out[n++] = 0xc;
    out[n++] = static_cast<int64_t>(sizeof(gTPGlobals.mSinTable) / sizeof(gTPGlobals.mSinTable[0]));
    out[n++] = static_cast<int64_t>(sizeof(gDynamicGridPositions));
    out[n++] = static_cast<int64_t>(sizeof(gDynamicGridNormals));
    return n;
}

// ---- the functions
void tpw_dfl_animate(int32_t columns, int32_t rows, float step, float phase) { DC_AnimateFlag(columns, rows, step, phase); }

void tpw_dfl_draw(float x, float y, float z, float w, int32_t columns, int32_t rows, int32_t texture, float angle) {
    DC_DrawFlag(x, y, z, w, columns, rows, texture, angle);
}

}  // extern "C"
