// Test library for the dynamic face list and the water (unit w127; libtpw_unit_DynamicFaces.dylib, tests/test_DynamicFaces.py), main app:
//   src/DynamicFaces.cpp   sort, DC_ResetSquareFaceListPtr, DC_AddSquareFaceToList, DC_AddPolyFaceToList, HDZ_RotatePointZ, DC_AnimateWater,
//                          DC_DrawWater (and the two 16 x 19 grids it defines)
//
// The only callee outside the file is CTRACE (the two "Too many ... dynamic faces !" messages), a fake that records the level and the
// text. DC_DrawWater calls the real DC_AddPolyFaceToList, so the module compares the face list it leaves. The list arrays and the
// scalars the unit reads (their owners are not in the library) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Debug.h"
#include "DynamicMesh.h"
#include "TPGlobals.h"
#include "analyser.h"
#include "calllog.h"
#include "m3d2.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read: Format (the only member src/TbString.cpp leaves to the test) does nothing
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files, defined here
int32_t gDynamicCellF9BD4;
uint32_t gDynamicCellRandom[110];
int32_t gDynamicCellFBDBC;
uint8_t gDynamicCellFlag = 1;
sDynamicFace gDynamicFaces[kDynamicFaceMax];
uint32_t gDynamicFaceKeys[kDynamicFaceMax];
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
static_assert(sizeof(TPGlobals::mSinTable) == 0x4000, "the table's size");

namespace {
std::vector<void*> gBlocks;
}

// ---- the fake
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

extern "C" {

// ---- memory for the module
void tpw_df_reset() {
    for (void* block : gBlocks) std::free(block);
    gBlocks.clear();
}

// A block of `size` bytes filled with `fill`.
uint64_t tpw_df_alloc(uint64_t size, uint32_t fill) {
    void* block = std::malloc(size != 0 ? size : 1);
    gBlocks.push_back(block);
    std::memset(block, static_cast<int>(fill), size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

uint64_t tpw_df_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"faces", gDynamicFaces}, {"keys", gDynamicFaceKeys}, {"count", &gDynamicCellFBDBC}, {"f9bd4", &gDynamicCellF9BD4},
        {"flag", &gDynamicCellFlag}, {"random", gDynamicCellRandom}, {"tpg", &gTPGlobals}, {"gridA", gDynamicGridNormals},
        {"gridB", gDynamicGridPositions},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module needs: gTPGlobals' mask, scale and table (the header's 0x4, 0x8, 0xc), the table's length, a grid's size.
int32_t tpw_df_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = 4;
    out[n++] = 8;
    out[n++] = 0xc;
    out[n++] = static_cast<int64_t>(sizeof(gTPGlobals.mSinTable) / sizeof(gTPGlobals.mSinTable[0]));
    out[n++] = static_cast<int64_t>(sizeof(gDynamicGridPositions));
    out[n++] = static_cast<int64_t>(sizeof(sDynamicFace));
    out[n++] = static_cast<int64_t>(sizeof(gDynamicFaces));
    return n;
}

// ---- the functions (the type is passed as a plain int)
void tpw_df_sort(uint32_t* keysMinusOne, uint32_t count) { sort(keysMinusOne, count); }

void tpw_df_reset_list() { DC_ResetSquareFaceListPtr(); }

void tpw_df_add_square(float x, float y, float z, uint32_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                       uint32_t unused2, int32_t keepPosition, int32_t type) {
    DC_AddSquareFaceToList(x, y, z, static_cast<uint8_t>(side), texture, rotation, unused1, large, static_cast<uint8_t>(unused2), keepPosition,
                           static_cast<DYN_Type>(type));
}

void tpw_df_add_poly(s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points, int32_t type) {
    DC_AddPolyFaceToList(positions, texture, uvs, points, static_cast<DYN_Type>(type));
}

void tpw_df_rotate(s3DPoint* points, float angle) { HDZ_RotatePointZ(points, angle); }

void tpw_df_animate(int32_t columns, int32_t rows, float step, float phase) { DC_AnimateWater(columns, rows, step, phase); }

void tpw_df_draw(float x, float y, float z, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f) { DC_DrawWater(x, y, z, a, b, c, d, e, f); }

}  // extern "C"
