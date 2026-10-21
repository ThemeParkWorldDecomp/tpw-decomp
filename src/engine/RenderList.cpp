// RenderList.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Light.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_meshdelay.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include <cstddef>
#include <cstring>
#include <new>

// tb_resource_display.cpp (engine library of SimTheme Park 2.04b3): CDisplay and the
// render list's start-up. PARTIAL: CRenderList's members (0x100262e0..0x10026954) and
// the rest of the inventory's range are still to do; CRenderMemory, which the inventory
// files here, is in tb_rendermemory.cpp (the name its own assertions give).

// ---- The render list ------------------------------------------------------------------
CRenderList* gei_RenderList = nullptr;

int32_t RenderList_Init() {
    if (gei_RenderList != nullptr) return 0;
    gei_RenderList = new (std::nothrow) CRenderList;
    if (gei_RenderList == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t RenderList_Close() {
    if (gei_RenderList == nullptr) return 0;
    delete gei_RenderList;
    gei_RenderList = nullptr;
    return 1;
}

// tb_renderentry.cpp: SRenderListEntry::CalcRenderEntry (engine library of SimTheme Park 2.04b3,
// 0x1002477c, 5,352 bytes; the inventory files it under tb_resource_display.cpp, next to
// SetupPrimitive and the render list). CRenderList::RenderByTexture calls it for every face set
// of a mesh node once SetupPrimitive has started the primitive: the face set's vertices are
// copied into the primitive from the node's transformed positions, with their colours and
// texture coordinates; the faces that turn to the viewer become indices, clipped when the
// entry's flags say the node's box crosses a plane of the view; then the new vertices get their
// fog and projection and the primitive its depth.

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

// tb_mesh_frame.cpp: the mesh object's per-frame members (engine library of SimTheme Park
// 2.04b3, 0x1001db1c..0x1001e418; the inventory files the range under tb_profiler.cpp, the
// original name is not in the binary; "tb_mesh.cpp" is in StaticCalculateLights' assertion)
// and two small functions of the same library, ColorVertices (0x10003570) and DotProduct
// (0x10025cac).
//
// Prepare is what the map's lists call for every mesh in view (CMapWho::PrepareWorldspace,
// the global lists, CMeshDelay::Prepare): at most once per window pass, it walks the mesh's
// nodes with the view matrix, and every node in view is lit, added to the render list and has
// its textures asked for.

float DotProduct(D3DVECTOR a, D3DVECTOR b) {
    const float yy = a.y * b.y;
    return ppc_fmadds(a.z, b.z, ppc_fmadds(a.x, b.x, yy));
}

// tb_renderentry_setup.cpp: SRenderListEntry::GetFaceSetBlendMode (0x10025e8c, 352 bytes) and
// SetupPrimitive (0x10026044, 608 bytes), engine library of SimTheme Park 2.04b3 (the
// inventory files them under tb_resource_display.cpp, next to CalcRenderEntry and the render
// list). CRenderList::RenderByTexture asks the first for the blending mode that sorts a face
// set and calls the second to start the primitive CalcRenderEntry then fills: the render
// memory's next record, with the face set's texture page, blending mode and vertex format.

namespace {

// The node's flags.
constexpr uint32_t kNodeBlend2 = 0x00000002;          // blending bit 0x800
constexpr uint32_t kNodeInterface = 0x02000000;       // blending bits 0x5156 (the blend of an interface mesh)
constexpr uint32_t kNodeBlend3 = 0x08000000;          // blending bit 0x20000
constexpr uint32_t kNodeDynamic = 0x000c0000;         // either: the primitive comes from the render memory's dynamic pool

// The render list entry's flags (AddEntry's argument ored with gRenderEntryFlags).
constexpr uint32_t kEntryBlend1 = 0x10000000;         // blending bit 0x10000000
constexpr uint32_t kEntryBlend2 = 0x04000000;         // blending bits 0x30000000
constexpr uint32_t kEntryBlend3 = 0x08000000;         // blending bits 0x50000000
// The face set's flag byte 0: blending bit 0x800000.
constexpr uint8_t kFaceSetBlend = 0x02;

// The face set's texture record (sTexture::mF00): the texture is drawn with the plain blending
// mode instead of the texture entry's.
constexpr uint16_t kRecordPlainBlend = 0x0800;

// The texture entry's flags. kEntryStages with no mF38 is a texture with a second set of
// coordinates and further stages (the planes of the same image): CalcRenderEntry's case.
constexpr uint32_t kTextureMore = 0x00100000;         // blending bit 0x40000
constexpr uint32_t kTextureStages = 0x00000400;

// Blending mode bits (the renderer's SetBlendingMode takes them). The names say where each
// comes from; what the renderer does with most of them is not known (SPrimitive names two).
constexpr int32_t kBlendUntextured = 0x1256;          // a face set with no texture (RenderByTexture's too)
constexpr int32_t kBlendFog = 0x00100000;             // display flag kEnableFog
constexpr int32_t kBlendSpecular = 0x00080000;        // display flag kSpecularEnable: the specular second pass
constexpr int32_t kBlendNode2 = 0x00000800;
constexpr int32_t kBlendInterface = 0x00005156;
constexpr int32_t kBlendNode3 = 0x00020000;
constexpr int32_t kBlendFaceSet = 0x00800000;
constexpr int32_t kBlendEntry1 = 0x10000000;
constexpr int32_t kBlendEntry2 = 0x30000000;
constexpr int32_t kBlendEntry3 = 0x50000000;
constexpr int32_t kBlendTextureMore = 0x00040000;
constexpr int32_t kBlendTextureWrap = 0x00400000;
constexpr int32_t kBlendTextureStages = 0x00000080;

// The primitive's flags SetupPrimitive writes: the projection type 2 of an interface mesh
// (SPrimitive::kTypeMask holds the type), and 0x80 for a texture with further stages.
constexpr uint32_t kPrimitiveInterface = 2;
constexpr uint32_t kPrimitiveStages = 0x00000080;

// The primitive type SetupPrimitive asks for (a triangle list) and its two vertex formats:
// one set of texture coordinates, or two.
constexpr int32_t kTriangleList = 4;
constexpr uint32_t kFormatOneSet = 0x1c4;
constexpr uint32_t kFormatTwoSets = 0x2c4;

// Whether the entry is a texture with further stages: flag 0x400 and no mF38 (a set of
// coordinates for the second stage).
bool HasStages(const CTextureEntry* entry) {
    return entry != nullptr && (entry->mFlags & kTextureStages) != 0 && entry->mF38 == nullptr;
}

}  // namespace

// Bits of the blending mode, in the order the original ORs them: the display's fog and
// specular switches, the node's and the entry's flags, the face set's. A face set without a
// texture record, or without a texture entry, ends with 0x1256; otherwise the texture entry
// contributes its own blending mode (CTextureEntry::GetBlendingMode) unless the record says
// the plain one, then the entry's flag bits. QUIRK, kept: the original tests the entry for
// NULL three more times after it has answered for NULL (dead branches).
int32_t SRenderListEntry::GetFaceSetBlendMode(sFaceSet* faceSet, CTextureEntry* texture) {
    int32_t mode = 0;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) mode |= kBlendFog;
    if ((gei_Display.mFlags & CDisplay::kSpecularEnable) != 0) mode |= kBlendSpecular;
    const uint32_t nodeFlags = mNode->mFlags;
    if ((nodeFlags & kNodeBlend2) != 0) mode |= kBlendNode2;
    if ((nodeFlags & kNodeInterface) != 0) mode |= kBlendInterface;
    if ((nodeFlags & kNodeBlend3) != 0) mode |= kBlendNode3;
    if ((faceSet->mF0C[0] & kFaceSetBlend) != 0) mode |= kBlendFaceSet;
    if ((mFlags & kEntryBlend1) != 0) mode |= kBlendEntry1;
    if ((mFlags & kEntryBlend2) != 0) mode |= kBlendEntry2;
    if ((mFlags & kEntryBlend3) != 0) mode |= kBlendEntry3;
    const sTexture* const record = faceSet->mTexture.get();
    if (record == nullptr || texture == nullptr) return mode | kBlendUntextured;
    if ((record->mF00 & kRecordPlainBlend) == 0) {
        mode |= static_cast<int32_t>(texture->GetBlendingMode());
    } else {
        mode |= kBlendUntextured;
    }
    if ((texture->mFlags & kTextureMore) != 0) mode |= kBlendTextureMore;
    if ((texture->mFlags & CTextureEntry::kWrapAny) != 0) mode |= kBlendTextureWrap;
    if (HasStages(texture)) mode |= kBlendTextureStages;
    return mode;
}

// Starts the primitive for face set `faceSetNumber`: the texture entry is found as
// RenderByTexture found it (the record's frame with texture setting 0x40, else frame 0), the
// blending mode asked for before the primitive is made, and the primitive is the render
// memory's next one from the dynamic pool for a node with flag 0x40000 or 0x80000, else the
// static pool. Vertex format 0x2c4 (two sets of coordinates, 0x28 bytes a vertex) for a node
// with flag 0x2000000 or a texture with further stages, else 0x1c4 (0x20 bytes). The page is
// the entry's texture when it is on one. NULL (and *extra untouched) when the render memory
// has no record; otherwise the primitive gets the flags and mF34 below and *extra the vertex
// size. QUIRK, kept: the flags overwrite whatever the render memory's record held.
SPrimitive* SRenderListEntry::SetupPrimitive(int32_t faceSetNumber, int32_t* extra) {
    tag_sNode* const node = mNode;
    const int32_t pool = (node->mFlags & kNodeDynamic) != 0 ? CRenderMemory::kDynamic : CRenderMemory::kStatic;
    sFaceSet* const faceSet = node->mFaceSets.get() + faceSetNumber;
    sTexture* const record = faceSet->mTexture.get();
    CsTexture* const texture = record != nullptr ? record->mTexture.get() : nullptr;
    const bool framed = record != nullptr && (gei_TextureSettings.mFlags & kSettingsFrames) != 0;
    const int32_t frame = framed ? record->mF02 : 0;
    CTextureEntry* const entry = texture != nullptr ? texture->GetTextureEntry(frame) : nullptr;
    const bool stages = HasStages(entry);
    const uint32_t format = (node->mFlags & kNodeSecondTexture) != 0 || stages ? kFormatTwoSets : kFormatOneSet;
    void* const page = entry != nullptr ? entry->GetTexture() : nullptr;
    const int32_t mode = GetFaceSetBlendMode(faceSet, entry);
    SPrimitive* const primitive = gei_DrawPrimitive->GetNextPrimitive(pool, kTriangleList, format, mode, page);
    if (primitive == nullptr) return nullptr;
    primitive->mFlags = (mFlags & kEntryInterface) != 0 ? kPrimitiveInterface : 0;
    if (stages) primitive->mFlags |= kPrimitiveStages;
    primitive->mF34 = entry != nullptr ? entry->mF40 : -1;
    *extra = SFVF_VertexSize(format);
    return primitive;
}

// tb_renderlist.cpp: CRenderList's members (engine library of SimTheme Park 2.04b3; the
// inventory files the range 0x100262e0..0x10026954 under tb_resource_display.cpp). The list
// holds the nodes the world wants drawn this frame: AddEntry transforms a node's vertices
// at once, Render draws the face sets texture page by texture page.
//
// The work arrays of RenderByTexture are engine globals (not exported): the face sets
// waiting to be drawn at 0x100c562c and the tree of texture pages at 0x100b162c.

SRenderGroup gRenderGroups[0x1000];
SRenderFace gRenderFaces[0x8000];

namespace {

// The node's own flag: the entry's data is its point list, not its mesh words.
constexpr uint32_t kNodePoints = 0x01000000;
// The entry's flag: never drawn in sorted order.
constexpr uint32_t kEntryUnsorted = 0x02000000;
// What a face set without a texture entry is drawn with.
constexpr int32_t kUntexturedBlend = 0x1256;

// A face set's place in the tree: a new group for the page, filled with the face.
void StartGroup(SRenderGroup* group, uintptr_t page, SRenderFace* face) {
    group->mPage = page;
    group->mBelow = nullptr;
    group->mAbove = nullptr;
    if ((face->mSorted & 1) != 0) {
        group->mSorted = face;
        group->mUnsorted = nullptr;
    } else {
        group->mSorted = nullptr;
        group->mUnsorted = face;
    }
}

}  // namespace

int32_t CRenderList::AddEntry(CMatrix& view, CMatrix& unused, CLight** lights, tag_sNode* node, int32_t flags) {
    if (mCount >= kCapacity) {
        gei_ErrorHandler.Push(1, static_cast<int32_t>(0xb0754008));
        return 0;
    }
    if (node->mVertexCount != 0) {
        SRenderListEntry* entry = &mEntries[mCount];
        entry->mNode = node;
        entry->mLink = gei_MapWho->mF30;
        entry->mFlags = static_cast<uint32_t>(flags) | gRenderEntryFlags;
        uint8_t* vertices = gei_RenderMemory->GetVertexBufferPtr(node->mVertexCount);
        if (vertices != nullptr) {
            entry->mVertices = vertices;
            const int32_t count = node->mVertexCount;
            sVertexPosnSet* source = node->mVertices;
            if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x8c);
            gei_MathUnit->Transform(&view, vertices, source, 0x10, 0, count);
            // The Mac adds 4 to the 32-bit address.
            const uintptr_t data = (node->mFlags & kNodePoints) != 0
                                       ? reinterpret_cast<uintptr_t>(node->mPoints.get())
                                       : reinterpret_cast<uintptr_t>(node->mMeshWords.get()) + 4;
            entry->mData = reinterpret_cast<void*>(data);
            ++mCount;
        }
    }
    return 1;
}

void CRenderList::Reset() { mCount = 0; }

int32_t CRenderList::RenderByTexture() {
    const uint32_t instantMask = gei_DrawPrimitive->InstantMask();
    SRenderFace* const faces = gRenderFaces;
    SRenderFace* nextFace = faces;
    SRenderGroup* const groups = gRenderGroups;
    SRenderGroup* nextGroup = groups;

    // Every face set of every entry, last first, goes into its texture page's group.
    for (int32_t i = mCount; i-- > 0;) {
        SRenderListEntry* entry = &mEntries[i];
        tag_sNode* node = entry->mNode;
        const uint32_t faceSetCount = node->mFaceSetCount;
        // (Computed before the loop even when there are no face sets.)
        sFaceSet* faceSet = node->mFaceSets.get() + (static_cast<int32_t>(faceSetCount) - 1);
        for (uint32_t n = faceSetCount; n-- > 0; --faceSet) {
            sTexture* textureRecord = faceSet->mTexture.get();
            CsTexture* texture = textureRecord != nullptr ? textureRecord->mTexture.get() : nullptr;
            const bool framed = textureRecord != nullptr && (gei_TextureSettings.mFlags & kSettingsFrames) != 0;
            const int32_t frame = framed ? textureRecord->mF02 : 0;
            CTextureEntry* textureEntry = texture != nullptr ? texture->GetTextureEntry(frame) : nullptr;
            SRenderFace* face = nextFace;
            face->mTexture = textureEntry;
            uintptr_t page;
            if (textureEntry == nullptr) {
                face->mBlendMode = kUntexturedBlend;
                page = 0;
                face->mSorted = 1;
            } else {
                page = reinterpret_cast<uintptr_t>(textureEntry->GetTexture());
                face->mBlendMode = entry->GetFaceSetBlendMode(faceSet, textureEntry);
                face->mSorted = (static_cast<uint32_t>(face->mBlendMode) & instantMask) != 0 ? 1 : 0;
                if ((entry->mFlags & kEntryUnsorted) != 0) face->mSorted = 0;
            }
            face->mEntry = entry;
            face->mFaceSet = static_cast<uint8_t>(n);
            face->mNext = nullptr;
            if (face == faces) {
                StartGroup(nextGroup, page, face);
                ++nextGroup;
            } else {
                SRenderGroup* group = groups;
                bool placed = false;
                while (!placed) {
                    if (group->mPage == page) {
                        if ((face->mSorted & 1) != 0) {
                            // Blended faces keep to blending mode order, then texture entry order.
                            SRenderFace* before = nullptr;
                            SRenderFace* after = group->mSorted;
                            while (after != nullptr && after->mBlendMode < face->mBlendMode) {
                                before = after;
                                after = after->mNext;
                            }
                            while (after != nullptr && after->mBlendMode == face->mBlendMode &&
                                   reinterpret_cast<uintptr_t>(after->mTexture) <
                                       reinterpret_cast<uintptr_t>(face->mTexture)) {
                                before = after;
                                after = after->mNext;
                            }
                            face->mNext = after;
                            if (before != nullptr) {
                                before->mNext = face;
                            } else {
                                group->mSorted = face;
                            }
                        } else {
                            if (group->mUnsorted != nullptr) face->mNext = group->mUnsorted;
                            group->mUnsorted = face;
                        }
                        placed = true;
                    } else if (group->mPage < page) {
                        if (group->mAbove == nullptr) {
                            StartGroup(nextGroup, page, face);
                            group->mAbove = nextGroup;
                            placed = true;
                            ++nextGroup;
                        } else {
                            group = group->mAbove;
                        }
                    } else {
                        if (group->mBelow == nullptr) {
                            StartGroup(nextGroup, page, face);
                            group->mBelow = nextGroup;
                            placed = true;
                            ++nextGroup;
                        } else {
                            group = group->mBelow;
                        }
                    }
                }
            }
            ++nextFace;
        }
    }

    // Then every group, in the order they were made: the sorted faces first, a primitive
    // per run of the same blending mode, then the others one by one.
    int32_t extra = 0;   // (stack garbage until SetupPrimitive has set it, which comes first)
    for (SRenderGroup* group = groups; group != nextGroup; ++group) {
        SPrimitive* primitive = nullptr;
        for (SRenderFace* face = group->mSorted; face != nullptr; face = face->mNext) {
            SRenderListEntry* entry = face->mEntry;
            if (primitive == nullptr) {
                primitive = entry->SetupPrimitive(face->mFaceSet, &extra);
                // Quirk: a face set with nothing to draw ends the whole list, successfully.
                if (primitive == nullptr) return 1;
            }
            if (entry->CalcRenderEntry(primitive, face->mFaceSet, extra) == 0) return 0;
            if (face->mNext == nullptr || face->mNext->mBlendMode != face->mBlendMode) {
                if (gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f) == 0) return 0;
                primitive = nullptr;
            }
        }
        for (SRenderFace* face = group->mUnsorted; face != nullptr; face = face->mNext) {
            SRenderListEntry* entry = face->mEntry;
            primitive = entry->SetupPrimitive(face->mFaceSet, &extra);
            if (primitive == nullptr) return 0;
            if (entry->CalcRenderEntry(primitive, face->mFaceSet, extra) == 0) return 0;
            if (gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f) == 0) return 0;
        }
    }
    return 1;
}

int32_t CRenderList::Render() {
    if (RenderByTexture() == 0) return 0;
    if (gei_RenderMemory->ExecuteUnrenderedPrimitives() == 0) return 0;
    return 1;
}
