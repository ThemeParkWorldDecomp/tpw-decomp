#include "tpw_probe.h"
// tb_renderentry.cpp: SRenderListEntry::CalcRenderEntry (engine library of SimTheme Park 2.04b3,
// 0x1002477c, 5,352 bytes; the inventory files it under tb_resource_display.cpp, next to
// SetupPrimitive and the render list). CRenderList::RenderByTexture calls it for every face set
// of a mesh node once SetupPrimitive has started the primitive: the face set's vertices are
// copied into the primitive from the node's transformed positions, with their colours and
// texture coordinates; the faces that turn to the viewer become indices, clipped when the
// entry's flags say the node's box crosses a plane of the view; then the new vertices get their
// fog and projection and the primitive its depth.
#include "tb_resource_display.h"

#include <cstring>

#include "MapWho.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_printf_opengl.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

namespace {

// Texture settings flag 0x40: a face set's texture record names the frame to draw.
constexpr uint32_t kSettingsFrames = 0x00000040;
// The node's flag: a second set of texture coordinates, for the node's own texture record (mF98).
constexpr uint32_t kNodeSecondTexture = 0x02000000;
// Texture entry flags. kEntryEnvironment (CTextureLoadManager::LoadFile gives it to a texture
// loaded with load flag 4, that is a texture record with flag 0x2000): the coordinates come
// from the normals. kEntryStages with no mF38: a second set of coordinates (SetupPrimitive picks
// vertex format 0x2c4) and the pages of the next entries as the further stages.
// kEntryRoundOff: the primitive gets SPrimitive::kRoundOff.
constexpr uint32_t kEntryEnvironment = 0x08000000;
constexpr uint32_t kEntryRoundOff = 0x04000000;
constexpr uint32_t kEntryStages = 0x00000400;
// The render list entry's flags: the outside codes of the planes the node's box crosses
// (CClipping's six bits), and 0x2000000 (CMesh::Prepare's interface meshes, which the render
// list never sorts) for projection type 2 and depth 0.
constexpr uint32_t kEntryClipPlanes = 0x0000003f;
constexpr uint32_t kEntryInterface = 0x02000000;
// The primitive's blending mode bit for a depth worked out from the vertex count.
constexpr uint32_t kBlendCountDepth = 0x00800000;

// A pair of texture coordinates inside a vertex: the first at 0x18 (SFVF_TLCTEX's u and v), the
// second at 0x20 of a vertex of format 0x2c4.
struct STexCoord {
    float u, v;
};

STexCoord* CoordAt(uint8_t* vertex) { return reinterpret_cast<STexCoord*>(vertex); }

// Vertex `number` of the primitive's vertex memory (a 32-bit product, as the Mac's).
const SFVF_TLCTEX& VertexAt(uint8_t* data, int32_t number, int32_t stride) {
    return *reinterpret_cast<const SFVF_TLCTEX*>(data + number * stride);
}

// An address plus a 32-bit byte count, for the room tests: the Mac's sum wraps at 4 GB, the
// host's does not.
uintptr_t AddressPlus(const void* address, int32_t bytes) {
    return reinterpret_cast<uintptr_t>(address) + static_cast<uintptr_t>(static_cast<intptr_t>(bytes));
}

// The texture coordinates of `count` vertices `stride` bytes apart from `at` (the place of one
// pair in the first vertex): the node's coordinate sets from vertex `first` on, scaled into the
// entry's place on its page; zero when there is no entry or it is not on a page. With
// `environment` they are first worked out from the vertices' normals and rows 0 and 1 of the
// view matrix (a sphere map: u = 0.5 + 0.5 row0.n, v = 0.5 - 0.5 row1.n). QUIRK, kept: that loop
// counts `count` down past zero, so the ordinary loop after it, which the original runs in every
// case, starts at the vertex after the face set with a count of 0xffffffff and writes on until
// the memory ends (the original faults there; so does this).
void SetTextureCoordinates(uint8_t* at, int32_t stride, uint32_t count, uint32_t first, tag_sNode* node,
                           CTextureEntry* entry, bool environment) {
    if (entry == nullptr || entry->GetTexture() == nullptr) {
        for (; count != 0; --count) {
            CoordAt(at)->u = 0.0f;
            CoordAt(at)->v = 0.0f;
            at += stride;
        }
        return;
    }
    const float uOffset = entry->GetUOffset();
    const float vOffset = entry->GetVOffset();
    const float uSize = entry->GetUSize();
    const float vSize = entry->GetVSize();
    if (environment) {
        const s3DPoint* normals = node->mPoints.get();
        const D3DVECTOR row0 = {gei_ViewMatrix->Return(0, 0), gei_ViewMatrix->Return(0, 1), gei_ViewMatrix->Return(0, 2)};
        const D3DVECTOR row1 = {gei_ViewMatrix->Return(1, 0), gei_ViewMatrix->Return(1, 1), gei_ViewMatrix->Return(1, 2)};
        uint32_t vertex = first;
        while (count-- != 0) {
            const s3DPoint& point = normals[node->mF94[vertex]];
            ++vertex;
            const D3DVECTOR normal = {point.x, point.y, point.z};
            CoordAt(at)->u = ppc_fmadds(uSize, ppc_fmadds(0.5f, DotProduct(row0, normal), 0.5f), uOffset);
            CoordAt(at)->v = ppc_fmadds(vSize, ppc_fnmsubs(0.5f, DotProduct(row1, normal), 0.5f), vOffset);
            at += stride;
        }
    }
    // Vertex i's coordinates are lane i & 3 of set i >> 2 (u in the first four words, v in the last).
    const sUVSet* set = node->mUVs.get() + (first >> 2);
    uint32_t lane = first & 3;
    for (; count != 0; --count) {
        const float* source = &set->mF00[lane];
        if (++lane == 4) {
            lane = 0;
            ++set;
        }
        CoordAt(at)->u = ppc_fmadds(uSize, source[0], uOffset);
        CoordAt(at)->v = ppc_fmadds(source[4], vSize, vOffset);
        at += stride;
    }
}

}  // namespace

int32_t SRenderListEntry::CalcRenderEntry(SPrimitive* primitive, int32_t faceSet, int32_t extra) {
    tpw_probe::Hit(tpw_probe::kCalcRenderEntry);
    const int32_t stride = extra;
    CDrawPrimitive* const draw = gei_DrawPrimitive;
    const SPrimitive* const current = draw->mCurrent;
    tag_sNode* const node = mNode;
    const int32_t vertexCount = primitive->mVertexCount;
    // Room in the current primitive's memory for the counts of both primitives and the whole node
    // (nothing is done without it, successfully).
    const uintptr_t vertexEnd =
        AddressPlus(current->mVertexData, draw->VertexSize() * (vertexCount + node->mF5E + current->mVertexCount));
    const uintptr_t indexEnd =
        AddressPlus(current->mIndexData, 2 * (primitive->mIndexCount + node->mF5C * 3 + current->mIndexCount));
    if (reinterpret_cast<uintptr_t>(draw->mVertexEnd) < vertexEnd ||
        reinterpret_cast<uintptr_t>(draw->mIndexEnd) < indexEnd) {
        return 1;
    }

    const sFaceSet& set = node->mFaceSets[faceSet];
    uint8_t* const vertices = primitive->mVertexData + stride * vertexCount;
    sTexture* const record = set.mTexture.get();
    CsTexture* const texture = record != nullptr ? record->mTexture.get() : nullptr;
    const bool framed = record != nullptr && (gei_TextureSettings.mFlags & kSettingsFrames) != 0;
    const int32_t frame = framed ? record->mF02 : 0;
    CTextureEntry* const entry = texture != nullptr ? texture->GetTextureEntry(frame) : nullptr;
    const bool environment = entry != nullptr && (entry->mFlags & kEntryEnvironment) != 0;

    // The face set's vertices are mF08 to mF0A of the node.
    const uint32_t first = set.mF08;
    const uint32_t count = static_cast<uint32_t>(set.mF0A) - first + 1;
    SetTextureCoordinates(vertices + 0x18, stride, count, first, node, entry, environment);

    if ((node->mFlags & kNodeSecondTexture) != 0) {
        sTexture* const record2 = node->mF98.get();
        CsTexture* const texture2 = record2 != nullptr ? record2->mTexture.get() : nullptr;
        CTextureEntry* const entry2 = texture2 != nullptr ? texture2->GetTextureEntry(0) : nullptr;
        // The original reads this entry's flags before it knows there is one (address 8 when
        // there is none); what it reads only counts when there is an entry.
        const bool environment2 = entry2 != nullptr && (entry2->mFlags & kEntryEnvironment) != 0;
        SetTextureCoordinates(vertices + 0x20, stride, count, first, node, entry2, environment2);
        primitive->mTexture1 = entry2 != nullptr ? entry2->GetTexture() : nullptr;
    } else if (entry != nullptr && (entry->mFlags & kEntryStages) != 0 && entry->mF38 == nullptr) {
        SetTextureCoordinates(vertices + 0x20, stride, count, first, node, entry, false);
        // The further stages are the entries after this one, the planes the texture's load made
        // of the same image (each but the last has CTextureEntry::kNextIsPlane).
        primitive->mTexture1 = entry[1].GetTexture();
        if ((entry[1].mFlags & CTextureEntry::kNextIsPlane) != 0) primitive->mTexture2 = entry[2].GetTexture();
    }

    // Positions from the node's transformed vertices (0x10 bytes each, by the node's index
    // words), colours from the entry's data ANDed with the light manager's mask, or the mask
    // alone for a face set with flag 1 or a node without colours.
    const uint16_t* const positions = node->mF94.get();
    const uint32_t mask = static_cast<uint32_t>(gei_LightManager->mF04);
    uint8_t* vertex = vertices;
    if ((set.mF0C[0] & 1) == 0 && mData != nullptr) {
        const uint32_t* const colours = static_cast<const uint32_t*>(mData) + first;
        for (uint32_t i = 0; i != count; ++i) {
            std::memcpy(vertex, mVertices + 0x10 * positions[first + i], 0x10);
            reinterpret_cast<SFVF_TLCTEX*>(vertex)->colour = colours[i] & mask;
            vertex += stride;
        }
    } else {
        for (uint32_t i = 0; i != count; ++i) {
            std::memcpy(vertex, mVertices + 0x10 * positions[first + i], 0x10);
            reinterpret_cast<SFVF_TLCTEX*>(vertex)->colour = mask;
            vertex += stride;
        }
    }
    // A texture that is not on a page: the vertices take its average colour (white unless it
    // is a compressed image).
    if (entry != nullptr && entry->GetTexture() == nullptr) {
        const uint32_t colour = (entry->mFlags & CTextureEntry::kCompressed) != 0
                                    ? static_cast<CCompBase*>(entry->mData)->AverageColor()
                                    : 0xffffffffu;
        ColorVertices(reinterpret_cast<SFVF_TLCTEX*>(vertices), colour, static_cast<int32_t>(count), stride);
    }

    // The faces, as numbers of the primitive's vertices.
    const int32_t oldCount = primitive->mVertexCount;
    const sFace* face = node->mFaces.get() + set.mFirstFace;
    uint32_t faces = set.mFaceCount;
    uint16_t* index = primitive->mIndexData + primitive->mIndexCount;
    primitive->mVertexCount = oldCount + static_cast<int32_t>(count);
    if ((mFlags & kEntryClipPlanes) != 0) {
        // The faces stop once less than ten vertices' room or 0x30 bytes of indices are left.
        const uintptr_t vertexLimit = AddressPlus(draw->mVertexEnd, -(stride * 10));
        const uintptr_t indexLimit = reinterpret_cast<uintptr_t>(draw->mIndexEnd) - 0x30;
        uint8_t result[4];
        uint8_t codes[0x2000] = {};
        CClipping::CPP_TestData(result, gei_Clipping, codes, vertices, static_cast<int32_t>(count), stride, mFlags);
        if (result[0] != 0) {   // every vertex is outside one plane
            primitive->mVertexCount = oldCount;
            return 1;
        }
        while (faces-- != 0) {
            const int32_t a = face->mF02[0] - static_cast<int32_t>(first);
            const int32_t b = face->mF02[1] - static_cast<int32_t>(first);
            const int32_t c = face->mF02[2] - static_cast<int32_t>(first);
            if ((codes[c] & codes[a] & codes[b]) == 0) {
                const int32_t va = a + oldCount;
                const int32_t vb = b + oldCount;
                const int32_t vc = c + oldCount;
                uint8_t* const data = primitive->mVertexData;
                if (SFVF_FacesViewer(VertexAt(data, va, stride), VertexAt(data, vb, stride), VertexAt(data, vc, stride))) {
                    index[0] = static_cast<uint16_t>(va);
                    index[1] = static_cast<uint16_t>(vb);
                    index[2] = static_cast<uint16_t>(vc);
                    index += 3;
                    const int32_t planes = codes[a] | codes[b] | codes[c];
                    if (planes != 0) {
                        index = gei_Clipping->ClipPoly(planes, index, reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData),
                                                       primitive->mVertexCount, stride);
                    }
                }
            }
            ++face;
            if (vertexLimit < AddressPlus(primitive->mVertexData, stride * primitive->mVertexCount) ||
                indexLimit < reinterpret_cast<uintptr_t>(index)) {
                faces = 0;
            }
        }
        if (index == primitive->mIndexData) {   // nothing to draw: the vertices are given back
            primitive->mVertexCount = oldCount;
            return 1;
        }
        primitive->mIndexCount = static_cast<int32_t>(index - primitive->mIndexData);
    } else {
        const int32_t shift = static_cast<int32_t>(first) - oldCount;
        for (; faces != 0; --faces) {
            const int32_t va = face->mF02[0] - shift;
            const int32_t vb = face->mF02[1] - shift;
            const int32_t vc = face->mF02[2] - shift;
            uint8_t* const data = primitive->mVertexData;
            if (SFVF_FacesViewer(VertexAt(data, va, stride), VertexAt(data, vb, stride), VertexAt(data, vc, stride))) {
                index[0] = static_cast<uint16_t>(va);
                index[1] = static_cast<uint16_t>(vb);
                index[2] = static_cast<uint16_t>(vc);
                index += 3;
            }
            ++face;
        }
        primitive->mIndexCount = static_cast<int32_t>(index - primitive->mIndexData);
    }

    // Fog and projection of the new vertices (the count is read again after each call); the
    // depth they add, or, for a blending mode with 0x800000, one from their count.
    const int32_t projection = (mFlags & kEntryInterface) != 0 ? 2 : 0;
    gei_DrawPrimitive->ApplyFog(reinterpret_cast<SFVF_TLCTEX*>(vertices), primitive->mVertexCount - oldCount);
    const float depth = gei_MathUnit->Project(vertices, stride, primitive->mVertexCount - oldCount, projection);
    primitive->mDepth = primitive->mDepth + depth;
    if ((primitive->mBlendingMode & kBlendCountDepth) != 0) {
        primitive->mDepth =
            projection == 2
                ? 0.0f
                : ppc_fmadds(gei_Display.mZ54, static_cast<float>(primitive->mVertexCount - oldCount), 0.01f);
    }
    if (entry != nullptr && (entry->mFlags & kEntryRoundOff) != 0) primitive->mFlags |= SPrimitive::kRoundOff;
    return 1;
}
