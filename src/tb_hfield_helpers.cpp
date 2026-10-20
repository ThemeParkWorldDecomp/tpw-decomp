// tb_hfield_helpers.cpp: the height field's helpers that CHField::Execute and the culling
// call (engine tb_hfield.cpp range, 0x1000e760..0x1000f658), and the face sorter of the
// draw primitive (QSortFaces_float, 0x10006d40, which the inventory files under tb_error.cpp).
//
//   0x1000e760 CHField::CheckGamutArea     (148 bytes)
//   0x1000e824 CHField::CalcFullLighting   (492)
//   0x1000ea40 CHField::CalcLine           (672)
//   0x1000f060 CHField::QSort_textures     (284)
//   0x1000f1b0 CHField::GetTexturesUsed    (516)
//   0x1000f3f8 CHField::ProcessTextureParameters (172)
//   0x1000f4ec CHField::qSortTextures_lpTex (364)
//   0x10006d40 QSortFaces_float            (324)
#include "tb_hfield.h"

#include <utility>

#include "MapWho.h"
#include "m3d2.h"
#include "platform/d3d_types.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

namespace {

// The middle of a range as the sorts take it: the 32-bit sum shifted right arithmetically.
int32_t Middle(int32_t low, int32_t high) {
    return static_cast<int32_t>(static_cast<uint32_t>(low) + static_cast<uint32_t>(high)) >> 1;
}

// A page (or any pointer) as the Mac compares it: unsigned.
uintptr_t Address(const void* pointer) { return reinterpret_cast<uintptr_t>(pointer); }

}  // namespace

// Whether any row of the area's rows (area[1] to area[0]) has cells in view that reach the
// area's columns (area[3] to area[2]); `area` is a node's mF90. The rows in view are only
// compared with the area's first and last row once; every row the area names is then looked
// at, in view or not, so a row outside 0..127 reads whatever lies next to the row bytes.
int32_t CHField::CheckGamutArea(int8_t* area) {
    const int32_t lastRow = area[0];
    if (mF128 > lastRow) return 0;
    const int32_t firstRow = area[1];
    if (mF12C < firstRow) return 0;
    const SGamutQuads* cells = QuadRows();
    for (int32_t row = firstRow; row <= lastRow; ++row) {
        if (cells->mLow[row] <= area[2] && cells->mHigh[row] >= area[3]) return 1;
    }
    return 0;
}

// The colour of every grid point of the rows 0..mGridHeight - 1 (the whole grid, not only
// the points in view), once for each change of the lights. A row is built on the stack as
// 128 positions and normals, then CLightManager::CalcIntensities fills the row's colours.
// Quirks kept: the position steps by the origin's x (sHField mF08[0]) and not by the cell
// size, while z steps by the cell size; the last row's z slope reads the row 128 floats
// back, not mGridWidth floats; the arrays hold 128 points, as the stack frame does.
void CHField::CalcFullLighting() {
    if (mF12968 == gei_System.mLightFrame && (gei_System.mFlags & CSystem::kLightsChanged) == 0) return;
    const sHField& field = *mF00;
    const float* heights = field.mHeights;
    SVertex vertices[128];
    D3DVECTOR normals[128];
    float z = field.mF08[1];
    for (int32_t row = 0; row < mGridHeight; ++row) {
        float x = field.mF08[0];
        for (int32_t column = 0; column < mGridWidth; ++column) {
            vertices[column].x = x;
            vertices[column].y = heights[0];
            vertices[column].z = z;
            float slopeX;
            if (column <= 0) {
                slopeX = heights[0] - heights[1];
            } else if (column < mGridWidth - 1) {
                slopeX = heights[-1] - heights[1];
            } else {
                slopeX = heights[-1] - heights[0];
            }
            normals[column].x = slopeX * mInvHeightRange;
            normals[column].y = 1.0f;
            float slopeZ;
            if (row <= 0) {
                slopeZ = heights[0] - heights[mGridWidth];
            } else if (row < mGridHeight - 1) {
                slopeZ = heights[-mGridWidth] - heights[mGridWidth];
            } else {
                slopeZ = heights[-0x80] - heights[0];
            }
            normals[column].z = slopeZ * mInvHeightRange;
            ++heights;
            x += field.mF08[0];
        }
        gei_LightManager->CalcIntensities(mVertexColours[row], normals, vertices, mGridWidth);
        z += field.mF08[3];
    }
    mF12968 = gei_System.mLightFrame;
}

// Grid row `row`, columns low..high: the world position of each point (x = column * cell + origin,
// z = row * cell + origin, both fused; y is the height added to 0.0, which turns -0.0 into
// +0.0), transformed in place by the final matrix, then the outside code of each point (the
// clipper's test, mask 0xff) into codes[low..high]. Nothing happens when low is 127 or more.
// The third array is never read; the w word of a point is whatever Transform makes of it. An
// empty range (low above high) still transforms and tests with a count of 0 or less.
void CHField::CalcLine(uint8_t* codes, SVertex* vertices, uint32_t* spare, int32_t low, int32_t high, int32_t row) {
    if (low >= 127) return;
    const sHField& field = *mF00;
    SVertex* first = vertices + low;
    const float* heights = field.mHeights;
    const int32_t rowStart = mGridWidth * row;
    const float z = ppc_fmadds(static_cast<float>(row), field.mF08[3], field.mF08[1]);
    for (int32_t column = low; column <= high; ++column) {
        SVertex& point = vertices[column];
        point.x = ppc_fmadds(static_cast<float>(column), field.mF08[2], field.mF08[0]);
        point.y = 0.0f + heights[column + rowStart];
        point.z = z;
    }
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    const int32_t count = high - low + 1;
    gei_MathUnit->Transform(gei_FinalMatrix, first, first, 0x10, 0x10, count, 0);
    uint8_t unused[4];   // the clipper's answer (the AND and the OR of the codes): not used
    CClipping::CPP_TestData(unused, gei_Clipping, codes + low, first, count, 0x10, 0xff);
}

// Sorts the pages by address (unsigned), the quicksort of the original: the pivot is the
// middle element, the first scan runs up while the element is below it. QUIRK: the second scan
// runs down while the element is above the pivot and the FIRST index is above `low` (the
// natural test is the second index), so when the first scan stops at `low` the second does
// not run and the swap takes the last element, and the result can come out unsorted.
// An empty range does nothing; the original reads textures[(low + high) >> 1] first.
void CHField::QSort_textures(void** textures, int32_t low, int32_t high) {
    if (high < low) return;
    int32_t i = low;
    int32_t j = high;
    const uintptr_t pivot = Address(textures[Middle(low, high)]);
    do {
        while (i <= high && Address(textures[i]) < pivot) ++i;
        while (low < i && Address(textures[j]) > pivot) --j;
        if (i > j) break;
        std::swap(textures[i], textures[j]);
        --j;
        ++i;
    } while (i <= j);
    if (low < j) QSort_textures(textures, low, j);
    if (i < high) QSort_textures(textures, i, high);
}

// For every cell in view (rows mF128 .. mF12C - 1 of the SGamutQuads at +0x128, columns mLow ..
// mHigh - 1 of each) whose quad does not have flag 1: its texture entry goes to entries[],
// in the order of the cells, and its page (the entry's texture while it is on a cache page,
// else NULL) to textures[] unless that is in the list already; count is the length of the
// list, which is sorted at the end. The entry is the next of the procedural texture entries
// (mPrepareEntries, which PrepareList filled) when texture setting 4 is on and the cell is inside the rectangle
// mF234..mF23C by mF238..mF240, else the first entry of the mesh texture the quad names
// (NULL without a texture object).
void CHField::GetTexturesUsed(void** textures, int32_t& count, CTextureEntry** entries) {
    int32_t entryCount = 0;
    int32_t procCount = 0;
    count = 0;
    const SGamutQuads* cells = QuadRows();
    const sHField& field = *mF00;
    for (int32_t z = mF128; z < mF12C; ++z) {
        for (int32_t x = cells->mLow[z]; x < cells->mHigh[z]; ++x) {
            const sHFieldQuad& quad = field.mQuads[x + z * static_cast<int32_t>(field.mF18[0])];
            if ((quad.mF00 & 1) != 0) continue;
            bool procedural = false;
            if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesOn) != 0) {
                procedural = x >= mF234 && x <= mF23C && z >= mF238 && z <= mF240;
            }
            CTextureEntry* entry;
            if (procedural) {
                entry = mPrepareEntries[procCount++].mTexture;
            } else {
                CsTexture* texture = mF04[quad.mF02].mTexture;
                entry = texture != nullptr ? texture->GetTextureEntry(0) : nullptr;
            }
            entries[entryCount++] = entry;
            void* page = entry != nullptr ? entry->GetTexture() : nullptr;
            int32_t found = 0;
            while (found < count && textures[found] != page) ++found;
            if (found == count) {
                textures[found] = page;
                ++count;
            }
        }
    }
    QSort_textures(textures, 0, count - 1);
}

// Turns the four corner numbers of a cell (one byte each, in memory order) by the quad's
// flags: 0x40 first swaps the second and fourth; then 0x08 rotates the four one place right
// (the last comes first), else 0x10 swaps the first with the third and the second with the
// fourth, else 0x20 rotates one place left (the first goes last). No flag among 0x78: no change.
void CHField::ProcessTextureParameters(uint32_t* corners, sHFieldQuad* quad) {
    if ((quad->mF00 & 0x78) == 0) return;
    uint8_t* c = reinterpret_cast<uint8_t*>(corners);
    if ((quad->mF00 & 0x40) != 0) std::swap(c[1], c[3]);
    if ((quad->mF00 & 0x08) != 0) {
        const uint8_t last = c[3];
        c[3] = c[2];
        c[2] = c[1];
        c[1] = c[0];
        c[0] = last;
        return;
    }
    if ((quad->mF00 & 0x10) != 0) {
        std::swap(c[0], c[2]);
        std::swap(c[1], c[3]);
        return;
    }
    if ((quad->mF00 & 0x20) != 0) {
        const uint8_t first = c[0];
        c[0] = c[1];
        c[1] = c[2];
        c[2] = c[3];
        c[3] = first;
    }
}

// Sorts the quads Execute drew by their page (the first word, unsigned): QSort_textures'
// quicksort with the natural second scan (the second index above `low`), moving whole records.
// An empty range does nothing (the original reads quads[(low + high) >> 1] first).
void CHField::qSortTextures_lpTex(QuadSaveInfo* quads, int32_t low, int32_t high) {
    if (high < low) return;
    int32_t i = low;
    int32_t j = high;
    const uintptr_t pivot = Address(quads[Middle(low, high)].mTexture);
    do {
        while (i <= high && Address(quads[i].mTexture) < pivot) ++i;
        while (j > low && Address(quads[j].mTexture) > pivot) --j;
        if (i > j) break;
        std::swap(quads[i], quads[j]);
        --j;
        ++i;
    } while (i <= j);
    if (low < j) qSortTextures_lpTex(quads, low, j);
    if (i < high) qSortTextures_lpTex(quads, i, high);
}

// Sorts the faces of a primitive by key, largest first: the same quicksort on float keys
// (ordered compares, so a NaN key stops both scans), and every swap of two keys swaps the
// two faces (three 16-bit indices each) at the same positions.
void QSortFaces_float(float* keys, uint16_t* faces, int32_t low, int32_t high) {
    if (high < low) return;
    int32_t i = low;
    int32_t j = high;
    const float pivot = keys[Middle(low, high)];
    do {
        while (i <= high && keys[i] > pivot) ++i;
        while (j > low && keys[j] < pivot) --j;
        if (i > j) break;
        std::swap(keys[i], keys[j]);
        std::swap(faces[3 * i], faces[3 * j]);
        std::swap(faces[3 * i + 1], faces[3 * j + 1]);
        std::swap(faces[3 * i + 2], faces[3 * j + 2]);
        --j;
        ++i;
    } while (i <= j);
    if (low < j) QSortFaces_float(keys, faces, low, j);
    if (i < high) QSortFaces_float(keys, faces, i, high);
}
