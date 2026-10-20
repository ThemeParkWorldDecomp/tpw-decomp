// tb_drawprimitive_addsprite.cpp (engine library of SimTheme Park 2.04b3; the original file is
// tb_drawprimitive.cpp, the inventory files the range under tb_error.cpp): the draw primitive's
// batching sprite and the vertex record's four small members it builds its quad with.
//
//   0x10008230 CDrawPrimitive::AddSprite     (1,392 bytes)
//   0x100087e8 SFVF_TLCTEX::ReturnPtr        (12)
//   0x10008824 SFVF_TLCTEX::SetUV            (20)
//   0x10008864 SFVF_TLCTEX::SetDiffuse       (8)
//   0x1000889c SFVF_TLCTEX::SetVertex        (52)
//
// DrawSprite (src/tb_drawprimitive_sprite.cpp) makes a primitive of its own for each sprite and
// executes it. AddSprite is what CSpriteManager::RenderSprites uses (through SSpriteGameData::
// AddRegular): it adds the four corners to the primitive being built, so that every sprite of
// one texture and blending mode shares a primitive, and the caller executes the batch when the
// kind changes. It starts a primitive only when there is none.
#include "tb_drawprimitive.h"

#include <cstddef>
#include <cstring>

#include "MapWho.h"
#include "tb_clipping.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"

namespace {

// gei_Display.mFlags: the fog switch and the per-vertex fog (as DrawSprite).
constexpr uint32_t kDisplayFog = 0x2000;
constexpr uint32_t kDisplayVertexFog = 0x4000;
// The sprite's blending mode gains this bit when the fog is on, and the primitive's loses it
// (SPrimitive::kFogCleared) before the fog intensities say whether the primitive is fogged.
constexpr uint32_t kSpriteFogCleared = 0x100000;
// AddPoly_TLCTEX's flags: the triangles of the fan turn the other way.
constexpr uint32_t kReverseWinding = 0x2;

// The sprite's primitive: GetNextPrimitive(kDynamic, triangle list, SFVF_TLCTEX).
constexpr int32_t kSpritePool = 2;
constexpr int32_t kTriangleList = 4;
constexpr uint32_t kTLCTEXFormat = 0x1c4;
// The corners are 0x20 bytes apart whatever vertex size the object holds (the room test below
// does use the object's).
constexpr int32_t kVertexStride = 0x20;
constexpr uint32_t kVertexShift = 5;
// The room the corners need as the clipper's work space: four vertices plus what six planes can add.
constexpr size_t kWorkVertices = 16;

// `pointer` moved by a byte count the Mac computes in 32 bits (a negative one goes back).
uintptr_t Moved(const void* pointer, uint32_t bytes) {
    return reinterpret_cast<uintptr_t>(pointer) + static_cast<intptr_t>(static_cast<int32_t>(bytes));
}

// One corner: the sprite's x, y, z and w for it (the four words at `corner`), its colour, and
// the picture's coordinate (u, v) as texture set 0.
void SetCorner(SFVF_TLCTEX* vertex, const void* corner, int32_t colour, float u, float v) {
    SVertex position;
    std::memcpy(&position, corner, sizeof position);
    vertex->SetVertex(position);
    vertex->SetDiffuse(static_cast<uint32_t>(colour));
    vertex->SetUV(0, u, v);
}

// The four corners of the sprite in the order the picture's corners go with, (u0, v0),
// (u1, v0), (u1, v1), (u0, v1), written to consecutive records from `first`.
void SetCorners(SFVF_TLCTEX* first, const SSpriteDrawParameters* sprite) {
    const uint8_t* corners = static_cast<const uint8_t*>(sprite->mVertices);
    const float u[4] = {sprite->mU0, sprite->mU1, sprite->mU1, sprite->mU0};
    const float v[4] = {sprite->mV0, sprite->mV0, sprite->mV1, sprite->mV1};
    SFVF_TLCTEX* vertex = first;
    for (int32_t i = 0; i < 4; ++i) {
        SetCorner(vertex, corners + 0x10 * i, sprite->mF08, u[i], v[i]);
        if (i < 3) vertex = vertex->ReturnPtr(kVertexStride, 1);
    }
}

}  // namespace

// ---- SFVF_TLCTEX ----------------------------------------------------------------------------------

SFVF_TLCTEX* SFVF_TLCTEX::ReturnPtr(int32_t size, int32_t index) {
    const uint32_t bytes = static_cast<uint32_t>(index) * static_cast<uint32_t>(size);
    return reinterpret_cast<SFVF_TLCTEX*>(Moved(this, bytes));
}

void SFVF_TLCTEX::SetUV(int32_t set, float newU, float newV) {
    SFVF_TLCTEX* where = reinterpret_cast<SFVF_TLCTEX*>(Moved(this, static_cast<uint32_t>(set) << 3));
    where->u = newU;
    where->v = newV;
}

void SFVF_TLCTEX::SetDiffuse(uint32_t newColour) { colour = newColour; }

void SFVF_TLCTEX::SetVertex(SVertex vertex) {
    x = vertex.x;
    y = vertex.y;
    z = vertex.z;
    rhw = vertex.w;
}

// ---- CDrawPrimitive::AddSprite ----------------------------------------------------------------------

// The steps, in the listing's order. The display's fog switch adds 0x100000 to the sprite's own
// blending mode (the caller's record changes); the outside codes of the four corners are worked
// out with the mask `mode` (-1 all planes, -3 all but the far one, as DrawSprite) and a sprite
// wholly outside one plane answers 1. The primitive is the current one, or a new one from
// GetNextPrimitive (NULL: answer 1) with the sprite's blending mode and texture; if it already
// existed it only takes the sprite's blending mode over, its texture is not looked at. The sprite's
// colour is ANDed with the light manager's and stored back, the blending mode stored in the
// primitive.
//
// Two paths. A sprite that crosses a plane (the OR of the codes is not zero) is built on the stack
// and given to AddPoly_TLCTEX with the codes just worked out, which clips it into the primitive's
// memory; nothing added (no room, clipped away) answers 1 with nothing more done, else the
// per-vertex fog and the projection treat only the vertices added. A sprite wholly inside is
// written directly: the room test needs four vertices and six indices to spare, with the vertex
// size of the OBJECT (+0x18) for the vertices and the corners are placed 0x20 apart whatever that
// size is; the corners go to the vertex memory at the vertex count times 0x20, the counts rise by
// 4 and 6, and the six indices are (b, b + 2, b + 3, b, b + 1, b + 2) or, with flag 2, (b, b + 3,
// b + 2, b, b + 2, b + 1), b being the vertex count before. Then fog and projection as above.
//
// Per-vertex fog (display flags 0x2000 and 0x4000 both set): CalcVertexFogIntensities(the new
// vertices, their number, the object's vertex size); the CURRENT primitive's blending mode loses
// 0x100000 and gains 0x80000 when it answers nonzero. The projection: the math unit's Project
// (the new vertices, 0x20, their number, the sprite's last word) and the primitive's depth
// (+0x30) is raised by its answer (single precision). Always 1.
//
// Quirks kept. In the no-room case the blending mode, the sprite's colour and a new primitive's
// existence are already changed. The specular word of every corner is never written (stack
// leftovers on the Mac for the clipped path, whatever was in the vertex memory for the direct
// one): the translation's stack copies are zero. Not reproduced: the original adds and multiplies
// addresses in 32 bits (the host's do not wrap at 4 GB).
int32_t CDrawPrimitive::AddSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    ClipCodeType clip = {0xff, 0};
    if ((gei_Display.mFlags & kDisplayFog) != 0) sprite->mBlendingMode |= kSpriteFogCleared;

    uint8_t codes[16];
    uint8_t result[4] = {0xff, 0, 0, 0};
    CClipping::CPP_TestData(result, gei_Clipping, codes, sprite->mVertices, 4, 0x10, static_cast<uint32_t>(mode));
    clip.mAll = result[0];
    clip.mAny = result[1];
    if (clip.mAll != 0) return 1;

    SPrimitive* primitive = mCurrent;
    if (primitive == nullptr) {
        primitive = GetNextPrimitive(kSpritePool, kTriangleList, kTLCTEXFormat, static_cast<int32_t>(sprite->mBlendingMode),
                                     sprite->mTexture);
        if (primitive == nullptr) return 1;
    }

    sprite->mF08 = static_cast<int32_t>(static_cast<uint32_t>(sprite->mF08) & static_cast<uint32_t>(gei_LightManager->mF04));
    primitive->mBlendingMode = sprite->mBlendingMode;
    SFVF_TLCTEX* first = reinterpret_cast<SFVF_TLCTEX*>(
        Moved(primitive->mVertexData, static_cast<uint32_t>(primitive->mVertexCount) << kVertexShift));

    int32_t added;
    if (clip.mAny != 0) {
        // Crosses a plane: build the corners on the stack and have the polygon adder clip them in.
        SFVF_TLCTEX quad[kWorkVertices] = {};
        SetCorners(quad, sprite);
        const int32_t before = primitive->mVertexCount;
        AddPoly_TLCTEX(quad, 4, kVertexStride, &clip, static_cast<int32_t>(sprite->mFlags));
        added = primitive->mVertexCount - before;
        if (added == 0) return 1;
    } else {
        // Wholly inside: write the corners straight into the primitive, if there is room for them.
        SPrimitive* current = mCurrent;
        const uintptr_t vertexLimit = Moved(current->mVertexData, static_cast<uint32_t>(VertexSize()) * static_cast<uint32_t>(current->mVertexCount + 4));
        const uintptr_t indexLimit = Moved(current->mIndexData, static_cast<uint32_t>(current->mIndexCount + 6) << 1);
        if (reinterpret_cast<uintptr_t>(mVertexEnd) < vertexLimit || reinterpret_cast<uintptr_t>(mIndexEnd) < indexLimit) return 1;

        uint16_t* indices = reinterpret_cast<uint16_t*>(Moved(primitive->mIndexData, static_cast<uint32_t>(primitive->mIndexCount) << 1));
        SetCorners(first, sprite);
        const int32_t base = primitive->mVertexCount;
        primitive->mVertexCount = base + 4;
        primitive->mIndexCount += 6;
        if ((sprite->mFlags & kReverseWinding) != 0) {
            indices[0] = static_cast<uint16_t>(base);
            indices[1] = static_cast<uint16_t>(base + 3);
            indices[2] = static_cast<uint16_t>(base + 2);
            indices[3] = static_cast<uint16_t>(base);
            indices[4] = static_cast<uint16_t>(base + 2);
            indices[5] = static_cast<uint16_t>(base + 1);
        } else {
            indices[0] = static_cast<uint16_t>(base);
            indices[1] = static_cast<uint16_t>(base + 2);
            indices[2] = static_cast<uint16_t>(base + 3);
            indices[3] = static_cast<uint16_t>(base);
            indices[4] = static_cast<uint16_t>(base + 1);
            indices[5] = static_cast<uint16_t>(base + 2);
        }
        added = 4;
    }

    const uint32_t displayFlags = gei_Display.mFlags;
    if ((displayFlags & kDisplayFog) != 0 && (displayFlags & kDisplayVertexFog) != 0) {
        const uint8_t fogged = gei_LightManager->CalcVertexFogIntensities(first, added, VertexSize());
        mCurrent->mBlendingMode &= ~SPrimitive::kFogCleared;
        if (fogged != 0) mCurrent->mBlendingMode |= SPrimitive::kFogged;
    }
    const float projected = gei_MathUnit->Project(first, kVertexStride, added, sprite->mF28);
    primitive->mDepth = primitive->mDepth + projected;
    return 1;
}
