#include "tpw_probe.h"
// tb_drawprimitive_buffer.cpp (engine library of SimTheme Park 2.04b3; the original file is
// tb_drawprimitive.cpp by its own assertions, 0x10006954..0x10008230; the inventory files
// the range under tb_error.cpp): the draw primitive's buffer. A primitive is a record of
// the render memory's table. GetNextPrimitive makes one and fills in its header, the caller
// writes vertices and indices into the memory the record points at (AddClipPolyList clips
// and adds a triangle list), and ExecuteCurrentPrimitive finishes it (fog, projection,
// depth) and either hands it to the renderer or, for the sorted draw, orders its faces by
// depth. The constructor and destructor are in tb_drawprimitive.cpp, AddPoly_TLCTEX
// (0x100078c0) and DrawSprite in tb_drawprimitive_sprite.cpp; AddSprite is still to do.
#include "tb_drawprimitive.h"

#include <cstddef>
#include <cstring>

#include "MapWho.h"
#include "platform/msl.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"

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

// `pointer` moved by a byte count the Mac computes in 32 bits (a negative one goes back).
uintptr_t Moved(const void* pointer, uint32_t bytes) {
    return reinterpret_cast<uintptr_t>(pointer) + static_cast<intptr_t>(static_cast<int32_t>(bytes));
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
