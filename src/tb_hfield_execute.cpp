#include "tpw_probe.h"
// CHField::Execute (engine 0x1000f69c, 6,072 bytes; the inventory's tb_hfield.cpp range): the
// height field's drawing, called by CMapWho::RenderWindow after the sky.
//
// Only when the view or the lights changed (system flags 0xc0000000; otherwise the static
// primitive of the last such frame is still in the render memory): the lights are prepared,
// every grid point lit (CalcFullLighting), the texture entry of every cell in view found
// (GetTexturesUsed), then the cells in view are walked row by row with two rows of transformed
// grid points (CalcLine) at hand. A cell with quad flag 1 is skipped; one whose four corners
// are all outside one plane is not drawn; the others become four vertices in one primitive
// (format 0x1c4): kept whole when both triangles face the viewer, one triangle when only one
// does, or, when a corner is outside, each facing triangle clipped by CClipping::ClipPoly.
// Each drawn cell leaves a QuadSaveInfo; they are sorted by page, and each run of cells on the
// same page gets the primitive (the first run) or a duplicate of it, its triangles as index
// fans and an execution with depth vertices * 2,000,000.
//
// Every a*b+c written with ppc_fmadds (the texture coordinates) and the facing test's
// ppc_fmsubs (tb_drawprimitive.h) are fused in the original.
//
// Quirks kept: the cell's corner 1 and 2 colours and points come from the next grid row, read
// through mVertexColours[row + 1] and the next CalcLine row with no bound check (row 127 would
// read past both); the specular word of every vertex is never written (the primitive keeps
// what its memory held; the clipped corners are copied to ClipPoly with stack garbage there);
// CalcLine's third array is copied along with the other two every row although neither CalcLine
// nor Execute reads it (the vertices take mVertexColours); when only the second triangle faces the viewer the four
// vertices are moved down by one with an overlapping memcpy (memmove here, which gives the
// forward copy's result); every primitive gets the vertex count of the whole height field.
#include "tb_hfield.h"

#include <cstdint>
#include <cstring>

#include "Light.h"
#include "MapWho.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_textureentry.h"

CTextureEntry* gQuadTextures[0x4000];
QuadSaveInfo gQuadSaveInfo[0x4000];
float gFaceX0, gFaceX1, gFaceX2, gFaceY0, gFaceY1, gFaceY2, gFaceW0, gFaceW1, gFaceW2;

namespace {

// The vertex size of format 0x1c4 (engine data 0x100517b8, the 0x1c4 entry of the vertex size
// table at 0x100517b4).
constexpr int32_t kVertexSize = 0x20;
static_assert(kVertexSize == sizeof(SFVF_TLCTEX), "one SFVF_TLCTEX a vertex");

// The texture coordinates of a cell's corners 0..3 (a local array in the original, copied
// from engine data 0x100517c4); ProcessTextureParameters chooses which corner gets which.
constexpr float kCornerUV[4][2] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};

// The part of a texture entry the vertices use: its place on its page.
struct TexturePlace {
    float mUSize, mVSize, mUOffset, mVOffset;
};

// One corner of a cell: its transformed point (four words from CalcLine), its colour and the
// corner number ProcessTextureParameters left for it.
struct Corner {
    const float* mPoint;
    uint32_t mColour;
    uint8_t mUV;
};

// A vertex of the cell: the point copied as four words, the colour (halved and without its
// low byte for a cell with quad flag 0x400, unless the display has flag 0x800), the texture
// coordinates of the corner placed on the page. The specular word stays as it was.
void PutVertex(SFVF_TLCTEX& out, const Corner& corner, bool shaded, const TexturePlace& place) {
    std::memcpy(&out.x, corner.mPoint, 4 * sizeof(float));
    out.colour = shaded ? (corner.mColour >> 1) & 0x7f7f7f00u : corner.mColour;
    out.u = ppc_fmadds(place.mUSize, kCornerUV[corner.mUV][0], place.mUOffset);
    out.v = ppc_fmadds(place.mVSize, kCornerUV[corner.mUV][1], place.mVOffset);
}

// The cell's four vertices from vertices[0] on: corners 0, 1, 2, 3 with quad flag 2, else
// 1, 2, 3, 0 (the diagonal between the two triangles changes).
void PutCell(SFVF_TLCTEX* vertices, const Corner (&corners)[4], const sHFieldQuad& quad, const TexturePlace& place) {
    const int32_t start = (quad.mF00 & 2) != 0 ? 0 : 1;
    for (int32_t k = 0; k < 4; ++k) {
        const bool shaded = (gei_Display.mFlags & CDisplay::kFlag800) == 0 && (quad.mF00 & 0x400) != 0;
        PutVertex(vertices[k], corners[(start + k) & 3], shaded, place);
    }
}

// The triangles of a polygon of `count` vertices from vertex `first` as a fan, written from
// the last triangle back: (first, first + k + 1, first + k + 2) for k = count - 3 down to 0.
void PutFan(uint16_t*& out, int32_t first, int32_t count) {
    for (int32_t k = count - 2; k > 0;) {
        --k;
        out[0] = static_cast<uint16_t>(first);
        out[1] = static_cast<uint16_t>(first + k + 1);
        out[2] = static_cast<uint16_t>(first + k + 2);
        out += 3;
    }
}

// Notes a drawn cell.
void SaveQuad(int32_t index, CTextureBase* page, int32_t first, const CTextureEntry* entry, int32_t count1,
              int32_t count2) {
    QuadSaveInfo& info = gQuadSaveInfo[index];
    info.mTexture = page;
    info.mF08 = entry != nullptr ? entry->mF40 : -1;
    info.mFirstVertex = first;
    info.mCount1 = static_cast<int16_t>(count1);
    info.mCount2 = static_cast<int16_t>(count2);
}

}  // namespace

int32_t CHField::Execute() {
    SGamutQuads& cells = *QuadRows();
    SGamutQuads& points = *VertexRows();
    if (cells.mFirstRow > cells.mEndRow) return 1;
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) == 0) {
        tpw_probe::Hit(tpw_probe::kHFieldExecuteSkipped);
        return 1;
    }
    tpw_probe::Hit(tpw_probe::kHFieldExecute);

    int32_t saved = 0;   // QuadSaveInfo records written
    gev_LightChain = nullptr;
    gei_MapWho->mF14[6]->PrepareLights();
    sAlignedBox box;     // not initialised (PrepareLighting does not read it)
    gei_LightManager->PrepareLighting(&gei_LightMatrix, &box);
    CalcFullLighting();
    void* textures[1024];
    textures[0] = nullptr;   // the original passes whatever is there when GetTexturesUsed lists no page
    int32_t textureCount;
    GetTexturesUsed(textures, textureCount, gQuadTextures);
    tpw_probe::Hit(tpw_probe::kHFieldTextures, static_cast<uint32_t>(textureCount));
    int32_t mode = 0x1256;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) mode |= 0x100000;
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(1, 4, 0x1c4, mode, textures[0]);
    if (primitive == nullptr) {
        tpw_probe::Hit(tpw_probe::kHFieldPrimitiveNull);
        return 1;
    }

    // Two grid rows at a time: [0] the cells' top row, [1] the row below (contiguous in the
    // original, which reads [1] as [0] + 0x80 and so on). `spare` is CalcLine's unused array.
    uint8_t codes[2][128];
    float vertices[2][128][4];
    uint32_t spare[2][128];
    int32_t used = 0;    // vertices written
    int32_t row = cells.mFirstRow;
    CalcLine(codes[1], reinterpret_cast<SVertex*>(vertices[1]), spare[1], points.mLow[row], points.mHigh[row], row);
    CTextureEntry** texture = gQuadTextures;   // one entry for each cell whose flag 1 is clear
    for (; row < cells.mEndRow; ++row) {
        std::memcpy(vertices[0], vertices[1], sizeof vertices[1]);
        std::memcpy(codes[0], codes[1], sizeof codes[1]);
        std::memcpy(spare[0], spare[1], sizeof spare[1]);
        CalcLine(codes[1], reinterpret_cast<SVertex*>(vertices[1]), spare[1], points.mLow[row + 1],
                 points.mHigh[row + 1], row + 1);
        for (int32_t cell = cells.mLow[row]; cell < cells.mHigh[row]; ++cell) {
            sHFieldQuad* quad = &mF00->mQuads.get()[cell + row * static_cast<int32_t>(mF00->mF18[0])];
            if ((quad->mF00 & 1) != 0) continue;
            if ((codes[0][cell] & codes[0][cell + 1] & codes[1][cell + 1] & codes[1][cell]) == 0) {
                // The corner numbers, a byte each in memory order (the Mac's word 0x00010203).
                uint32_t cornerWord;
                uint8_t* order = reinterpret_cast<uint8_t*>(&cornerWord);
                order[0] = 0;
                order[1] = 1;
                order[2] = 2;
                order[3] = 3;
                ProcessTextureParameters(&cornerWord, quad);
                CTextureEntry* entry = *texture;
                CTextureBase* page;
                TexturePlace place;
                if (entry == nullptr) {
                    page = nullptr;
                    place = {0.0f, 0.0f, 0.0f, 0.0f};
                } else {
                    page = entry->GetTexture();
                    place = {entry->mUSize, entry->mVSize, entry->mUOffset, entry->mVOffset};
                }
                const int32_t outside = codes[0][cell] | codes[0][cell + 1] | codes[1][cell + 1] | codes[1][cell];
                const Corner corners[4] = {
                    {vertices[0][cell], mVertexColours[row][cell], order[0]},
                    {vertices[1][cell], mVertexColours[row + 1][cell], order[1]},
                    {vertices[1][cell + 1], mVertexColours[row + 1][cell + 1], order[2]},
                    {vertices[0][cell + 1], mVertexColours[row][cell + 1], order[3]},
                };
                if (outside == 0) {
                    // All in view: the four vertices go straight into the primitive.
                    SFVF_TLCTEX* cell4 = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + used;
                    PutCell(cell4, corners, *quad, place);
                    const SFVF_TLCTEX* base = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData);
                    const bool front1 = SFVF_FacesViewer(base[used], base[used + 1], base[used + 2]);
                    const bool front2 = SFVF_FacesViewer(base[used], base[used + 2], base[used + 3]);
                    if (front1 && front2) {
                        SaveQuad(saved, page, used, entry, 4, 0);
                        used += 4;
                        ++saved;
                    } else if (front1 || front2) {
                        SaveQuad(saved, page, used, entry, 3, 0);
                        used += 3;
                        ++saved;
                        // Only the second triangle (0, 2, 3): its last two vertices move down.
                        if (front2) std::memmove(cell4 + 1, cell4 + 2, 2 * kVertexSize);
                    }
                } else {
                    // A corner outside: each facing triangle is clipped into the primitive.
                    SFVF_TLCTEX staged[4];
                    SFVF_TLCTEX clip[21];   // ClipPoly's second work buffer (0x2a0 bytes, as the original's)
                    int32_t count1 = 0;
                    int32_t count2 = 0;
                    PutCell(staged, corners, *quad, place);
                    if (SFVF_FacesViewer(staged[0], staged[1], staged[2])) {
                        std::memcpy(&clip[0], &staged[0], kVertexSize);
                        std::memcpy(&clip[1], &staged[1], kVertexSize);
                        std::memcpy(&clip[2], &staged[2], kVertexSize);
                        count1 = 3;
                        gei_Clipping->ClipPoly(outside, reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + used,
                                               clip, count1, kVertexSize);
                    }
                    if (SFVF_FacesViewer(staged[0], staged[2], staged[3])) {
                        std::memcpy(&clip[0], &staged[0], kVertexSize);
                        std::memcpy(&clip[1], &staged[2], kVertexSize);
                        std::memcpy(&clip[2], &staged[3], kVertexSize);
                        count2 = 3;
                        gei_Clipping->ClipPoly(outside,
                                               reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + (used + count1),
                                               clip, count2, kVertexSize);
                    }
                    if (count1 + count2 > 0) {
                        SaveQuad(saved, page, used, entry, count1, count2);
                        used += count1 + count2;
                        ++saved;
                    }
                }
            }
            ++texture;
        }
    }

    primitive->mVertexCount = used;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) gei_DrawPrimitive->CheckFogOnCurrentPrimitive();
    const int32_t blending = static_cast<int32_t>(primitive->mBlendingMode);   // after the fog, before the projection
    gei_DrawPrimitive->ProjectCurrentPrimitive(0);
    tpw_probe::Hit(tpw_probe::kHFieldCells, static_cast<uint32_t>(saved));
    qSortTextures_lpTex(gQuadSaveInfo, 0, saved - 1);

    // One execution per run of cells on the same page.
    int32_t i = 0;
    const QuadSaveInfo* info = gQuadSaveInfo;
    while (i < saved) {
        CTextureBase* const page = info->mTexture;
        int32_t run = 1;
        while (i + run < saved && page == gQuadSaveInfo[i + run].mTexture) ++run;
        if (primitive != nullptr) {
            primitive->mTexture = page;
        } else {
            primitive = gei_DrawPrimitive->DuplicateCurrentPrimitive(1, 4, 0x1c4, blending, info->mTexture);
            if (primitive == nullptr) return 1;
        }
        primitive->mF34 = info->mF08;
        uint16_t* index = primitive->mIndexData;
        for (; run != 0; --run) {
            PutFan(index, info->mFirstVertex, info->mCount1);
            PutFan(index, info->mFirstVertex + info->mCount1, info->mCount2);
            ++info;
            ++i;
        }
        primitive->mFlags = (primitive->mFlags | 0x100) & ~SPrimitive::kDrawn;
        primitive->mVertexCount = used;
        primitive->mIndexCount = static_cast<int32_t>(index - primitive->mIndexData);
        if (gei_DrawPrimitive->ExecuteCurrentPrimitive(static_cast<float>(primitive->mVertexCount * 2000000)) == 0)
            return 0;
        primitive = nullptr;
    }
    return 1;
}
