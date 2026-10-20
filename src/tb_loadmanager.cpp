// tb_loadmanager.cpp (engine library of SimTheme Park 2.04b3, 0x1001658c..0x1001722c; the
// name is the one in CsTexture's assertions): the load manager's RemoveTexture, the
// delayed-load list and CsTexture.
#include "tb_loadmanager.h"

#include <cstring>
#include <new>

#include "Wavelet.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

int32_t gDelayedLoadCount;
CsTexture* gDelayedLoads[256];

// One reference less on the load that holds `texture`; at the last one the texture is
// closed and deleted and the table's last load takes its place. 1 when it was deleted.
int32_t CTextureLoadManager::RemoveTexture(CTextureEntry* texture) {
    int32_t index = mCount - 1;
    while (index >= 0 && mLoads[index].mTexture != texture) --index;
    if (index < 0) return 0;
    STextureLoad& load = mLoads[index];
    if (--load.mReferences != 0) return 0;
    load.mTexture->Close();
    delete[] load.mTexture;
    load = mLoads[--mCount];
    return 1;
}

void DelayedLoad_LoadTextures() {
    for (int32_t i = 0; i < gDelayedLoadCount; ++i) gDelayedLoads[i]->LoadTextureFromDisk();
    gDelayedLoadCount = 0;
}

void* CsTexture::operator new(size_t size) noexcept { return tpw::ArenaAlloc(size); }
void CsTexture::operator delete(void* block) { tpw::ArenaFree(block); }
void* CsTexture::operator new[](size_t size) noexcept { return tpw::ArenaAlloc(size); }
void CsTexture::operator delete[](void* block) { tpw::ArenaFree(block); }

void CsTexture::Reset() {
    mTexture = nullptr;
    mInfo = nullptr;
    mEntries = nullptr;
    mEntryCount = 0;
    mReferences = 0;
}

// The entry list is an array of pointers: four bytes each on the Mac, a pointer's size here.
static CTextureEntry** NewEntryList(uint32_t count) {
    return static_cast<CTextureEntry**>(gMalloc(count * static_cast<uint32_t>(sizeof(CTextureEntry*))));
}

int32_t CsTexture::Load(CTextureEntry* entry) {
    Reset();
    mEntries = NewEntryList(1);
    if (mEntries == nullptr) {
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    mEntryCount = static_cast<int16_t>(0x8000);
    mReferences = 1;
    mEntries[0] = entry;
    return 1;
}

// Loads one file through the manager. As in the original, the first list it allocates is
// lost: Load(entry), inline here, resets the object and allocates another.
// When a list cannot be allocated the original deletes the loaded entry as a single
// object although the manager made it as an array; here the entry is destroyed and its
// memory left alone.
int32_t CsTexture::Load(char* name) {
    CTextureEntry* entry = gei_TLoadManager->LoadFile(name);
    if (entry == nullptr) return 0;
    mEntries = NewEntryList(1);
    if (mEntries == nullptr) {
        entry->~CTextureEntry();
        return 0;
    }
    mEntries[0] = entry;
    if (!Load(entry)) {
        gFree(reinterpret_cast<void*&>(mEntries));
        entry->~CTextureEntry();
        return 0;
    }
    mEntryCount = static_cast<int16_t>(0x8000);
    mReferences = 1;
    return 1;
}

int32_t CsTexture::Load(sTexture* texture, sTextureInfo* info) {
    if (mEntries != nullptr) {
        ++mReferences;
        for (int32_t i = 0; i < mEntryCount; ++i) {
            if ((texture->mF00 & 0x30) != 0) mEntries[i]->mFlags |= 0x30000;
        }
        return 1;
    }
    mTexture = texture;
    mInfo = info;
    mEntries = NewEntryList(mInfo->mFrames);
    if (mEntries == nullptr) return 0;
    if ((texture->mF00 & 0x80) != 0) {
        // A blank image of the object's own. Its size is the 16-bit word at byte 6 of what
        // is the name in other records (file bytes the loader does not swap: big end first).
        CTextureEntry* entry = new (std::nothrow) CTextureEntry[1];
        SetTextureEntries(0x8000);
        if (entry == nullptr) {
            gei_ErrorHandler.Push(2, kErrOutOfMemory);
            return 0;
        }
        const auto* record = reinterpret_cast<const uint8_t*>(info->mName.get());
        if (!entry->Create(record[6] << 8 | record[7], 1)) {
            delete[] entry;
            gei_ErrorHandler.Push(2, kErrOutOfMemory);
            return 0;
        }
        SetTextureEntry(entry, 0);
        mEntries[0]->mFlags = 0x1000240;
    } else {
        SetTextureEntries(GetFrameCount());
        if ((gei_TextureSettings.mFlags & 0x40000000) == 0) {
            LoadTextureFromDisk();
        } else if (gDelayedLoadCount < 0x100) {
            gDelayedLoads[gDelayedLoadCount++] = this;
        }
    }
    mReferences = 1;
    return 1;
}

int32_t CsTexture::LoadTextureFromDisk() {
    for (int32_t i = 0; i < mEntryCount; ++i) {
        if ((mTexture->mF00 & 0x1000) != 0) gei_TLoadManager->SetLoadFlags(2);
        if ((mTexture->mF00 & 0x2000) != 0) gei_TLoadManager->SetLoadFlags(4);
        if ((mTexture->mF00 & 0x4000) != 0) gei_TLoadManager->SetLoadFlags(8);
        mEntries[i] = gei_TLoadManager->LoadTexture(mInfo->mFolder, mInfo->mName.get() + i * 0x14);
        CTextureEntry* entry = mEntries[i];
        if (entry == nullptr) continue;
        if ((mTexture->mF00 & 0x10) != 0) entry->mFlags |= 0x10000;
        if ((mTexture->mF00 & 0x20) != 0) entry->mFlags |= 0x20000;
        if ((mTexture->mF00 & 0x30) != 0) {
            const int32_t width = entry->mWidth;
            if (!(width == entry->mHeight && width > 0 && (width & (width - 1)) == 0)) {
                msl_assertion_failed("(mpps_TEntry[i]->GetSrcWidth()==mpps_TEntry[i]->GetSrcHeight()) && "
                                     "IsPO2(mpps_TEntry[i]->GetSrcWidth())",
                                     "tb_loadmanager.cpp", 0xfb);
            }
        }
    }
    return 1;
}

int32_t CsTexture::ReLoad(CsTexture* other) { return Load(other->mTexture, other->mInfo); }

int32_t CsTexture::Close() {
    if (--mReferences != 0) return 0;
    if (mEntries == nullptr) msl_assertion_failed("mpps_TEntry!=NULL", "tb_loadmanager.cpp", 0x112);
    if ((mEntryCount & 0x8000) == 0) {
        for (int32_t i = 0; i < mEntryCount; ++i) gei_TLoadManager->RemoveTexture(mEntries[i]);
    } else {
        mEntries[0]->Close();
        delete[] mEntries[0];
    }
    gFree(reinterpret_cast<void*&>(mEntries));
    return 1;
}

uint16_t CsTexture::GetFrameCount() { return mInfo->mFrames; }
