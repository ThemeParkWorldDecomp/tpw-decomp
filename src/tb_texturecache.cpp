// The texture cache's start-up and shutdown (engine library of SimTheme Park 2.04b3;
// 0x10032bd0, 0x10032d64, 0x1003377c..0x10034b48 and 0x100385a8, which the inventory files
// under Wavelet.cpp, and InitFrequencyTable 0x1000967c from its tb_hfield.cpp range).
// PARTIAL: placing entries on pages (Prepare, Remove, ...) is still to do.
#include "tb_texturecache.h"

#include <cstring>
#include <new>

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_error.h"
#include "tb_loader_dependencies.h"   // CTextureEntry
#include "tb_printf_opengl.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"

CTextureCache* gei_TCache = nullptr;

// ---- CTextureBase ---------------------------------------------------------------------
void CTextureBase::SetFlags(int32_t flags) { mFlags = static_cast<uint32_t>(flags) | (mFlags & kKeptBySetFlags); }

CTextureBase::~CTextureBase() {}

// ---- Start-up and shutdown ------------------------------------------------------------
int32_t gFrequencyTable[16];

void InitFrequencyTable() {
    for (int32_t i = 0; i < 16; ++i) gFrequencyTable[i] = 16 - i;
}

int32_t TCache_Init(int32_t format) {
    InitFrequencyTable();
    if (gei_TCache != nullptr) return 0;
    gei_TCache = new (std::nothrow) CTextureCache;
    if (gei_TCache == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return gei_TCache->Init(format);
}

int32_t TCache_Close() {
    CTextureCache* cache = gei_TCache;
    if (cache == nullptr) return 0;
    cache->Close();
    delete gei_TCache;
    gei_TCache = nullptr;
    return 1;
}

CTextureCache::CTextureCache() {
    mShare[0] = 0.5f;
    mShare[1] = 0.25f;
    mShare[2] = 0.17f;
    mShare[3] = 0.065f;
    mShare[4] = 0.012f;
    mShare[5] = 0.003f;
    mF1894[5] = 0x80;
    mF1894[4] = 0x80;
    mF1894[3] = 0x80;
    mF1894[2] = 0x80;
    mF1894[1] = 0x80;
    mF1894[0] = 0x100;
    mF18DC = 0;
    mF1908 = 0;
    mF18E0 = 0;
    mF18E8 = 1.0f;
    mLargestSize = 0;
    mF1860 = 4;
    mF18AC = 0;
    mF18B0 = 0;
    mFlags = 0;
    mF18BC = 0;
    mF18B8 = 0;
    mSizeMask = 7;
    mMipmaps = 0;
    for (int32_t i = 0; i < kSizes; ++i) {
        mPageCount[i] = 0;
        mF1830[i] = 0;
    }
}

// The page sizes, largest first. AutoCreateSurfaces reads one entry past the six when the
// cache has no page at all; in the original's data that is the first entry of the table
// of bytes per pixel, 2.
static const int32_t kPageSizes[CTextureCache::kSizes + 1] = {256, 128, 64, 32, 16, 8, 2};

// Bytes per pixel of the texture formats (the engine's table at 0x10051eb0).
static const int32_t kBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};

// {pages, value}: the first row whose page count is not exceeded gives mF18DC (its index)
// and mF18E0 (its value).
static const int32_t kQuality[4][2] = {{47, 12}, {79, 8}, {111, 4}, {128, 0}};

int32_t CTextureCache::AutoDetectPageCreation() {
    CTextureSettings& settings = gei_TextureSettings;
    // One 256 x 256 page in bytes.
    const int32_t pageBytes = kBytesPerPixel[settings.mFormat] << 16;
    int32_t pages = gei_OpenGL->MaxTextures(0x100, settings.mFormat);
    if (settings.mTextureMemoryLimit != 0) {
        const int32_t limit = settings.mTextureMemoryLimit / pageBytes;
        if (pages > limit) pages = limit;
    } else {
        if (pages > 0x100) pages = 0x100;
        if (pages > 0x80) pages -= 8;
        else if (pages > 0x40) pages -= 4;
        else if (pages > 0x20) pages -= 2;
    }
    settings.mTextureMemory = pages * pageBytes;
    if (mF1908 != 0) {
        mF18DC = mF1908;
    } else {
        mF18DC = 0;
        while (mF18DC < 3 && pages > kQuality[mF18DC][0]) ++mF18DC;
        mF18E0 = kQuality[mF18DC][1];
    }
    if (pages < 0x20) {
        pages = 0x20;
        mFlags &= ~kFlag2;
    }
    // The flag is cleared before it is tested, so the three-quarters branch never runs.
    mFlags &= ~kFlag2;
    if ((mFlags & kFlag2) != 0 && pages > 0x40) {
        mFlags |= kFlag2;
        pages = pages * 3 / 4;
    } else {
        mFlags &= ~kFlag2;
        mMipmaps = 0;
        mSizeMask = 0;
    }
    std::memset(mWanted, 0, sizeof mWanted);
    // Each size's share of the pages, counted in pages of that size (a 128 page is a
    // quarter of a 256 page, and so on down).
    const float count = static_cast<float>(pages);
    mWanted[0] = ppc_fctiwz(mShare[0] * (count * 1.0f));
    mWanted[1] = ppc_fctiwz(mShare[1] * (count * 4.0f));
    mWanted[2] = ppc_fctiwz(mShare[2] * (count * 16.0f));
    mWanted[3] = ppc_fctiwz(mShare[3] * (count * 64.0f));
    mWanted[4] = ppc_fctiwz(mShare[4] * (count * 256.0f));
    mWanted[5] = ppc_fctiwz(mShare[5] * (count * 1024.0f));
    return 1;
}

int32_t CTextureCache::AutoCreateSurfaces() {
    for (int32_t i = 0; i < kSizes; ++i) {
        int32_t count = mWanted[i];
        if (count != 0 && !CreateSurfaces(kPageSizes[i], count, gei_TextureSettings.mFormat, mMipmaps)) {
            for (int32_t size = 0; size < kSizes; ++size) {
                for (int32_t page = 0; page < mPageCount[size]; ++page) {
                    CTextureLink* link = mPages[size][page];
                    if (link != nullptr) {
                        if (link->mTexture != nullptr) delete link->mTexture;
                        delete link;
                    }
                }
                mPageCount[size] = 0;
            }
            return 0;
        }
        if (i < kSizes - 1) {
            mWanted[i + 1] += mWanted[i] - count;
            mWanted[i] = count;
        }
    }
    int32_t first = 0;
    while (first < kSizes && mPageCount[first] == 0) ++first;
    mLargestSize = kPageSizes[first];
    return 1;
}

int32_t CTextureCache::CreateSurfaces(int32_t size, int32_t& count, int32_t format, int32_t mipmaps) {
    const int32_t index = ConvertSurfaceSizeToFlag(size);
    int32_t made = 0;
    while (made < count) {
        if (mPageCount[index] >= kPagesPerSize) break;
        auto* texture = static_cast<CTextureBase*>(gei_System.mRenderSystem->CreateTextureBase());
        if (texture == nullptr) break;
        if (!texture->Create(size, size, format, mipmaps)) {
            delete texture;
            break;
        }
        CTextureLink* link = new (std::nothrow) CTextureLink;
        ++made;
        mPages[index][mPageCount[index]] = link;
        mPages[index][mPageCount[index]]->mTexture = texture;
        ++mPageCount[index];
    }
    count = made;
    return count > 0;
}

int32_t CTextureCache::Init(int32_t format) {
    CTextureSettings& settings = gei_TextureSettings;
    if ((settings.mFlags & 2) != 0) {
        mFlags |= kFlag2;
        mMipmaps = 4;
    }
    if (gei_System.mRenderer == CSystem::kRendererOpenGL) settings.mFormat = 4;
    if (!AutoDetectPageCreation()) return 0;
    if (!AutoCreateSurfaces()) return 0;
    SetUITextureSize();
    mFlags |= kReady;
    return 1;
}

int32_t CTextureCache::Close() {
    gei_TCache->Flush(1);
    for (int32_t size = 0; size < kSizes; ++size) {
        for (int32_t page = 0; page < mPageCount[size]; ++page) {
            CTextureLink* link = mPages[size][page];
            if (link != nullptr) {
                if (link->mTexture != nullptr) delete link->mTexture;
                delete link;
            }
        }
        mPageCount[size] = 0;
    }
    mFlags &= ~kReady;
    return 1;
}

// Takes one entry off its page. 1 when it is off (or the cache is not up, or the entry was
// on no page); 0 with an assertion for no entry or an entry its page does not have.
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    if ((mFlags & kReady) == 0) return 1;
    if (entry == nullptr) {
        msl_assertion_failed("0", "tb_texturecache_init.cpp", 0x3db);
        return 0;
    }
    if ((entry->mFlags & CTextureEntry::kOnPage) == 0) return 1;
    CTextureEntry* previous = nullptr;
    CTextureEntry* current = entry->mPage->mEntries;
    while (current != nullptr && current != entry) {
        previous = current;
        current = current->mNext;
    }
    if (current == nullptr) {
        msl_assertion_failed("0", "tb_texturecache_init.cpp", 0x3d4);
        return 0;
    }
    if (previous == nullptr) entry->mPage->mEntries = current->mNext;
    else previous->mNext = current->mNext;
    entry->mFlags &= 0xffff0fcfu;
    entry->mFlags &= 0xffffffcfu;
    entry->mFlags |= (entry->mFlags >> 2) & 0x30;
    entry->mCacheWidth = static_cast<int16_t>((entry->mWidth + 7) & 0xfff8);
    entry->mCacheHeight = static_cast<int16_t>((entry->mHeight + 7) & 0xfff8);
    entry->mSortKey = static_cast<int32_t>((static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & 0x4000) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & 0x80000) != 0) entry->mSortKey |= 0x40000000;
    return 1;
}

// An entry leaves its page: its "on a page" bits go, its two "wanted" bits fall back to
// the pair above them, its size is rounded up again and its sort key rebuilt from its
// priority and size.
static void ReleaseEntry(CTextureCache* cache, CTextureEntry* entry, bool clearPlace) {
    entry->mFlags &= ~0x00007000u;
    const int32_t mask = cache->mSizeMask;
    entry->mFlags &= ~0x00000030u;
    entry->mFlags |= (entry->mFlags >> 2) & 0x30;
    entry->mCacheWidth = static_cast<int16_t>(~mask & (entry->mWidth + mask));
    entry->mCacheHeight = static_cast<int16_t>(~mask & (entry->mHeight + mask));
    entry->mSortKey = static_cast<int32_t>((static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & 0x4000) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & 0x80000) != 0) entry->mSortKey |= 0x40000000;
    if (clearPlace) {
        STexturePlace* place = entry->mPlace;
        std::memset(place, 0, 0x10);
        place->mF10 = 0;
        place->mF14 = 0;
    }
}

void CTextureCache::Flush(int32_t mode) {
    const bool clearPlace = (mode & 1) != 0;
    for (int32_t size = 0; size < kSizes; ++size) {
        for (int32_t page = 0; page < mPageCount[size]; ++page) {
            CTextureLink* link = mPages[size][page];
            CTextureEntry* entry = link->mEntries;
            link->mEntries = nullptr;
            if ((mode & 2) == 0) {
                for (; entry != nullptr; entry = entry->mNext) ReleaseEntry(this, entry, clearPlace);
                continue;
            }
            // Only the flagged entries leave; the others go back on the page (in reverse
            // order).
            link->mTexture->SetFlags(0);
            while (entry != nullptr) {
                if ((entry->mFlags & 0x200000) != 0) {
                    ReleaseEntry(this, entry, clearPlace);
                    entry = entry->mNext;
                } else {
                    CTextureEntry* next = entry->mNext;
                    entry->mNext = link->mEntries;
                    link->mEntries = entry;
                    entry = next;
                }
            }
        }
    }
}

// The last line makes the three cases above it pointless, as in the original.
void CTextureCache::SetUITextureSize() {
    if (mPageCount[2] == 0) mUITextureSize = 0x100;
    else if (mPageCount[1] < mPageCount[2]) mUITextureSize = 0x40;
    else mUITextureSize = 0x80;
    mUITextureSize = 0x10;
}
