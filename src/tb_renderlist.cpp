// tb_renderlist.cpp: CRenderList's members (engine library of SimTheme Park 2.04b3; the
// inventory files the range 0x100262e0..0x10026954 under tb_resource_display.cpp). The list
// holds the nodes the world wants drawn this frame: AddEntry transforms a node's vertices
// at once, Render draws the face sets texture page by texture page.
//
// The work arrays of RenderByTexture are engine globals (not exported): the face sets
// waiting to be drawn at 0x100c562c and the tree of texture pages at 0x100b162c.
#include "tb_resource_display.h"

#include "MapWho.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_rendermemory.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

SRenderGroup gRenderGroups[0x1000];
SRenderFace gRenderFaces[0x8000];

namespace {

// Settings flag 0x40: a face set's texture record names the frame to draw.
constexpr uint32_t kSettingsFrames = 0x00000040;
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

void CRenderList::Reset() { mCount = 0; }

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
