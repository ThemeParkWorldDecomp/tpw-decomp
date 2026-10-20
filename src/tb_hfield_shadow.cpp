// The height field's cloud shadow pass (engine tb_hfield.cpp):
// CHField::CalcShadowLine 0x10010e7c (712 bytes), DrawSkyShadow 0x10011180 (5,488).
#include "tb_hfield.h"

#include <cstring>

#include "m3d2.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_sky.h"
#include "tb_textureentry.h"

uint8_t gShadowUInitialized = 0, gShadowVInitialized = 0;
float gShadowUScale, gShadowVScale;

void CHField::CalcShadowLine(uint8_t* codes, SVertex* vertices, int32_t low, int32_t high, int32_t row) {
    if (low == 127 || high == -1) return;
    const sHField& field = *mF00;
    const float* heights = mF12970 != nullptr ? static_cast<float*>(mF12970) : field.mHeights.get();
    const float lift = gei_Display.mF938;
    SVertex* first = vertices + low;
    const int32_t rowStart = mGridWidth * row;
    const float z = ppc_fmadds(static_cast<float>(row), field.mF08[3], field.mF08[1]);
    for (int32_t column = low; column <= high; ++column) {
        vertices[column].x = ppc_fmadds(static_cast<float>(column), field.mF08[2], field.mF08[0]);
        vertices[column].y = lift + heights[column + rowStart];
        vertices[column].z = z;
    }
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    const int32_t count = high - low + 1;
    gei_MathUnit->Transform(gei_FinalMatrix, first, first, 0x10, 0x10, count, 0);
    uint8_t unused[4];
    CClipping::CPP_TestData(unused, gei_Clipping, codes + low, first, count, 0x10, 0xff);
}

namespace {

constexpr int32_t kVertexSize = sizeof(SFVF_TLCTEX);
// Engine constant 0x1004fc10, shared with the sky's scroll coordinates.
constexpr float kStep = 0x1.78d4fep-6f;

// Writes a reverse fan, with the clipped counts narrowed to signed halfwords first.
void PutFan(uint16_t*& out, int32_t first, int16_t count) {
    for (int32_t k = count - 2; k > 0;) {
        --k;
        out[0] = static_cast<uint16_t>(first);
        out[1] = static_cast<uint16_t>(first + k + 1);
        out[2] = static_cast<uint16_t>(first + k + 2);
        out += 3;
    }
}

// The specular word is untouched, including in the stack polygons sent to ClipPoly.
void PutCell(SFVF_TLCTEX* out, const float (&points)[2][128][4], int32_t cell, int32_t row,
             bool diagonal, uint32_t colour, const float (&uv)[4][2], const float (&place)[4]) {
    const float* corners[4] = {points[0][cell], points[1][cell], points[1][cell + 1], points[0][cell + 1]};
    const int32_t start = diagonal ? 0 : 1;
    for (int32_t i = 0; i < 4; ++i) {
        const int32_t corner = (start + i) & 3;
        const bool edge = corner == 0 || corner == 3 ? row == 0 : row == -1;
        std::memcpy(&out[i].x, corners[corner], 16);
        out[i].colour = edge ? colour & 0xffffff00u : colour;
        out[i].u = ppc_fmadds(place[0], uv[corner][0], place[2]);
        out[i].v = ppc_fmadds(place[1], uv[corner][1], place[3]);
    }
}

}  // namespace

int32_t CHField::DrawSkyShadow() {
    if (mF12970 == nullptr) return 1;
    if ((gei_System.mFlags & 0xc0000000u) == 0 && mShadowPrimitive != nullptr) {
        SFVF_TLCTEX* vertices = reinterpret_cast<SFVF_TLCTEX*>(mShadowPrimitive->mVertexData);
        const D3DVECTOR* position = gei_Sky.GetPosition();
        float previous[3];
        std::memcpy(previous, mPad295C, sizeof previous);
        const float dz = position->z - previous[2];
        const float dx = position->x - previous[0];
        std::memcpy(mPad295C, position, sizeof previous);
        for (uint32_t i = 0; i < static_cast<uint32_t>(mShadowPrimitive->mVertexCount); ++i) {
            vertices[i].u += dz;
            vertices[i].v += dx;
        }
        return 1;
    }
    CTextureEntry* entry = gei_Sky.GetSkyTEntry();
    if (entry == nullptr) return 1;
    const uint32_t flags = (gei_Display.mFlags & CDisplay::kEnableFog) != 0 ? 0x60 : 0x20;
    uint32_t mode = (entry->GetBlendingMode() & ~0x400u) | 0x800;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) mode |= 0x100000;
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(1, 4, 0x1c4, static_cast<int32_t>(mode),
                                                              entry->GetTexture());
    mShadowPrimitive = primitive;
    if (primitive == nullptr) return 1;
    uint16_t* indices = primitive->mIndexData;
    const uintptr_t vertexLimit = reinterpret_cast<uintptr_t>(gei_DrawPrimitive->mVertexEnd) - 16 * kVertexSize;
    const uintptr_t indexLimit = reinterpret_cast<uintptr_t>(gei_DrawPrimitive->mIndexEnd) - 0x30;
    const D3DVECTOR* position = gei_Sky.GetPosition();
    const uint32_t colour = static_cast<uint32_t>(gei_Display.mF8FC);
    std::memcpy(mPad295C, position, 12);
    primitive->mFlags = flags;
    SGamutQuads& cells = *QuadRows();
    SGamutQuads& rows = *VertexRows();
    uint8_t codes[2][128];
    float points[2][128][4];
    int32_t used = 0;
    CalcShadowLine(codes[1], reinterpret_cast<SVertex*>(points[1]), rows.mLow[cells.mFirstRow],
                   rows.mHigh[cells.mFirstRow], cells.mFirstRow);
    const float place[4] = {entry->mUSize, entry->mVSize, entry->mUOffset, entry->mVOffset};
    for (int32_t row = cells.mFirstRow; row < cells.mEndRow; ++row) {
        std::memcpy(points[0], points[1], sizeof points[0]);
        std::memcpy(codes[0], codes[1], sizeof codes[0]);
        CalcShadowLine(codes[1], reinterpret_cast<SVertex*>(points[1]), rows.mLow[row + 1], rows.mHigh[row + 1], row + 1);
        const float rowUV = kStep * static_cast<float>(row);
        for (int32_t cell = cells.mLow[row]; cell < cells.mHigh[row]; ++cell) {
            SFVF_TLCTEX* out = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + used;
            if (vertexLimit < reinterpret_cast<uintptr_t>(out) || indexLimit < reinterpret_cast<uintptr_t>(indices))
                goto finished;
            const sHFieldQuad& quad = mF00->mQuads.get()[cell + row * static_cast<int32_t>(mF00->mF18[0])];
            // The original tests bit 0x80000000 of this zero-extended halfword, so no quad flags skip it.
            if ((codes[0][cell] & codes[0][cell + 1] & codes[1][cell] & codes[1][cell + 1]) != 0) continue;
            if (gShadowUInitialized == 0) {
                gShadowUScale = 1.0f;
                gShadowUInitialized = 1;
            }
            if (gShadowVInitialized == 0) {
                gShadowVScale = 1.0f;
                gShadowVInitialized = 1;
            }
            const float columnUV = kStep * static_cast<float>(cell);
            const float uv[4][2] = {
                {gShadowUScale * (position->x + columnUV), gShadowVScale * ((kStep + position->z) - rowUV)},
                {gShadowUScale * (position->x + columnUV), gShadowVScale * (position->z - rowUV)},
                {gShadowUScale * ((kStep + position->x) + columnUV), gShadowVScale * (position->z - rowUV)},
                {gShadowUScale * ((kStep + position->x) + columnUV), gShadowVScale * ((kStep + position->z) - rowUV)},
            };
            const int32_t outside = codes[0][cell] | codes[0][cell + 1] | codes[1][cell] | codes[1][cell + 1];
            const int32_t first = used;
            int32_t count1 = 0, count2 = 0;
            if (outside == 0) {
                PutCell(out, points, cell, row, (quad.mF00 & 2) != 0, colour, uv, place);
                const bool front1 = SFVF_FacesViewer(out[0], out[1], out[2]);
                const bool front2 = SFVF_FacesViewer(out[0], out[2], out[3]);
                if (front1 && front2) {
                    count1 = 4;
                } else if (front1 || front2) {
                    count1 = 3;
                    if (front2) std::memmove(out + 1, out + 2, 2 * kVertexSize);
                }
                used += count1;
            } else {
                SFVF_TLCTEX staged[4];
                SFVF_TLCTEX clip[21];
                PutCell(staged, points, cell, row, (quad.mF00 & 2) != 0, colour, uv, place);
                if (SFVF_FacesViewer(staged[0], staged[1], staged[2])) {
                    std::memcpy(clip, staged, 3 * kVertexSize);
                    count1 = 3;
                    gei_Clipping->ClipPoly(outside, out, clip, count1, kVertexSize);
                }
                if (SFVF_FacesViewer(staged[0], staged[2], staged[3])) {
                    std::memcpy(clip, staged, kVertexSize);
                    std::memcpy(clip + 1, staged + 2, 2 * kVertexSize);
                    count2 = 3;
                    gei_Clipping->ClipPoly(outside, out + count1, clip, count2, kVertexSize);
                }
                if (count1 + count2 > 0) {
                    used += count1 + count2;
                } else {
                    count1 = 0;
                    count2 = 0;
                }
            }
            const int16_t fan1 = static_cast<int16_t>(count1);
            const int16_t fan2 = static_cast<int16_t>(count2);
            PutFan(indices, first, fan1);
            PutFan(indices, first + fan1, fan2);
        }
    }
finished:
    primitive->mVertexCount = used;
    primitive->mIndexCount = static_cast<int32_t>(indices - primitive->mIndexData);
    if (primitive->mVertexCount == 0 || primitive->mIndexCount == 0) {
        mShadowPrimitive = nullptr;
        return 1;
    }
    const int32_t depth = static_cast<int32_t>(static_cast<uint32_t>(used) * 1900000u);
    return gei_DrawPrimitive->ExecuteCurrentPrimitive(static_cast<float>(depth)) == 0 ? 0 : 1;
}
