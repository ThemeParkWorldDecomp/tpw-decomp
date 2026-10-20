// tb_renderentry_setup.cpp: SRenderListEntry::GetFaceSetBlendMode (0x10025e8c, 352 bytes) and
// SetupPrimitive (0x10026044, 608 bytes), engine library of SimTheme Park 2.04b3 (the
// inventory files them under tb_resource_display.cpp, next to CalcRenderEntry and the render
// list). CRenderList::RenderByTexture asks the first for the blending mode that sorts a face
// set and calls the second to start the primitive CalcRenderEntry then fills: the render
// memory's next record, with the face set's texture page, blending mode and vertex format.
#include "tb_resource_display.h"

#include "m3d2.h"
#include "tb_drawprimitive.h"
#include "tb_loadmanager.h"
#include "tb_rendermemory.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

namespace {

// Texture settings flag 0x40: a face set's texture record names the frame to draw.
constexpr uint32_t kSettingsFrames = 0x00000040;

// The node's flags.
constexpr uint32_t kNodeBlend2 = 0x00000002;          // blending bit 0x800
constexpr uint32_t kNodeInterface = 0x02000000;       // blending bits 0x5156 (the blend of an interface mesh)
constexpr uint32_t kNodeSecondTexture = 0x02000000;   // the node has a second set of texture coordinates (same bit)
constexpr uint32_t kNodeBlend3 = 0x08000000;          // blending bit 0x20000
constexpr uint32_t kNodeDynamic = 0x000c0000;         // either: the primitive comes from the render memory's dynamic pool

// The render list entry's flags (AddEntry's argument ored with gRenderEntryFlags).
constexpr uint32_t kEntryBlend1 = 0x10000000;         // blending bit 0x10000000
constexpr uint32_t kEntryBlend2 = 0x04000000;         // blending bits 0x30000000
constexpr uint32_t kEntryBlend3 = 0x08000000;         // blending bits 0x50000000
constexpr uint32_t kEntryInterface = 0x02000000;      // CMesh::Prepare's interface meshes: projection type 2

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
