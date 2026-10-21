// DrawPrimitive.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of DrawPrimitive code, named after it.

#include "helpers/tb_errorHelpers.h"
#include "helpers/tb_hfieldHelpers.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/d3d_types.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_clipping.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include <cstddef>
#include <cstring>
#include <utility>

// tb_drawprimitive.cpp (engine library of SimTheme Park 2.04b3; the inventory files the
// range, 0x10006954..0x10008230, under tb_error.cpp): the renderer-independent half of
// the draw primitive. PARTIAL: construction only; the primitive queue, clipping, fog and
// sprite members are still to do.

// Set when the card is an NVIDIA one (vendor 0x10de) and the renderer is 1, which the Mac
// build never is. Engine data 0x10052b38, not exported; name invented.
int32_t gDrawPrimitiveNvidia = 0;

CDrawPrimitive::CDrawPrimitive() {
    mFields[0] = 0;
    mFields[4] = 0xffffffff;
    mFields[5] = 0;
    mFields[6] = 0;
    mFields[7] = 0;
    mFields[1] = 0;
    mFields[8] = 0;
    if (gei_DDraw->mDeviceInfo.vendorID != 0x10de) return;
    if (gei_System.mRenderer != 1) return;
    gDrawPrimitiveNvidia = 1;
}

CDrawPrimitive::~CDrawPrimitive() {}

// tb_drawprimitive_buffer.cpp (engine library of SimTheme Park 2.04b3; the original file is
// tb_drawprimitive.cpp by its own assertions, 0x10006954..0x10008230; the inventory files
// the range under tb_error.cpp): the draw primitive's buffer. A primitive is a record of
// the render memory's table. GetNextPrimitive makes one and fills in its header, the caller
// writes vertices and indices into the memory the record points at (AddClipPolyList clips
// and adds a triangle list), and ExecuteCurrentPrimitive finishes it (fog, projection,
// depth) and either hands it to the renderer or, for the sorted draw, orders its faces by
// depth. The constructor and destructor are in tb_drawprimitive.cpp, AddPoly_TLCTEX
// (0x100078c0) and DrawSprite in tb_drawprimitive_sprite.cpp; AddSprite is still to do.

static_assert(sizeof(SPrimitive) == sizeof(SRenderLoadInfo), "a primitive is a render memory record");
static_assert(offsetof(SPrimitive, mVertexData) == offsetof(SRenderLoadInfo, mData), "vertex pointer place");
static_assert(offsetof(SPrimitive, mIndexData) == offsetof(SRenderLoadInfo, mIndexData), "index pointer place");

namespace {

// gei_Display.mFlags: 0x2000 is the fog switch (CLBLobbyView::SetFogOn), 0x4000 lets the
// vertices be fogged one by one; the name of the second is not known.
constexpr uint32_t kDisplayFog = 0x2000;
constexpr uint32_t kDisplayVertexFog = 0x4000;
// gei_System.mFlags 0x4000000: GetNextPrimitive keeps a dynamic record's blending mode
// "current" only when it is clear; the name of the flag is not known.
constexpr uint32_t kSystemFlag04000000 = 0x4000000;
// The renderers the buffer knows (gei_System.mRenderer): 1 and 3 are the ones that fog and
// sort the faces; with 4 the primitives marked SPrimitive::kSortSkipped are left alone, the
// others reach the sorter's assertion.
constexpr int32_t kRendererNoSort = 4;
// The record ExecuteCurrentPrimitive asks the render memory for: dynamic, not remembered.
constexpr int32_t kScratchFlags = CRenderMemory::kDynamic | CRenderMemory::kKeepCurrent;
// Vertices a polygon of a clipped triangle can need, and the bytes the clip's work array
// holds: AddClipPolyList stops when less than this much room is left.
constexpr uintptr_t kVertexReserve = 0x140;   // ten vertices of 0x20 bytes
constexpr uintptr_t kIndexReserve = 0x18;     // four triangles
constexpr int32_t kClipVertexSize = 0x20;     // the stride the clipper is given (SFVF_TLCTEX)
constexpr size_t kCodeCount = 0x410;          // the stack array of outside codes

// The record LoadInfo hands out, as the primitive it is.
SPrimitive* AsPrimitive(SRenderLoadInfo* info) { return reinterpret_cast<SPrimitive*>(info); }

// The depth of vertex `number` of a strided block: the float at +8 of the vertex. The
// byte offset is a 32-bit product, as the original computes it.
float VertexDepth(const uint8_t* vertices, uint32_t number, int32_t stride) {
    const uint32_t offset = number * static_cast<uint32_t>(stride) + 8;
    float depth;
    std::memcpy(&depth, vertices + static_cast<int32_t>(offset), sizeof depth);
    return depth;
}

}  // namespace

// Makes the primitive's record: the pool `flags` names (a dynamic one that is going to be
// executed at once is not remembered for DuplicateCurrentPrimitive), its blending mode,
// type, vertex format and texture, and the room the render memory has left for its data.
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat,
                                             int32_t mode, void* texture) {
    if ((flags & CRenderMemory::kDynamic) != 0 && (static_cast<uint32_t>(mode) & InstantMask()) != 0 &&
        (gei_System.mFlags & kSystemFlag04000000) == 0) {
        flags |= CRenderMemory::kKeepCurrent;
    }
    mCurrent = AsPrimitive(gei_RenderMemory->LoadInfo(flags));
    if (mCurrent == nullptr) return nullptr;
    mCurrent->mBlendingMode = static_cast<uint32_t>(mode);
    mCurrent->mPrimitiveType = primitiveType;
    mCurrent->mVertexFormat = vertexFormat;
    mCurrent->mTexture = texture;
    mCurrent->mF34 = -1;
    SetVertexSize(SFVF_VertexSize(vertexFormat));
    mVertexEnd = gei_RenderMemory->GetVertexEndPosition();
    mIndexEnd = gei_RenderMemory->GetIndexEndPosition();
    return mCurrent;
}

// The same header on a copy of the remembered record (which keeps its vertex data); the
// room left is not looked up again.
SPrimitive* CDrawPrimitive::DuplicateCurrentPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat,
                                                      int32_t mode, void* texture) {
    mCurrent = AsPrimitive(gei_RenderMemory->LoadInfo_CopyVDB(flags));
    if (mCurrent == nullptr) return nullptr;
    mCurrent->mBlendingMode = static_cast<uint32_t>(mode);
    mCurrent->mPrimitiveType = primitiveType;
    mCurrent->mVertexFormat = vertexFormat;
    mCurrent->mTexture = texture;
    mCurrent->mF34 = -1;
    SetVertexSize(SFVF_VertexSize(vertexFormat));
    return mCurrent;
}

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

// An empty primitive (no vertices or no indices, as signed numbers) counts as drawn.
int32_t CDrawPrimitive::ExternExecute(SPrimitive* primitive) {
    tpw_probe::Hit(tpw_probe::kExternExecute);
    if (primitive->mVertexCount < 1 || primitive->mIndexCount < 1) return 1;
    return Execute(primitive);
}

// Quirks kept: the work array for the sort is the memory behind the record that
// LoadInfo(kDynamic | kKeepCurrent) hands out, from its offset 0x28 on (the record's own
// pointers and the records after it), a float for every face; the divide by the vertex
// count is not guarded (0 gives infinity or NaN); the original tests renderer 4 twice in a
// row (the second test can never pass); a renderer other than 1 and 3 asks the render memory
// for the work record, then asserts (tb_drawprimitive.cpp line 439) and skips the sort. The
// fog, projection and round-off steps and the "execute now" test each read the current
// primitive afresh, as the original does, so an Execute that replaces it is followed.
int32_t CDrawPrimitive::ExecuteCurrentPrimitive() {
    if ((mCurrent->mFlags & SPrimitive::kCheckFog) != 0) CheckFogOnCurrentPrimitive();
    const uint32_t flags = mCurrent->mFlags;
    if ((flags & SPrimitive::kProject) != 0) {
        ProjectCurrentPrimitive(static_cast<int32_t>(flags & SPrimitive::kTypeMask));
    }
    if ((mCurrent->mFlags & SPrimitive::kRoundOff) != 0) RoundOffPrimitive();
    mCurrent->mDepth = mCurrent->mDepth / static_cast<float>(mCurrent->mVertexCount);

    SPrimitive* primitive = mCurrent;
    const uint32_t mode = primitive->mBlendingMode;
    if ((mode & InstantMask()) != 0 || (primitive->mFlags & SPrimitive::kExecuteNow) != 0) {
        if (Execute(primitive) == 0) {
            mCurrent = nullptr;
            return 0;
        }
        mCurrent->mFlags |= SPrimitive::kDrawn;
    } else if (mode != SPrimitive::kNoDraw &&
               !(gei_System.mRenderer == kRendererNoSort && (mode & SPrimitive::kSortSkipped) != 0)) {
        SRenderLoadInfo* scratch = gei_RenderMemory->LoadInfo(kScratchFlags);
        const int32_t faces = primitive->mIndexCount / 3;
        const uint16_t* indices = primitive->mIndexData;
        const int32_t renderer = gei_System.mRenderer;
        if (renderer == 1 || renderer == 3) {
            uint8_t* workArea = reinterpret_cast<uint8_t*>(scratch) + offsetof(SRenderLoadInfo, mData);
            float* depths = reinterpret_cast<float*>(workArea);
            const uint8_t* vertices = primitive->mVertexData;
            const int32_t stride = VertexSize();
            for (int32_t face = 0; face < faces; ++face, indices += 3) {
                float sum = VertexDepth(vertices, indices[0], stride);
                sum = sum + VertexDepth(vertices, indices[1], stride);
                sum = sum + VertexDepth(vertices, indices[2], stride);
                depths[face] = sum;
            }
            QSortFaces_float(depths, primitive->mIndexData, 0, faces - 1);
        } else {
            msl_assertion_failed("0", "tb_drawprimitive.cpp", 0x1b7);
        }
    }
    mCurrent = nullptr;
    return 1;
}

int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    SPrimitive* primitive = mCurrent;
    if (primitive == nullptr || primitive->mVertexCount == 0 || primitive->mIndexCount == 0) return 1;
    if (depth != -1.0f) primitive->mDepth = depth;
    primitive = mCurrent;
    const uint32_t vertexBytes = static_cast<uint32_t>(primitive->mVertexCount) * static_cast<uint32_t>(VertexSize());
    const uint32_t indexBytes = static_cast<uint32_t>(primitive->mIndexCount) << 1;
    gei_RenderMemory->SaveInfo(
        reinterpret_cast<char*>(Moved(primitive->mVertexData, vertexBytes)),
        reinterpret_cast<uint16_t*>(Moved(primitive->mIndexData, indexBytes)));
    return ExecuteCurrentPrimitive();
}

int32_t CDrawPrimitive::ExecutePrimitiveIfInstant(SPrimitive* primitive) {
    if ((primitive->mBlendingMode & InstantMask()) != 0) {
        if (Execute(primitive) == 0) return 0;
        primitive->mFlags |= SPrimitive::kDrawn;
    }
    return 1;
}

// The triangles go to the start of the primitive's indices (its count is not added to the
// write place, only to the room test) and the vertices they use must be the first
// `vertexCount` of its vertex data. A triangle whose three vertices are outside one plane
// is dropped; one partly outside is clipped in place: the clipper rewrites the three
// numbers as a polygon of n vertices, which becomes a fan of n - 2 triangles (written from
// the last one back, the triangle's own three entries being the first), and fewer than
// three vertices take the triangle out again. The room is tested again before every
// triangle that is kept, and the result is the indices written so far, which is also what
// a full buffer returns.
// Not reproduced: the original adds and multiplies in 32 bits, so counts that carry an
// address across 4 GB wrap there and not here. A NULL current primitive returns 0 (the
// original reads low memory first, harmlessly, and returns 0 too).
int32_t CDrawPrimitive::AddClipPolyList(int32_t& vertexCount, uint16_t* indices, int32_t indexCount, int32_t mask) {
    SPrimitive* primitive = mCurrent;
    if (primitive == nullptr) return 0;
    const int32_t stride = VertexSize();
    const uintptr_t vertexEnd = reinterpret_cast<uintptr_t>(mVertexEnd);
    const uintptr_t indexEnd = reinterpret_cast<uintptr_t>(mIndexEnd);
    const uint32_t vertexTotal = static_cast<uint32_t>(primitive->mVertexCount + vertexCount);
    const uint32_t indexTotal = static_cast<uint32_t>(primitive->mIndexCount + indexCount);
    const uintptr_t vertexUsed = Moved(primitive->mVertexData, static_cast<uint32_t>(stride) * vertexTotal);
    const uintptr_t indexUsed = Moved(primitive->mIndexData, indexTotal * 2u);
    if (vertexEnd < vertexUsed || indexEnd < indexUsed) return 0;

    // The outside codes of the vertices (the original leaves the array unset beyond the
    // vertices the test fills in; an index naming one of those is not meaningful).
    uint8_t clip[4] = {0xff, 0, 0, 0};
    const uintptr_t vertexLimit = vertexEnd - kVertexReserve;
    const uintptr_t indexLimit = indexEnd - kIndexReserve;
    uint8_t codes[kCodeCount] = {};
    CClipping::CPP_TestData(clip, gei_Clipping, codes, primitive->mVertexData, vertexCount, kClipVertexSize, 0xff);
    if (clip[0] != 0) return 0;

    uint16_t* out = primitive->mIndexData;
    const uint32_t keep = ~static_cast<uint32_t>(mask);
    for (int32_t done = 0; done < indexCount; done += 3, indices += 3) {
        const uint32_t all = codes[indices[0]] & codes[indices[1]] & codes[indices[2]];
        if ((keep & all) != 0) continue;
        if (reinterpret_cast<uintptr_t>(out) > indexLimit ||
            Moved(primitive->mVertexData, static_cast<uint32_t>(vertexCount) << 5) > vertexLimit) {
            return static_cast<int32_t>(out - primitive->mIndexData);
        }
        out[0] = indices[0];
        out[1] = indices[1];
        out[2] = indices[2];
        out += 3;
        const uint32_t any = codes[indices[0]] | codes[indices[1]] | codes[indices[2]];
        const uint32_t planes = keep & any;
        if (planes == 0) continue;

        VertexClipList list;
        list.mCount = 3;
        list.mList = out - 3;
        SFVF_TLCTEX* vertices = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData);
        gei_Clipping->ClipPoly(static_cast<int32_t>(planes), vertices, &list, vertexCount, kClipVertexSize);
        const int32_t polygon = list.mCount;
        if (polygon > 3) {
            uint16_t* first = out - 3;
            for (int32_t triangle = polygon - 3; triangle >= 1; --triangle) {
                first[triangle * 3 + 2] = first[triangle + 2];
                first[triangle * 3 + 1] = first[triangle + 1];
                first[triangle * 3] = first[0];
            }
            out = first + (polygon - 2) * 3;
        } else {
            out -= 3 - polygon;
        }
    }
    return static_cast<int32_t>(out - primitive->mIndexData);
}

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

static_assert(sizeof(ClipCodeType) == 2, "the clip codes are two bytes");

namespace {

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

void CDrawPrimitive::ApplyFog(SFVF_TLCTEX* vertices, int32_t count) {
    if ((gei_Display.mFlags & kDisplayFog) == 0 || (gei_Display.mFlags & kDisplayVertexFog) == 0) return;
    const uint8_t fogged = gei_LightManager->CalcVertexFogIntensities(vertices, count, VertexSize());
    mCurrent->mBlendingMode &= ~SPrimitive::kFogCleared;
    if (fogged != 0) mCurrent->mBlendingMode |= SPrimitive::kFogged;
}

// Only the renderers that fog (1 and 3) work out the intensities.
void CDrawPrimitive::CheckFogOnCurrentPrimitive() {
    const int32_t renderer = gei_System.mRenderer;
    if (renderer != 2) {
        if (renderer < 2) {
            if (renderer < 1) return;
        } else if (renderer > 3) {
            return;
        }
        if ((gei_Display.mFlags & kDisplayVertexFog) != 0) {
            const uint8_t fogged = gei_LightManager->CalcVertexFogIntensities(
                reinterpret_cast<SFVF_TLCTEX*>(mCurrent->mVertexData), mCurrent->mVertexCount, VertexSize());
            mCurrent->mBlendingMode &= ~SPrimitive::kFogCleared;
            if (fogged != 0) mCurrent->mBlendingMode |= SPrimitive::kFogged;
        }
    }
}

// The depth is read after the projection runs.
void CDrawPrimitive::ProjectCurrentPrimitive(int32_t type) {
    const int32_t count = mCurrent->mVertexCount;
    if (count == 0) return;
    const float projected = gei_MathUnit->Project(mCurrent->mVertexData, VertexSize(), count, type);
    SPrimitive* primitive = mCurrent;
    primitive->mDepth = primitive->mDepth + projected;
}

void CDrawPrimitive::RoundOffPrimitive() {}

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

namespace {

constexpr uint32_t kVertexShift = 5;
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
