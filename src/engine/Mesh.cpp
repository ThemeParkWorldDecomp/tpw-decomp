// Mesh.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Mesh code, named after it.

#include "Light.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_meshdelay.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include "Wavelet.h"
#include <cstddef>
#include <cstring>
#include <new>

// tb_meshdelay.cpp: the delayed meshes (include/tb_meshdelay.h).

CMeshDelay gMeshDelay;

void CMeshDelay::Reset() { mCount = 0; }

int32_t CMeshDelay::Prepare() {
    for (int32_t i = 0; i < mCount; i++) {
        mMeshes[i]->mFlags |= 0x10;
        mMeshes[i]->Prepare();   // virtual (CMesh's vtable slot 0x14)
    }
    return 1;
}

// The engine's mesh object (engine library of SimTheme Park 2.04b3, 0x1001cb44..0x1001f1ec;
// the inventory files the range under tb_profiler.cpp): construction, Init, Close and
// what Close undoes. Prepare and the drawing members are not translated yet.

CMesh::CMesh() {
    mHeader = nullptr;
    mFlags = 0;
    mF90 = -1;
    mF94 = -1;
    mF11C = 0;
    mF140 = 0;
    mRideBaseTextures = nullptr;
    mRideBaseOld = nullptr;
    mRideBaseCount = 0;
    mRideSignOld = nullptr;
    mRideSignTextures = nullptr;
    mRideSignCount = 0;
    mF13C = 0;
    std::memset(mF98, 0, sizeof mF98);
    RegisterLinkBase(this);
}

CMesh::~CMesh() {
    FreeLinkBase(this);
    Close();
}

// The textures of a mesh with header flag 0x200 are marked 0x4100000 when the display is
// 512 wide with small textures or 640 wide without them (and display flag 2 is off).
int32_t CMesh::Init(tag_sM3D2Header* header) {
    if (header == nullptr) return 0;
    mHeader = header;
    mFlags = 0;
    if ((header->mFlags & 2) != 0) {
        if (gei_HField == nullptr || !header->mHField) {
            gei_ErrorHandler.Push(3, static_cast<int32_t>(0xb0754006u));
            return 0;
        }
        gei_HField->Init(header);
    }
    if (std::strcmp(header->mName, "BlueprintMesh") == 0) {
        for (int32_t i = 0; i < header->mFaceSetCount; ++i) header->mFaceSets[i].mF0C[0] |= 2;
    }
    bool mark = false;
    if ((header->mFlags & 0x200) != 0 && (gei_Display.mFlags & 2) == 0) {
        mark = (gei_Display.mWidth == 0x200 && gUseSmallTextures()) ||
               (gei_Display.mWidth == 0x280 && !gUseSmallTextures());
    }
    if ((gei_TextureSettings.mFlags & 0x100) != 0) gei_TextureSettings.mFlags |= 0x40000000;
    for (int32_t i = 0; i < mHeader->mTextureCount; ++i) {
        sTexture& record = mHeader->mTextures[i];
        if (!record.mTexture) record.mTexture = new CsTexture;
        CsTexture* texture = record.mTexture;
        if (texture == nullptr) continue;
        sTextureInfo& info = mHeader->mTextureInfos[i];
        texture->Load(&record, &info);
        if (!mark) continue;
        for (int32_t frame = 0; frame < info.mFrames; ++frame) {
            CTextureEntry* entry = texture->GetTextureEntry(frame);
            if (entry != nullptr) entry->mFlags |= 0x4100000;
        }
    }
    if ((gei_TextureSettings.mFlags & 0x40000000) != 0) {
        DelayedLoad_LoadTextures();
        gei_TextureSettings.mFlags &= ~0x40000000u;
    }
    mNodeWords = static_cast<uint32_t*>(
        gMalloc((static_cast<uint32_t>(mHeader->mNodeCount) + static_cast<uint32_t>(mHeader->mUVSetCount) * 4) * 4));
    if (mNodeWords == nullptr) {
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        Close();
        return 0;
    }
    mNodeWordCursor = mNodeWords;
    for (int32_t i = 0; i < mHeader->mNodeCount; ++i) {
        tag_sNode& node = mHeader->mNodes[i];
        if (node.mF5E != 0) {
            node.mMeshWords = mNodeWordCursor;
            *mNodeWordCursor = 0xffffff7f;
        }
        mNodeWordCursor += node.mF5E + 1;
    }
    // The loading picture moves on with every mesh that is initialised.
    if ((gei_System.mFlags & 1) != 0) gei_Splash.InterpolateSplash();
    mFlags |= kInitialised;
    return 1;
}

int32_t CMesh::Close() {
    if (mHeader != nullptr) {
        DestroyBumpmappingNormals();
        DeleteRideSign();
        DeleteRideBase();
        for (int32_t i = 0; i < mHeader->mTextureCount; ++i) {
            sTexture& record = mHeader->mTextures[i];
            CsTexture* texture = record.mTexture;
            if (texture != nullptr && texture->Close()) {
                // Closed for the last time: its memory goes back without a destructor call,
                // as in the original (the destructor would close it once more).
                CsTexture::operator delete(texture);
                record.mTexture = nullptr;
            }
        }
        gFree(reinterpret_cast<void*&>(mNodeWords));
        mNodeWordCursor = nullptr;
        for (int32_t i = 0; i < mHeader->mNodeCount; ++i) mHeader->mNodes[i].mMeshWords = nullptr;
        if ((mHeader->mFlags & 2) != 0 && gei_HField != nullptr) gei_HField->Close();
    }
    mFlags &= ~static_cast<uint32_t>(kInitialised);
    mHeader = nullptr;
    return 1;
}

void CMesh::SetLocalMatrix(int32_t flags, CMatrix& matrix) {
    CMatrix flipY;
    flipY.m[1][1] = -1.0f;
    if ((flags & 1) != 0) {
        mMatrix = matrix * flipY;
        mFlags |= 1;
    }
    if ((flags & 2) != 0) {
        mMatrix2 = matrix * flipY;
        mFlags |= 2;
    }
}

void CMesh::SetLocalMatrix(int32_t flags, sMatrix& matrix) {
    CMatrix copy;
    std::memcpy(&copy, &matrix, sizeof(matrix));
    SetLocalMatrix(flags, copy);
}

// Unit v5752: the ride base members of CMesh (engine 0x1001d97c, 0x1001e644, 0x1001e72c).

namespace {

// Engine data 0x10051ce0: the overlay format (bytes a pixel) for each texture format index.
const int32_t kOverlayFormat[6] = {2, 2, 2, 4, 2, 4};

}  // namespace

// 0x1001d97c. Both branches of the flag test compute the same thing in the original.
void CMesh::CenterMatrix(int32_t flags, float scale) {
    (void)flags;
    const float x0 = mHeader->mBox.mMin[0], y0 = mHeader->mBox.mMin[1], z0 = mHeader->mBox.mMin[2];
    const float x1 = mHeader->mBox.mMax[0], y1 = mHeader->mBox.mMax[1], z1 = mHeader->mBox.mMax[2];
    float sx = scale * (2.0f / (x1 - x0));
    float sy = scale * (2.0f / (y1 - y0));
    float sz = scale * (1.0f / (z1 - z0));
    float tx = ppc_fnmsubs(x1, sx, 1.0f);
    float ty = ppc_fnmsubs(y1, sy, 1.0f);
    float tz = ppc_fnmsubs(z1, sz, 1.0f);
    CMatrix matrix;
    std::memset(&matrix, 0, sizeof(sMatrix));
    matrix.m[3][3] = 1.0f;
    matrix.m[0][0] = sx;
    matrix.m[1][1] = sy;
    matrix.m[2][2] = sz;
    matrix.m[3][0] = tx;
    matrix.m[3][1] = ty;
    matrix.m[3][2] = tz;
    SetLocalMatrix(1, matrix);
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

// With texture setting 0x20: every node (without node flag 0x200) that has a face set
// whose texture record has flag 0x1000 frees the block at its 0x74.
int32_t CMesh::DestroyBumpmappingNormals() {
    if ((gei_TextureSettings.mFlags & 0x20) == 0) return 1;
    tag_sNode* node = mHeader->mNodes;
    for (int32_t i = 0; i < mHeader->mNodeCount; ++i, ++node) {
        if ((node->mFlags & 0x200) != 0) continue;
        int32_t set = static_cast<int32_t>(node->mFaceSetCount) - 1;
        for (; set >= 0; --set) {
            const sTexture* texture = node->mFaceSets[set].mTexture;
            if (texture != nullptr && (texture->mF00 & 0x1000) != 0) break;
        }
        if (set >= 0) {
            void* block = node->mF74.get();
            gFree(block);
            node->mF74 = nullptr;
        }
    }
    return 1;
}

void* CMesh::GetData() { return mHeader; }
int32_t CMesh::GetLinkType() { return 2; }

// 0x1001e644. Answers what CreateProcTexture answers.
int32_t CMesh::GenerateRideBase(CTextureEntry* target, CTextureEntry* source, int32_t x, int32_t y, int32_t flags) {
    sTileOverlay overlay;
    overlay.mData = source->GetCorrectedData();
    overlay.mWidth = source->mWidth;
    overlay.mHeight = source->mHeight;
    overlay.mFormat = kOverlayFormat[gei_TextureSettings.mFormat];
    overlay.mFlags = static_cast<uint32_t>(flags);
    return gei_HField->mProcTextures->CreateProcTexture(target, x, y, &overlay);
}

// 0x1001e72c. The texture records of the cells x0..x1, y0..y1 of the height field that have both
// overlay flags (0x80, 0x200) get a generated texture; the old ones are remembered.
int32_t CMesh::GenerateRideBase(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (!(mRideBaseOld == nullptr && mRideBaseTextures == nullptr && (mFlags & kInitialised) != 0))
        msl_assertion_failed("(mps_OldRideBases==NULL) && (mps_NewRideBases==NULL) && (md_Flags&CMESHFLAGS__INTERNAL__INITIALIZED)",
                             "tb_mesh_exttexture.cpp", 0x44);
    mRideBaseCount = 0;
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) {
            sHFieldQuad& cell = gei_HField->mF00->mQuads[static_cast<int32_t>(
                static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * gei_HField->mF00->mF18[0])];
            if ((cell.mF00 & 0x80) != 0 && (cell.mF00 & 0x200) != 0) ++mRideBaseCount;
        }
    }
    mRideBaseOld = new (std::nothrow) SOldCsTexture[mRideBaseCount];
    mRideBaseTextures = new CsTexture[mRideBaseCount];
    if (mRideBaseOld == nullptr && mRideBaseTextures == nullptr) {
        DeleteRideBase();
        gei_ErrorHandler.Push(2, static_cast<int32_t>(0xB0754000u));
        return 0;
    }
    int32_t ok = 1;
    int32_t k = 0;
    for (int32_t y = y0; y <= y1; ++y) {
        for (int32_t x = x0; x <= x1; ++x) {
            sHFieldQuad& cell = gei_HField->mF00->mQuads[static_cast<int32_t>(
                static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * gei_HField->mF00->mF18[0])];
            uint32_t flags = cell.mF00;
            if ((flags & 0x80) != 0 && (flags & 0x200) != 0) {
                Ptr32<CsTexture>* where = &mHeader->mTextures[cell.mF02].mTexture;
                CTextureEntry** entryAddr = where->get()->GetTextureEntryAddr(0);
                CTextureEntry* created = new (std::nothrow) CTextureEntry[1];
                if (created == nullptr) {
                    gei_ErrorHandler.Push(2, static_cast<int32_t>(0xB0754000u));
                    for (int32_t i = k - 1; i >= 0; --i) mRideBaseTextures[i].Close();
                    delete[] mRideBaseOld;
                    delete[] mRideBaseTextures;
                    return 0;
                }
                CTextureEntry* source = *entryAddr;
                sTileOverlay overlay;
                overlay.mData = source->GetCorrectedData();
                overlay.mWidth = source->mWidth;
                overlay.mHeight = source->mHeight;
                overlay.mFormat = kOverlayFormat[gei_TextureSettings.mFormat];
                overlay.mFlags = flags;
                ok &= gei_HField->mProcTextures->CreateProcTexture(created, x, y, &overlay);
                gei_TCache->Remove(*entryAddr);
                (mRideBaseTextures + k)->Load(created);
                mRideBaseOld[k].mWhere = where;
                mRideBaseOld[k].mOld = where->get();
                *where = mRideBaseTextures + k;
                ++k;
            }
        }
    }
    if (k != mRideBaseCount) msl_assertion_failed("k==md_RideBaseCount", "tb_mesh_exttexture.cpp", 0x8f);
    if (ok != 0) mFlags |= kRideBase;
    return ok;
}

// Each record gets back the texture it had, last first; the two lists are deleted.
int32_t CMesh::DeleteRideBase() {
    if ((mFlags & kRideBase) != 0 && mRideBaseCount > 0) {
        for (int32_t i = mRideBaseCount - 1; i >= 0; --i) *mRideBaseOld[i].mWhere = mRideBaseOld[i].mOld;
        if (mRideBaseOld != nullptr) {
            delete[] mRideBaseOld;
            mRideBaseOld = nullptr;
        }
        if (mRideBaseTextures != nullptr) {
            delete[] mRideBaseTextures;
            mRideBaseTextures = nullptr;
        }
        mFlags &= ~static_cast<uint32_t>(kRideBase);
    }
    mRideBaseCount = 0;
    return 1;
}

// 0x1001ed9c: replaces the selected textures and writes their new pixel pointers
// as arena offsets (the two callers store them in 32-bit generated texture records).
int32_t CMesh::GenerateRideSign(uint32_t* indices, int32_t count) {
    if (mRideSignTextures != nullptr || mRideSignOld != nullptr)
        msl_assertion_failed("(mps_NewRideSign==NULL) && (mps_OldRideSign==NULL)", "tb_mesh_exttexture.cpp", 0xcf);
    mRideSignOld = new (std::nothrow) SOldCsTexture[count];
    mRideSignTextures = new CsTexture[count];
    if (mRideSignOld == nullptr || mRideSignTextures == nullptr) {
        if (mRideSignTextures != nullptr) gFree(reinterpret_cast<void*&>(mRideSignTextures));
        if (mRideSignOld != nullptr) gFree(reinterpret_cast<void*&>(mRideSignOld));
        return 0;
    }
    mRideSignCount = count;
    for (int32_t i = 0; i < count; ++i) {
        sTexture& entry = mHeader->mTextures[indices[i]];
        CsTexture* old = entry.mTexture;
        if (old->GetsTexture() == nullptr || !(old->GetsTexture()->mF00 & 0x80))
            msl_assertion_failed("((*cst)->GetsTexture()) && ((*cst)->GetsTexture()->us_TextureFlags & TF_IsGeneratedTexture)",
                                 "tb_mesh_exttexture.cpp", 0xf0);
        if (mRideSignTextures[i].ReLoad(old) == 0) {
            // kRideSign is only set on success, so DeleteRideSign leaves the arrays
            // and any earlier pointer swaps alone on this failure path.
            DeleteRideSign();
            return 0;
        }
        mRideSignOld[i].mOld = entry.mTexture;
        mRideSignOld[i].mWhere = &entry.mTexture;
        entry.mTexture = &mRideSignTextures[i];
        void* data = reinterpret_cast<void*>(GetTexturePointer(static_cast<int32_t>(indices[i])));
        indices[i] = data != nullptr ? tpw::ArenaOffset(data) : 0;
    }
    mFlags |= kRideSign;
    return 1;
}

int32_t CMesh::DeleteRideSign() {
    if ((mFlags & kRideSign) != 0 && mRideSignCount > 0) {
        for (int32_t i = 0; i < mRideSignCount; ++i) {
            if (mRideSignOld[i].mOld != nullptr) *mRideSignOld[i].mWhere = mRideSignOld[i].mOld;
        }
        mFlags &= ~static_cast<uint32_t>(kRideSign);
        if (mRideSignOld != nullptr) {
            delete[] mRideSignOld;
            mRideSignOld = nullptr;
        }
        if (mRideSignTextures != nullptr) {
            delete[] mRideSignTextures;
            mRideSignTextures = nullptr;
        }
        mRideSignCount = 0;
    }
    return 1;
}

// 0x1001f120. The signed comparison deliberately permits negative indices.
uintptr_t CMesh::GetTexturePointer(int32_t index) {
    if (mHeader == nullptr || index >= static_cast<int32_t>(mHeader->mTextureCount))
        msl_assertion_failed("mps_data && (texture<mps_data->us_NumTextures)", "tb_mesh_exttexture.cpp", 0x131);
    if (mHeader == nullptr || !mHeader->mTextures[index].mTexture) return 0;
    CTextureEntry* entry = mHeader->mTextures[index].mTexture->GetTextureEntry(0);
    return entry != nullptr ? reinterpret_cast<uintptr_t>(entry->GetData()) : 0;
}
