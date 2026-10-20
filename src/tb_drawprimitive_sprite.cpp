// tb_drawprimitive_sprite.cpp (engine library of SimTheme Park 2.04b3; the original file is
// tb_drawprimitive.cpp, the inventory files the range under tb_error.cpp): the draw primitive's
// polygon adder and its camera-facing sprite.
//
//   0x100078c0 CDrawPrimitive::AddPoly_TLCTEX   (824 bytes)
//   0x10007edc CDrawPrimitive::DrawSprite       (776)
//
// AddPoly_TLCTEX adds one convex polygon of transformed, lit, textured vertices to the primitive
// being built: it clips the polygon against the planes its outside codes name, appends the
// vertices to the primitive's vertex data and a fan of triangles to its indices. The rain, the
// snow, DrawSprite and the starfield draw through it. DrawSprite makes a primitive of its own
// for four corners, adds them as a polygon, fogs, projects and executes it.
#include "tb_drawprimitive.h"

#include <cstddef>
#include <cstring>

#include "MapWho.h"
#include "tb_clipping.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"

static_assert(sizeof(ClipCodeType) == 2, "the clip codes are two bytes");

namespace {

// gei_Display.mFlags: 0x2000 is the fog switch (CDisplay::kEnableFog), 0x4000 lets the vertices
// be fogged one by one (the name is not known).
constexpr uint32_t kDisplayFog = 0x2000;
constexpr uint32_t kDisplayVertexFog = 0x4000;

// The sprite's blending mode gains this bit when the fog is on (SPrimitive::kFogCleared: the
// fog intensities are worked out afresh below).
constexpr uint32_t kSpriteFogCleared = 0x100000;

// SSpriteDrawParameters::mFlags: the primitive is marked for round-off (SPrimitive::kRoundOff).
constexpr uint32_t kSpriteRoundOff = 0x400;
// AddPoly_TLCTEX's flags: the triangles of the fan turn the other way.
constexpr uint32_t kReverseWinding = 0x2;

// The vertex pool of a sprite: GetNextPrimitive(kDynamic, triangle list, SFVF_TLCTEX).
constexpr int32_t kSpritePool = 2;
constexpr int32_t kTriangleList = 4;
constexpr uint32_t kTLCTEXFormat = 0x1c4;
constexpr int32_t kVertexStride = 0x20;
// The polygons the clipper hands back have at most this many vertices before AddPoly_TLCTEX
// stops writing indices (a fan of up to 8 vertices).
constexpr int32_t kLargestFan = 8;

// The room the work arrays of a polygon need: four vertices plus what six planes can add, and
// the outside codes of the polygon.
constexpr size_t kWorkVertices = 16;
constexpr size_t kCodeBytes = 0x410;

// `pointer` moved by a byte count the Mac computes in 32 bits (a negative one goes back).
uintptr_t Moved(const void* pointer, uint32_t bytes) {
    return reinterpret_cast<uintptr_t>(pointer) + static_cast<intptr_t>(static_cast<int32_t>(bytes));
}

}  // namespace

// Quirks kept. The answer is always 1, whatever happened (the header's "0 on failure" never
// happens); a polygon with fewer than three vertices left by the clipper adds nothing, but a
// caller that passes fewer than three vertices with no clipping still has its vertex and index
// counts raised (by `count` and by (count - 2) * 3, both possibly negative) and gets no
// indices; a polygon of more than eight vertices gets its vertices and counts and no indices.
// The room test needs ten vertices and 24 indices to spare whatever the polygon is. The indices
// are written for the triangles from the last of the fan to the first, (v0, vk, vk+1) or, with
// flag 2, (v0, vk+1, vk), each from v0 = the number of the primitive's first new vertex.
// With no `clip` the outside codes of the polygon are worked out here (mask 0xff). The caller's
// vertices serve the clipper as work space (the polygon grows by up to six vertices there), so
// the array needs room for ten.
// Not reproduced: the original adds and multiplies addresses in 32 bits (the host's do not
// wrap at 4 GB); a NULL current primitive (the original reads low memory, harmlessly) adds
// nothing here.
int32_t CDrawPrimitive::AddPoly_TLCTEX(SFVF_TLCTEX* vertices, int32_t count, int32_t stride, ClipCodeType* clip,
                                       int32_t flags) {
    SPrimitive* primitive = mCurrent;
    if (primitive == nullptr) return 1;

    // Room for ten more vertices and 24 more indices in the memory the primitive may use.
    const uintptr_t vertexLimit =
        Moved(primitive->mVertexData, static_cast<uint32_t>(primitive->mVertexCount + 10) * static_cast<uint32_t>(VertexSize()));
    const uintptr_t indexLimit =
        Moved(primitive->mIndexData, static_cast<uint32_t>(primitive->mIndexCount + 0x18) * 2u);
    if (reinterpret_cast<uintptr_t>(mVertexEnd) < vertexLimit || reinterpret_cast<uintptr_t>(mIndexEnd) < indexLimit) {
        return 1;
    }

    // The first new vertex's number, where the vertices and the indices go.
    const int32_t firstVertex = primitive->mVertexCount;
    SFVF_TLCTEX* out = reinterpret_cast<SFVF_TLCTEX*>(
        Moved(primitive->mVertexData, static_cast<uint32_t>(firstVertex) * static_cast<uint32_t>(stride)));
    uint16_t* indices = reinterpret_cast<uint16_t*>(
        Moved(primitive->mIndexData, static_cast<uint32_t>(primitive->mIndexCount) * 2u));

    ClipCodeType own = {0xff, 0};
    if (clip == nullptr) {
        uint8_t codes[kCodeBytes];
        uint8_t result[4] = {0xff, 0, 0, 0};
        CClipping::CPP_TestData(result, gei_Clipping, codes, vertices, count, stride, 0xff);
        own.mAll = result[0];
        own.mAny = result[1];
        clip = &own;
    }
    if (clip->mAll != 0) return 1;

    if (clip->mAny == 0) {
        std::memcpy(out, vertices, static_cast<size_t>(static_cast<uint32_t>(stride) * static_cast<uint32_t>(count)));
    } else {
        gei_Clipping->ClipPoly(clip->mAny, out, vertices, count, stride);
        if (count < 3) return 1;
    }
    primitive = mCurrent;
    primitive->mVertexCount += count;
    primitive->mIndexCount += (count - 2) * 3;

    if (count < 3 || count > kLargestFan) return 1;
    const bool reversed = (static_cast<uint32_t>(flags) & kReverseWinding) != 0;
    for (int32_t k = count - 2; k >= 1; --k) {
        indices[0] = static_cast<uint16_t>(firstVertex);
        indices[1] = static_cast<uint16_t>(firstVertex + (reversed ? k + 1 : k));
        indices[2] = static_cast<uint16_t>(firstVertex + (reversed ? k : k + 1));
        indices += 3;
    }
    return 1;
}

// One quad of camera-facing corners (x, y, z, w each, in the screen's clip space) as a primitive of
// its own. `mode` is the mask of the planes whose outside codes count (callers pass -1, or -3 to
// leave out the far plane). The corners are the sprite's four, in order, with the picture's
// corners (u0, v0), (u1, v0), (u1, v1), (u0, v1) and the sprite's colour masked by the light
// manager's. The specular colour of the corners is never set (stack leftovers in the original,
// zero here). 1 when nothing is drawn (every corner outside a plane, no room for a primitive,
// AddPoly_TLCTEX's refusal), else what ExecuteCurrentPrimitive answers (also for a primitive
// with no vertices or no indices, which is not executed: 1). With the fog on the sprite's blending
// mode gains 0x100000 first (the caller's record is changed). The fogging and the projection are
// ApplyFog's and ProjectCurrentPrimitive's, written out in place (the projection's `type` is
// the sprite's last word).
int32_t CDrawPrimitive::DrawSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    ClipCodeType clip = {0xff, 0};
    if ((gei_Display.mFlags & kDisplayFog) != 0) sprite->mBlendingMode |= kSpriteFogCleared;

    uint8_t codes[16];
    uint8_t result[4] = {0xff, 0, 0, 0};
    CClipping::CPP_TestData(result, gei_Clipping, codes, sprite->mVertices, 4, 0x10, static_cast<uint32_t>(mode));
    clip.mAll = result[0];
    clip.mAny = result[1];
    if (clip.mAll != 0) return 1;

    SPrimitive* primitive = GetNextPrimitive(kSpritePool, kTriangleList, kTLCTEXFormat,
                                             static_cast<int32_t>(sprite->mBlendingMode), sprite->mTexture);
    if (primitive == nullptr) return 1;

    sprite->mF08 = static_cast<int32_t>(static_cast<uint32_t>(sprite->mF08) & static_cast<uint32_t>(gei_LightManager->mF04));

    // The corners, copied from the sprite's four and given its colour and the picture's corners.
    SFVF_TLCTEX quad[kWorkVertices] = {};
    const uint8_t* corners = static_cast<const uint8_t*>(sprite->mVertices);
    const float u[4] = {sprite->mU0, sprite->mU1, sprite->mU1, sprite->mU0};
    const float v[4] = {sprite->mV0, sprite->mV0, sprite->mV1, sprite->mV1};
    for (int32_t i = 0; i < 4; ++i) {
        std::memcpy(&quad[i], corners + 0x10 * i, 0x10);
        quad[i].colour = static_cast<uint32_t>(sprite->mF08);
        quad[i].u = u[i];
        quad[i].v = v[i];
    }
    if (AddPoly_TLCTEX(quad, 4, kVertexStride, &clip, static_cast<int32_t>(sprite->mFlags)) == 0) return 1;

    const uint32_t displayFlags = gei_Display.mFlags;
    if ((displayFlags & kDisplayFog) != 0 && (displayFlags & kDisplayVertexFog) != 0) {
        const uint8_t fogged = gei_LightManager->CalcVertexFogIntensities(
            reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData), primitive->mVertexCount, VertexSize());
        mCurrent->mBlendingMode &= ~SPrimitive::kFogCleared;
        if (fogged != 0) mCurrent->mBlendingMode |= SPrimitive::kFogged;
    }

    SPrimitive* current = mCurrent;
    if (current->mVertexCount != 0) {
        const float projected = gei_MathUnit->Project(current->mVertexData, VertexSize(), current->mVertexCount, sprite->mF28);
        current = mCurrent;
        current->mDepth = current->mDepth + projected;
    }
    if ((sprite->mFlags & kSpriteRoundOff) != 0) primitive->mFlags |= SPrimitive::kRoundOff;

    current = mCurrent;
    if (current == nullptr || current->mVertexCount == 0 || current->mIndexCount == 0) return 1;
    gei_RenderMemory->SaveInfo(
        reinterpret_cast<char*>(Moved(current->mVertexData,
                                      static_cast<uint32_t>(current->mVertexCount) * static_cast<uint32_t>(VertexSize()))),
        reinterpret_cast<uint16_t*>(Moved(current->mIndexData, static_cast<uint32_t>(current->mIndexCount) << 1)));
    return ExecuteCurrentPrimitive();
}
