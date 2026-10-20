// The engine's mesh object (engine library of SimTheme Park 2.04b3, 0x1001cb44..0x1001f1ec;
// the inventory files the range under tb_profiler.cpp): construction, Init, Close and
// what Close undoes. Prepare and the drawing members are not translated yet.
#include "tb_mesh.h"

#include <cstring>
#include <new>

#include "Wavelet.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_textureentry.h"
#include "platform/msl.h"

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

// 0x1001f120. The signed comparison deliberately permits negative indices.
uintptr_t CMesh::GetTexturePointer(int32_t index) {
    if (mHeader == nullptr || index >= static_cast<int32_t>(mHeader->mTextureCount))
        msl_assertion_failed("mps_data && (texture<mps_data->us_NumTextures)", "tb_mesh_exttexture.cpp", 0x131);
    if (mHeader == nullptr || !mHeader->mTextures[index].mTexture) return 0;
    CTextureEntry* entry = mHeader->mTextures[index].mTexture->GetTextureEntry(0);
    return entry != nullptr ? reinterpret_cast<uintptr_t>(entry->GetData()) : 0;
}
