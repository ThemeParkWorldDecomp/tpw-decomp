#include "tpw_probe.h"
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
#include "tb_mesh.h"

#include <cstddef>
#include <cstring>

#include "Light.h"
#include "MapWho.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_meshdelay.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// Names invented from what the flags do.
// Header flags (tag_sM3D2Header::mFlags):
constexpr uint32_t kHeaderInterface = 0x00000200;   // drawn with its own two matrices, never sorted
constexpr uint32_t kHeaderLate = 0x00200000;        // prepared after the rest of the frame (CMeshDelay)
constexpr uint32_t kHeaderMonoLights = 0x00800000;  // with renderer 4 the lights see one colour mask
// Mesh flags (CMesh::mFlags):
constexpr uint32_t kMeshLate = 0x10;                // CMeshDelay::Prepare is calling: do it now
constexpr uint32_t kMeshHidden = 0x04;              // Prepare does nothing
constexpr uint32_t kMeshOwnMatrix = 0x01;           // SetLocalMatrix filled mMatrix
// Node flags (tag_sNode::mFlags):
constexpr uint32_t kNodeSkipped = 0x00008210;       // never drawn
constexpr uint32_t kNodeLitAgain = 0x000c0000;      // drawn and lit again whatever changed
constexpr uint32_t kNodeInArea = 0x00002000;        // test the node's area against the map first
constexpr uint32_t kNodeAreaFromDisplay = 0x04000000;   // ... against the display's rectangle (not the height field)
constexpr uint32_t kNodeLeaf = 0x00000020;          // its children are not walked
constexpr uint32_t kNodeNoLights = 0x01000000;      // no vertex colours to work out
constexpr uint32_t kNodeExtraTexture = 0x02000000;  // mF98 is one more texture record
// Render entry flags (gRenderEntryFlags and AddEntry's flags): see CRenderList::AddEntry.
constexpr uint32_t kEntryUnsorted = 0x02000000;     // also what StaticCalculateLights tests: no global lights
constexpr uint32_t kEntryGlobalList = 0x01000000;   // the mesh is in global list 6
// CheckBoundingBox answers 0x40 ored with the outside codes when the box is in view.
constexpr uint32_t kInView = 0x40;
// System flags that make everything in view be lit and added again this frame.
constexpr uint32_t kSceneChanged = CSystem::kViewChanged | CSystem::kLightsChanged;
// Display flags:
constexpr uint32_t kDisplayNoLights = CDisplay::kFlag800;   // no lights prepared for meshes
constexpr uint32_t kDisplayNoMeshLights = 0x00001000;       // the mesh's own lights are not added
// The renderer for which a header with kHeaderMonoLights changes the light mask, and that mask.
constexpr int32_t kMonoRenderer = 4;
constexpr int32_t kMonoLightMask = static_cast<int32_t>(0xff0000ff);
// The mesh's lights (33 words at 0x98) as the CLight** the engine functions take. The words
// are 32-bit and nothing stores a light in them yet; the translation of AddLights will need
// CMesh to keep CLight pointers (include/MapWho.h).
CLight** LightsOf(CMesh* mesh) { return reinterpret_cast<CLight**>(mesh->mF98); }

}  // namespace

int32_t CMesh::Prepare() {
    tpw_probe::Hit(tpw_probe::kMeshPrepare);
    const int32_t frame = gei_Display.mFrameCount;
    const int32_t pass = gei_Display.mF84;
    // Once per window pass (the display's frame count and its count of windows drawn).
    if (mF90 == frame && mF94 == pass) return 1;
    if ((mHeader->mFlags & kHeaderLate) != 0 && (mFlags & kMeshLate) == 0) {
        // Put off until the rest of the frame is prepared: CMeshDelay::Prepare comes back with
        // kMeshLate set. A full list drops the mesh for this pass.
        if (gMeshDelay.mCount < 0x100) {
            gMeshDelay.mMeshes[gMeshDelay.mCount] = this;
            ++gMeshDelay.mCount;
        }
        return 1;
    }
    mFlags &= ~kMeshLate;
    // The callback runs once per frame, before the stamp is renewed.
    if (mCallback != nullptr && mF90 != frame) mCallback(mHeader);
    mF90 = frame;
    mF94 = pass;
    mF14C = 0;
    if ((mFlags & kMeshHidden) != 0) return 1;
    if ((mHeader->mFlags & kHeaderInterface) != 0) {
        gRenderEntryFlags |= kEntryUnsorted;
        return Preprocess(mHeader->mF78, &mMatrix, &mMatrix2);
    }
    gRenderEntryFlags &= ~kEntryUnsorted;
    // The view matrix, with the mesh's own matrix in front when it has one.
    CMatrix world = *gei_FinalMatrix;
    if ((mFlags & kMeshOwnMatrix) != 0) world = mMatrix * world;
    return Preprocess(mHeader->mF78, &world, &gei_LightMatrix);
}

int32_t CMesh::Preprocess(tag_sNode* node, CMatrix* matrix, CMatrix* matrix2) {
    CMatrix first;    // the node's matrix through `matrix`
    CMatrix second;   // and through `matrix2`
    for (; node != nullptr; node = node->mF08) {
        gei_MathUnit->MultMatrix(&node->mMatrix.m[0][0], &matrix->m[0][0], &first.m[0][0]);
        gei_MathUnit->MultMatrix(&node->mMatrix.m[0][0], &matrix2->m[0][0], &second.m[0][0]);
        const uint32_t flags = node->mFlags;
        // Whether the node is a candidate: not skipped, and either flagged to be done again
        // or the view or the lights changed.
        bool candidate = (flags & kNodeSkipped) == 0 &&
                         ((flags & kNodeLitAgain) != 0 || (gei_System.mFlags & kSceneChanged) != 0);
        if (candidate && (flags & kNodeInArea) != 0) {
            // The node's rows and columns (last row, first row, last column, first column, signed
            // bytes at 0x90).
            int8_t* area = reinterpret_cast<int8_t*>(node->mF90);
            if ((flags & kNodeAreaFromDisplay) != 0) {
                // They overlap the display's rectangle (the camera's surroundings).
                candidate = gei_Display.mF928 <= area[2] && gei_Display.mF930 <= area[0] &&
                            gei_Display.mF92C >= area[3] && gei_Display.mF934 >= area[1];
            } else {
                candidate = gei_HField->CheckGamutArea(area) != 0;
            }
        }
        if (candidate) {
            const uint32_t outside = gei_Clipping->CheckBoundingBox(node->mBox, first);
            if ((outside & kInView) != 0) {
                if (!StaticCalculateLights(node, second, LightsOf(this),
                                           static_cast<int32_t>(outside | gRenderEntryFlags))) {
                    return 0;
                }
                if (!gei_RenderList->AddEntry(first, second, LightsOf(this), node,
                                              static_cast<int32_t>(mF14C | (outside | gRenderEntryFlags)))) {
                    return 0;
                }
                PreprocessTextures(node);
            }
        }
        // The children, with this node's matrices.
        tag_sNode* child = node->mF0C;
        if ((node->mFlags & kNodeLeaf) == 0 && child != nullptr) {
            if (!Preprocess(child, &first, &second)) return 0;
        }
    }
    return 1;
}

void CMesh::PreprocessTextures(tag_sNode* node) {
    // The depth of the node's box (CheckBoundingBox, just before), never below 0 (a NaN gives 0).
    float depth;
    std::memcpy(&depth, &gei_Clipping->mMinZ, sizeof depth);
    const float distance = depth > 0.0f ? depth : 0.0f;
    for (int32_t i = 0; i < node->mFaceSetCount; ++i) {
        sTexture* record = node->mFaceSets[i].mTexture;
        if (record == nullptr) continue;
        CsTexture* texture = record->mTexture;
        if (texture == nullptr) { tpw_probe::Hit(tpw_probe::kMeshTexNull); continue; }
        // Texture setting 0x40: the record names the frame to ask for.
        const int32_t frame = (gei_TextureSettings.mFlags & 0x40) != 0 ? record->mF02 : 0;
        CTextureEntry* entry = texture->GetTextureEntry(frame);
        if (entry == nullptr) { tpw_probe::Hit(tpw_probe::kMeshTexNull); continue; }
        if ((node->mFlags & kNodeLitAgain) == 0) entry->Lock();
        // Record flags 0x80 and 0x400: the entry is taken off its page and the second flag goes.
        if ((record->mF00 & 0x80) != 0 && (record->mF00 & 0x400) != 0) {
            gei_TCache->Remove(entry);
            record->mF00 = static_cast<uint16_t>(record->mF00 & ~0x400);
        }
        gei_TCache->Prepare(entry, distance);
    }
    if ((node->mFlags & kNodeExtraTexture) != 0) {
        // One more record, asked for even when it has no entry (Prepare then gets NULL).
        sTexture* record = node->mF98;
        CsTexture* texture = record != nullptr ? record->mTexture.get() : nullptr;
        CTextureEntry* entry = texture != nullptr ? texture->GetTextureEntry(0) : nullptr;
        gei_TCache->Prepare(entry, distance);
    }
}

int32_t CMesh::StaticCalculateLights(tag_sNode* node, CMatrix& matrix, CLight** lights, int32_t flags) {
    uint32_t* words = node->mMeshWords;
    // Nothing to colour without vertices, or when the node asks for no lights.
    if (node->mF5E == 0 || (node->mFlags & kNodeNoLights) != 0) return 1;
    if (words == nullptr) msl_assertion_failed("node->pv_UserData", "tb_mesh.cpp", 0x319);
    // The first mesh word is the stamp of the last frame the node was lit in; lit again when
    // the lights changed or the node asks for it. (The original tests kNodeNoLights once more
    // before the work below, which cannot differ.)
    const int32_t stamp = gei_System.mLightFrame;
    if (static_cast<int32_t>(words[0]) == stamp && (gei_System.mFlags & CSystem::kLightsChanged) == 0 &&
        (node->mFlags & kNodeLitAgain) == 0) {
        return 1;
    }
    words[0] = static_cast<uint32_t>(stamp);
    if ((gei_Display.mFlags & kDisplayNoLights) == 0) {
        gev_LightChain = nullptr;
        gei_MapWho->mF30->PrepareLights();
        // The global lights too, unless the mesh is in their list or is an interface mesh.
        const bool global = (static_cast<uint32_t>(flags) & kEntryUnsorted) == 0 &&
                            (static_cast<uint32_t>(flags) & kEntryGlobalList) == 0;
        if (global) gei_MapWho->mF14[6]->PrepareLights();
        if ((gei_Display.mFlags & kDisplayNoMeshLights) == 0) gei_LightManager->AddLights(lights);
        gei_LightManager->PrepareLighting(&matrix, &node->mBox);
    }
    if ((mHeader->mFlags & kHeaderMonoLights) != 0 && gei_System.mRenderer == kMonoRenderer) {
        const int32_t mask = gei_LightManager->mF04;
        gei_LightManager->mF04 = kMonoLightMask;
        gei_LightManager->CalcNodeVertexColors(words + 1, node);
        gei_LightManager->mF04 = mask;
    } else {
        gei_LightManager->CalcNodeVertexColors(words + 1, node);
    }
    return 1;
}

void ColorVertices(SFVF_TLCTEX* vertices, uint32_t colour, int32_t count, int32_t stride) {
    const uint32_t alpha = colour >> 24;
    const uint32_t red = (colour >> 16) & 0xff;
    const uint32_t green = (colour >> 8) & 0xff;
    const uint32_t blue = colour & 0xff;
    uint8_t* destination = reinterpret_cast<uint8_t*>(vertices);
    for (uint32_t remaining = static_cast<uint32_t>(count); remaining != 0; --remaining) {
        // Always the first vertex's colour, as the original (see tb_mesh.h).
        const uint32_t old = vertices->colour;
        const uint32_t product = ((blue * (old & 0xff)) >> 8) | ((green * ((old >> 8) & 0xff)) & 0xff00) |
                                 (((red * ((old >> 16) & 0xff)) << 8) & 0xff0000) |
                                 (((alpha * (old >> 24)) << 16) & 0xff000000);
        std::memcpy(destination + offsetof(SFVF_TLCTEX, colour), &product, sizeof product);
        destination += stride;
    }
}

float DotProduct(D3DVECTOR a, D3DVECTOR b) {
    const float yy = a.y * b.y;
    return ppc_fmadds(a.z, b.z, ppc_fmadds(a.x, b.x, yy));
}
