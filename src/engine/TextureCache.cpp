// TextureCache.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TextureCache code, named after it.

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_error.h"
#include "tb_loader_dependencies.h"   // CTextureEntry
#include "tb_printf_opengl.h"
#include "tb_profiler.h"
#include "tb_qsort.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>



// The texture cache's start-up and shutdown (engine library of SimTheme Park 2.04b3;
// 0x10032bd0, 0x10032d64, 0x1003377c..0x10034b48 and 0x100385a8, which the inventory files
// under Wavelet.cpp, and InitFrequencyTable 0x1000967c from its tb_hfield.cpp range).
// PARTIAL: placing entries on pages (Prepare, Remove, ...) is still to do.

CTextureCache* gei_TCache = nullptr;

// ---- CTextureBase ---------------------------------------------------------------------
void CTextureBase::SetFlags(int32_t flags) { mFlags = static_cast<uint32_t>(flags) | (mFlags & kKeptBySetFlags); }

CTextureBase::~CTextureBase() {}

// tb_texturecache_frame2.cpp (engine library of SimTheme Park 2.04b3, the texture cache's
// per-frame calls other than Refresh and Execute; the inventory files the range under
// Wavelet.cpp): CTextureCache::Prepare 0x10032ddc, ReInitialize 0x1003447c,
// DumpTCStreamToLog_Prepare 0x10034bc4, DumpTCStreamToLog_Execute 0x10034d50,
// GenerateTexturePages 0x10037494, UploadToVRAM 0x1003820c and CTextureLink::PageUpdated
// 0x100385f4.
//
// The frame asks for pictures with Prepare, which queues every entry that is on no page
// (CTextureCache::mF18AC); Execute sorts the queue and GenerateTexturePages packs it onto
// pages, PackTextures calling UploadToVRAM and PageUpdated for each entry and page it fills.
// The file "tcache.log" is a debug stream of the requests (texture settings flag 0x80).

int32_t gTexturePageUpdates = 0;

namespace {

// The entry's flags (CTextureEntry::mFlags) this file reads; the named ones are in
// tb_textureentry.h.
constexpr uint32_t kEntryWanted = 0x00000030;      // the pair 0xc0 is copied down into it when an entry is queued or dropped
constexpr uint32_t kEntryQueued = 0x00002000;      // Prepare queued it this frame; UploadToVRAM clears it
constexpr uint32_t kEntrySortLocked = 0x00004000;  // the sort key gets 0x20000000
constexpr uint32_t kEntrySortBlend = 0x00080000;   // the sort key gets 0x40000000
constexpr uint32_t kEntryWrapU = 0x00010000;       // part of kWrapAny: UploadToVRAM's offset along u is 0
constexpr uint32_t kEntryWrapV = 0x00020000;       // the same along v
constexpr uint32_t kEntryFlag400 = 0x00000400;     // with no second block (mF38) a wrapping image is placed like any other
constexpr uint32_t kEntryFlag4000000 = 0x04000000; // UploadToVRAM takes the settings' four fixups
constexpr uint32_t kEntryNoPrepare = 0x02000000;   // Prepare ignores the entry
constexpr uint32_t kEntryAsked = 0x40000000;       // Prepare was called for it this frame
constexpr uint32_t kEntryCentred = 0x80000000;     // UploadToVRAM centres the image in a larger slot, once

// The entry's place record: this frame's use count is the byte gFrequencyIndex, mF10 their
// sum and mF14 the entry's score (0x8000 marks "used").
constexpr int32_t kPlaceUsed = 0x00008000;

// Cache flag: the stream file has been started.
constexpr uint32_t kCacheStreamStarted = 0x40000000;
// Texture settings flag: the requests are written to the stream file.
constexpr uint32_t kSettingsDumpStream = 0x00000080;

// The stream file's name (in the working directory).
const char kStreamName[] = "tcache.log";

// An entry's sort key: the priority (nearer is larger) in bits 16..27, the cache size
// (width * 256 + height) below it, and a bit each for a locked entry and for the 0x80000
// kind. CTextureCache::Remove builds the same key.
void RebuildSortKey(CTextureEntry* entry) {
    entry->mSortKey = static_cast<int32_t>(
        (static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & kEntrySortLocked) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & kEntrySortBlend) != 0) entry->mSortKey |= 0x40000000;
}

// An entry the packing gave up on: its page bits (0x1000..0x8000) go and its "wanted" pair
// falls back to the pair above it, its size is rounded up to 8 and its key rebuilt. This is
// CTextureCache::Remove's ending with 7 in place of the cache's size mask. (The locked bit
// 0x4000 is cleared with the page bits, so the key's locked test is dead, as in Remove.)
void ForgetPlacement(CTextureEntry* entry) {
    entry->mFlags &= 0xffff0fcfu;
    entry->mFlags &= ~kEntryWanted;
    entry->mFlags |= (entry->mFlags >> 2) & kEntryWanted;
    entry->mCacheWidth = static_cast<int16_t>((entry->mWidth + 7) & 0xfff8);
    entry->mCacheHeight = static_cast<int16_t>((entry->mHeight + 7) & 0xfff8);
    RebuildSortKey(entry);
}

// What the debug stream holds for one request, as the Mac wrote it after a one-byte tag:
// the entry's address (32 bits), flags, size and priority.
struct SStreamRecord {
    uint32_t mEntry;
    uint32_t mFlags;
    int16_t mWidth;
    int16_t mHeight;
    float mPriority;
};
static_assert(sizeof(SStreamRecord) == 0x10, "SStreamRecord layout");

}  // namespace

void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    tpw_probe::Hit(tpw_probe::kTexPrepare);
    if (entry == nullptr) { tpw_probe::Hit(tpw_probe::kTexPrepareNull); return; }
    // Both asserts return, and the function carries on.
    if (!(entry->mWidth > 0 && entry->mCacheWidth != 0)) {
        msl_assertion_failed("(src->GetSrcWidth()>0) && (src->GetDestWidth())", "tb_texturecache_core.cpp", 0x23);
    }
    if ((entry->mFlags & CTextureEntry::kWrapAny) != 0 && entry->mWidth != entry->mHeight) {
        msl_assertion_failed("(!src->IsWrapping()) || (src->GetSrcWidth()==src->GetSrcHeight())",
                             "tb_texturecache_core.cpp", 0x26);
    }
    if ((entry->mFlags & kEntryNoPrepare) != 0) { tpw_probe::Hit(tpw_probe::kTexPrepareIgnored); return; }
    entry->mFlags |= kEntryAsked;

    // This frame's use count (a byte, which stops at 255), the sum of the counts and the
    // score's "used" bit.
    STexturePlace* place = entry->mPlace;
    if (place->mF00[gFrequencyIndex] != 0xff) {
        ++place->mF00[gFrequencyIndex];
        ++place->mF10;
    }
    place->mF14 |= kPlaceUsed;

    if ((entry->mFlags & (CTextureEntry::kOnPage | kEntryQueued)) == 0) {
        // On no page and not queued yet: queue it, size and key as the cache holds them.
        entry->mPriority = distance;
        entry->mFlags |= kEntryQueued;
        entry->mFlags &= ~kEntryWanted;
        entry->mFlags |= (entry->mFlags >> 2) & kEntryWanted;
        entry->mCacheWidth = static_cast<int16_t>(~mSizeMask & (entry->mWidth + mSizeMask));
        entry->mCacheHeight = static_cast<int16_t>(~mSizeMask & (entry->mHeight + mSizeMask));
        RebuildSortKey(entry);
        entry->mNext = mF18AC;
        mF18AC = entry;
        tpw_probe::Hit(tpw_probe::kTexQueued);
    } else {
        tpw_probe::Hit(tpw_probe::kTexPrepareHeld);
        if (distance < entry->mPriority) entry->mPriority = distance;
        // When the view or the lights changed a locked entry on a page locks its page.
        if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0) {
            if ((entry->mFlags & CTextureEntry::kOnPage) != 0 && (entry->mFlags & CTextureEntry::kLocked) != 0) {
                entry->mPage->mTexture->PositionLock();
            }
        }
    }
    // A further plane of the same image is the next entry of the array.
    if ((entry->mFlags & CTextureEntry::kNextIsPlane) != 0) Prepare(entry + 1, distance);
    if ((gei_TextureSettings.mFlags & kSettingsDumpStream) != 0) DumpTCStreamToLog_Prepare(entry);
}

// tb_texturecache_sort.cpp (engine library of SimTheme Park 2.04b3; the inventory files the
// range under Wavelet.cpp, the original files were tb_texturecache_*.cpp): the texture
// cache's sorting and its lists of entries.
//
//   0x10033098 CTextureCache::Textures_QSort      0x100331f8 Textures_QSortFree
//   0x1003335c SortTextures                       0x10034e08 Entry_QSortInit
//   0x10035300 SortEntries                        0x100358ec SortByCriteria
//   0x10035ab0 SortByAddress                      0x10035be0 MergeListsConditional
//   0x10035e6c CreateNonWrappingTextureList
//
// Each frame Execute sorts the pages of every size (SortTextures) and the queue of entries
// that Prepare collected (SortEntries), then the packing arranges entries on pages with the
// list helpers. An entry's "sort key" puts the nearest picture first (see CTextureEntry
// and Prepare); the lists the packing builds are chains through CTextureEntry::mNext.
//
// Host notes. The sorts work on arrays of native pointers, so CQSort is told the host's record
// size and key offsets where the Mac passes 0x5c, 0x34 and 0x2c; SortByAddress sorts the
// pointers themselves as 8-byte records by the low 32 bits of the address (its keys on the Mac
// are 32-bit addresses; two entries are never at the same address, so the order is the
// order of the addresses unless they lie in different 4 GB regions).

namespace {

// Longest list the entry sorts take (the Mac keeps the array on the stack).
constexpr int32_t kSortLimit = 0x1000;


// Cache flags (CTextureCache::mFlags).
constexpr uint32_t kCacheReshuffle = 0x00000001;   // CACHEFLAG_RESHUFFLE: locked pages and entries are sorted too
constexpr uint32_t kCacheLimited = 0x00000004;     // SortEntries: the entries' area is held to mF18E4

constexpr uint32_t kEntryLocked = 0x00004000;      // the sort key gets 0x20000000
constexpr uint32_t kEntryDropped = 0x00008000;     // SortEntries: left out of the queue
constexpr uint32_t kEntryBlendKind = 0x00080000;   // the sort key gets 0x40000000
constexpr uint32_t kEntryPageBits = 0xffff0fcfu;   // clears 0x1000..0x8000 (page, queued, locked, dropped) and 0x30

// An entry's sort key: the priority (nearer is larger) in bits 16..27, the cache size
// (width * 256 + height) below it, and a bit each for a locked entry and for the 0x80000
// kind. Prepare, Remove and GenerateTexturePages build the same.
void RebuildSortKey_2(CTextureEntry* entry) {
    entry->mSortKey = static_cast<int32_t>(
        (static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & kEntryLocked) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & kEntryBlendKind) != 0) entry->mSortKey |= 0x40000000;
}

// An entry that goes back to the state of one on no page: the page bits and the "wanted" pair
// go, the pair above falls into it, the cache size is the size rounded up to 8 and the sort
// key is built again. (The page bits include the locked bit 0x4000, so the key's locked test
// is dead here, as in CTextureCache::Remove.)
void ResetEntry(CTextureEntry* entry) {
    entry->mFlags &= kEntryPageBits;
    entry->mFlags &= ~kEntryWanted;
    entry->mFlags |= (entry->mFlags >> 2) & kEntryWanted;
    entry->mCacheWidth = static_cast<int16_t>((entry->mWidth + 7) & 0xfff8);
    entry->mCacheHeight = static_cast<int16_t>((entry->mHeight + 7) & 0xfff8);
    RebuildSortKey_2(entry);
}

// What the cache has of an entry's pixels: the size the cache holds, width times height.
int32_t CacheArea(const CTextureEntry* entry) { return entry->mCacheWidth * entry->mCacheHeight; }

}  // namespace

// ---- The pages -------------------------------------------------------------------------

void CTextureCache::Textures_QSort(CTextureLink** links, int32_t low, int32_t high) {
    int32_t i = low;
    int32_t j = high;
    const int32_t pivot = links[(low + high) >> 1]->mF0C;
    do {
        while (links[i]->mF0C < pivot) ++i;
        while (links[j]->mF0C > pivot) --j;
        if (i <= j) {
            CTextureLink* moved = links[i];
            links[i] = links[j];
            links[j] = moved;
            ++i;
            --j;
        }
    } while (i <= j);
    if (low < j) Textures_QSort(links, low, j);
    if (i < high) Textures_QSort(links, i, high);
}

void CTextureCache::Textures_QSortFree(CTextureLink** links, int32_t low, int32_t high) {
    int32_t i = low;
    int32_t j = high;
    const int32_t pivot = links[(low + high) >> 1]->mF10;
    do {
        while (links[i]->mF10 < pivot) ++i;
        while (links[j]->mF10 > pivot) --j;
        if (i <= j) {
            CTextureLink* moved = links[i];
            links[i] = links[j];
            links[j] = moved;
            ++i;
            --j;
        }
    } while (i <= j);
    if (low < j) Textures_QSortFree(links, low, j);
    if (i < high) Textures_QSortFree(links, i, high);
}

void CTextureCache::SortTextures() {
    mF18B8 = 0;
    mF18BC = 0;
    mF18C0 = 0;
    for (int32_t size = 0; size < kSizes; ++size) {
        CTextureLink** pages = mPages[size];
        // How much of each page is in use; a page that is position locked is left alone
        // unless the cache is reshuffling.
        int32_t k = 0;
        for (; k < mPageCount[size]; ++k) {
            if ((mFlags & kCacheReshuffle) != 0 || pages[k]->mTexture->IsPositionLocked() == 0) {
                pages[k]->CalcUsage(static_cast<int32_t>(mFlags));
            }
        }
        int32_t last;
        if ((mFlags & kCacheReshuffle) == 0) {
            // The pages that are not locked go first, in any order (the scans meet in the
            // middle), then they are ordered by the pixels in use.
            last = mPageCount[size] - 1;
            int32_t first = 0;
            while (first < last) {
                for (; first < last; ++first) {
                    if (pages[first]->mTexture->IsPositionLocked() != 0) break;
                }
                for (; first < last; --last) {
                    if (pages[last]->mTexture->IsPositionLocked() == 0) break;
                }
                if (first <= last) {
                    CTextureLink* moved = pages[first];
                    pages[first] = pages[last];
                    pages[last] = moved;
                    ++first;
                    --last;
                }
            }
            if (last > 0) Textures_QSort(pages, 0, last);
            if (last < mPageCount[size]) ++last;
        } else {
            if (mPageCount[size] != 0) Textures_QSort(pages, 0, mPageCount[size] - 1);
            last = mPageCount[size];
        }
        mF1818[size] = last;

        // From the end back to the first page that is at most half full: the pages before it
        // are ordered by mF10.
        k = mF1818[size] - 1;
        for (; k > 0; --k) {
            CTextureLink* page = pages[k];
            const int32_t used = page->mF0C;
            const int32_t pixels = page->mTexture->GetPixelCount();
            if (static_cast<int32_t>(static_cast<uint32_t>(used) << 1) <= pixels) break;
        }
        if (k > 0) Textures_QSortFree(pages, 0, k);
        mF1818[size] = k;
    }

    // The totals over the pages that took part.
    mF18B8 = 0;
    mF18BC = 0;
    mF18C0 = 0;
    for (int32_t size = 0; size < kSizes; ++size) {
        CTextureLink** pages = mPages[size];
        for (int32_t k = 0; k < mPageCount[size]; ++k) {
            CTextureBase* texture = pages[k]->mTexture;
            if ((mFlags & kCacheReshuffle) != 0 || texture->IsPositionLocked() == 0) {
                pages[k]->CalcUsage(static_cast<int32_t>(mFlags));
                mF18B8 += pages[k]->mF08;
                mF18BC += pages[k]->mF0C;
                mF18C0 += texture->GetPixelCount();
            }
        }
    }
}

// tb_texturecache_frame.cpp (engine library of SimTheme Park 2.04b3, the texture cache's frame
// calls; the inventory files the range under Wavelet.cpp): CTextureCache::Refresh 0x10034204
// and Execute 0x10033688.
//
// Refresh runs at the start of every window's frame. It ages every entry on every page (the
// frame's use count goes out of the entry's place record and the record's score halves),
// forgets what was asked for the last frame and, when the view or the lights changed (the
// static pool is drawn again), unlocks every entry and page so they can be placed anew.
// Execute runs after the window's links were prepared: when pictures were asked for it sorts
// them, balances the pages and generates the pages.
//
// Refresh is void in the original; it answers 1 here because the declaration says int32_t.

uint32_t gFrequencyIndex = 0;

namespace {

// An entry's flags that Refresh clears: 0x2000 (asked for this frame; also cleared on the
// entries still waiting) and 0x40000000.
constexpr uint32_t kEntryFlags2000 = 0x00002000;
constexpr uint32_t kEntryFlags40000000 = 0x40000000;

// Execute's error when no page could be generated.
constexpr int32_t kErrGeneratePages = static_cast<int32_t>(0xb0754011);

}  // namespace

int32_t CTextureCache::Execute() {
    if ((gei_TextureSettings.mFlags & kSettingsDumpStream) != 0) DumpTCStreamToLog_Execute();
    if (mF18AC == nullptr) return 1;
    tpw_probe::Hit(tpw_probe::kTexExecute);
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0)
        BalanceCache_FreeLockedTextureSpace();
    SortTextures();
    SortEntries();
    BalanceCache_LevelSurfaceUsage();
    if (GenerateTexturePages() == 0) {
        gei_ErrorHandler.Push(2, kErrGeneratePages);
        return 0;
    }
    return 1;
}

extern int32_t gFrequencyTable[16];

void InitFrequencyTable();

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

// Bytes per pixel by texture format (engine data 0x10051eb0, indexed by the settings' mFormat).
static const int32_t kUploadBytes[8] = {2, 2, 2, 4, 2, 4, 0xc0, 3};

// Times `count` locks and unlocks of the first page of `size`; answers pixels per clock tick
// (0 when a lock or unlock fails or no page of that size exists).
double CTextureCache::TestUploadSpeed(int32_t size, int32_t count) {
    int32_t flag = ConvertSurfaceSizeToFlag(size);
    if (mPageCount[flag] <= 0) return 0.0f;
    int32_t bytes = kUploadBytes[gei_TextureSettings.mFormat];
    CTextureBase* texture = mPages[flag][0]->mTexture;
    gei_TextureSettings.mFlags |= 0x20000000u;
    int32_t total = count * (size * size * bytes);
    int32_t start = gProfiler.GetClockCycleCount10h();
    for (int32_t n = count;;) {
        bool last = (n == 0);
        n--;
        if (last) {
            int32_t end = gProfiler.GetClockCycleCount10h();
            gei_TextureSettings.mFlags &= 0xdfffffffu;
            int32_t ticks = (end - start) * 16;
            return static_cast<float>(static_cast<float>(total) / static_cast<float>(ticks));
        }
        if (texture->LockSurface() == 0) {
            gei_TextureSettings.mFlags &= 0xdfffffffu;
            return 0.0f;
        }
        if (texture->UnlockSurface() == 0) {
            gei_TextureSettings.mFlags &= 0xdfffffffu;
            return 0.0f;
        }
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

int32_t CTextureCache::Refresh() {
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0)
        mFlags |= kUnlocking;
    else
        mFlags &= ~static_cast<uint32_t>(kUnlocking);
    CycleFrequencyTable();
    for (int32_t size = 0; size < kSizes; size++) {
        for (int32_t page = 0; page < mPageCount[size]; page++) {
            CTextureLink* link = mPages[size][page];
            for (CTextureEntry* entry = link->mEntries; entry != nullptr; entry = entry->mNext) {
                entry->mFlags &= ~kEntryFlags2000;
                // The place record keeps a count per frame (0..15): this frame's goes out of
                // the total and starts again from 0.
                STexturePlace* place = entry->mPlace;
                place->mF10 -= place->mF00[gFrequencyIndex];
                place->mF00[gFrequencyIndex] = 0;
                place->mF14 >>= 1;
                entry->mFlags &= ~kEntryFlags40000000;
                entry->mPriority = 1.0f;
                if ((mFlags & kUnlocking) != 0) entry->Unlock();
            }
            if ((mFlags & kUnlocking) != 0) link->mTexture->PositionUnlock();
        }
    }
    for (CTextureEntry* entry = mF18AC; entry != nullptr; entry = entry->mNext) entry->mFlags &= ~kEntryFlags2000;
    mF18AC = nullptr;
    return 1;
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

int32_t CTextureCache::ReInitialize(int32_t format) {
    if ((mFlags & kReady) != 0) Close();
    if (format != -1) gei_TextureSettings.mFormat = format;
    return AutoCreateSurfaces();
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

// The first call starts the file over ("w+b") with the texture settings and the first word
// of the system and display objects (their flags); later calls append. Then one record: a tag
// byte 1 and the entry.
void CTextureCache::DumpTCStreamToLog_Prepare(CTextureEntry* entry) {
    const uint8_t tag = 1;
    FILE* file;
    if ((mFlags & kCacheStreamStarted) == 0) {
        mFlags |= kCacheStreamStarted;
        file = std::fopen(kStreamName, "w+b");
        if (file != nullptr) {
            std::fwrite(&gei_TextureSettings, 1, 0x50, file);
            std::fwrite(&gei_System, 1, 4, file);
            std::fwrite(&gei_Display, 1, 4, file);
        }
    } else {
        file = std::fopen(kStreamName, "a");
    }
    if (file != nullptr) {
        SStreamRecord record;
        record.mEntry = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry));
        record.mFlags = entry->mFlags;
        record.mWidth = entry->mWidth;
        record.mHeight = entry->mHeight;
        record.mPriority = entry->mPriority;
        std::fwrite(&tag, 1, 1, file);
        std::fwrite(&record, 1, sizeof record, file);
        std::fflush(file);
        std::fclose(file);
    }
}

// The marker for "Execute ran". The Mac stores the word 0x10 and writes its FIRST byte,
// which is 0 on a big-endian machine (the byte 0x10 is the fourth), so that is what the
// stream holds there.
void CTextureCache::DumpTCStreamToLog_Execute() {
    const uint8_t tag = 0;
    FILE* file = std::fopen(kStreamName, "a");
    if (file != nullptr) {
        std::fwrite(&tag, 1, 1, file);
        std::fflush(file);
        std::fclose(file);
    }
}

// ---- The queue ---------------------------------------------------------------------------

void CTextureCache::Entry_QSortInit(CTextureEntry** entries, int32_t low, int32_t high) {
    if (low < high) {
        void* scratch;
        gQSort.pulAsc(entries, &scratch, static_cast<int32_t>(sizeof(CTextureEntry)),
                      static_cast<int32_t>(offsetof(CTextureEntry, mSortKey)), low, high);
    }
}

// The texture cache's last two helpers (engine library of SimTheme Park 2.04b3; the inventory
// files the cache under Wavelet.cpp):
//
//   0x10034e94 CTextureCache::Entry_Specific_Compress   (616 bytes)
//   0x1003770c CTextureCache::BalanceCache_ClearTexture (176)
//
// Entry_Specific_Compress is what AutoMipmap and Entry_Compress call (src/tb_texturecache_balance.cpp);
// BalanceCache_ClearTexture is what the two balancing steps call on the pages they free.

namespace {

// Entry flags this file reads; the named ones are in tb_textureentry.h.
constexpr uint32_t kEntryDetail = 0x000000c0;       // how large the entry's detail class is (0, 0x40, 0x80, 0xc0)
constexpr uint32_t kEntryStoreLevel = 0x00000030;   // TEFLAG_ENTRY_MIPMAP_STORE_FILTER: how many cuts are left (0..3)
constexpr uint32_t kEntryStoreStep = 0x00000010;    // one cut
constexpr uint32_t kEntryAskedFor = 0x40000000;     // asked for this frame (Prepare sets it, Refresh clears it)
// The smallest page size: the last word of the page size table (engine data 0x10051f28, a copy of
// the table that lies at 0x10051f40: 256 down to 8). A wrapping entry is cut only while it is wider.
constexpr int32_t kSmallestPage = 8;

// The twelve levels (engine data 0x10051ec8, pairs of words): an entry is cut at a level when its
// detail class (flags & 0xc0) is at least the first word and its priority lies above
// mF18EC[second] and no higher than mF18EC[second + 1] (the cache's priority limits).
struct SLevel {
    int32_t mMinDetail;
    int32_t mLimit;
};
constexpr SLevel kLevels[12] = {{192, 3}, {192, 2}, {128, 3}, {192, 1}, {128, 2}, {64, 3},
                                {192, 0}, {128, 1}, {64, 2},  {128, 0}, {64, 1},  {64, 0}};

// An entry's sort key: the priority (nearer is larger) in bits 16..27, the cache size
// (width * 256 + height) added, and a bit each for a locked entry and for the 0x80000 kind.
// The same as CTextureCache::Remove's and Prepare's (src/tb_texturecache_frame2.cpp).
void RebuildSortKey_3(CTextureEntry* entry) {
    entry->mSortKey = static_cast<int32_t>(
        (static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & CTextureEntry::kLocked) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & kEntrySortBlend) != 0) entry->mSortKey |= 0x40000000;
}

// The test of tb_textureentry.h that is inline in the original: is there a cut left? A wrapping
// entry also stops at the smallest page size.
bool CanBeCutDown(CTextureEntry* entry) {
    if (entry->StoreAsWrapping() != 0) return (entry->mFlags & kEntryStoreLevel) != 0 && entry->mCacheWidth > kSmallestPage;
    return (entry->mFlags & kEntryStoreLevel) != 0;
}

}  // namespace

// Cuts the entries of the array down one mipmap level (half the width and height, rounding up)
// when the level asks for them, and takes the cut area off the cache's total (mF18C4). It stops
// early once the total is below `available` (a signed compare; AutoMipmap passes INT32_MIN, which
// no total is below). An entry whose priority is not a number is never cut. A wrapping entry that
// is already as small as the smallest page is passed over without a change (and the total stays).
// Levels above 11 read other data in the original (level 12 passes over every entry, as the
// first word of its pair is 256) and are not reproduced beyond that.
void CTextureCache::Entry_Specific_Compress(CTextureEntry** entries, int32_t count, int32_t level, int32_t available) {
    if (level < 0 || level > 12) return;
    // The priority limits are the consecutive floats from mF18EC (0x18ec, 0x18f0, ...).
    const float* limits = &mF18EC;
    for (int32_t i = 0; i < count; ++i) {
        CTextureEntry* entry = entries[i];
        if (level == 12 || static_cast<int32_t>(entry->mFlags & kEntryDetail) < kLevels[level].mMinDetail) continue;
        const float priority = entry->mPriority;
        if (!(priority > limits[kLevels[level].mLimit])) continue;
        if (!(priority <= limits[kLevels[level].mLimit + 1])) continue;
        if (!CanBeCutDown(entry)) continue;

        entry = entries[i];
        const int32_t oldArea = entry->mCacheWidth * entry->mCacheHeight;
        if ((entry->mFlags & kEntrySortBlend) != 0) continue;
        // The next inline function of tb_textureentry.h (line 0x165): the cut itself. Every
        // caller has tested CanBeCutDown, so the assertion cannot fire here.
        if ((entry->mFlags & kEntryStoreLevel) == 0) {
            msl_assertion_failed("(md_Flags&TEFLAG_ENTRY_MIPMAP_STORE_FILTER)>TEFLAG_ENTRY_MIPMAP_STORE_LEVEL0",
                                 "tb_textureentry.h", 0x165);
        }
        if (entry->StoreAsWrapping() != 0 && entry->mCacheWidth == kSmallestPage) continue;
        entry->mFlags -= kEntryStoreStep;
        entry->mCacheWidth = static_cast<int16_t>((entry->mCacheWidth + 1) >> 1);
        entry->mCacheHeight = static_cast<int16_t>((entry->mCacheHeight + 1) >> 1);
        RebuildSortKey_3(entry);

        const int32_t newArea = entries[i]->mCacheWidth * entries[i]->mCacheHeight;
        mF18C4 = mF18C4 - (oldArea - newArea);
        if (mF18C4 < available) i = count;
    }
}

// tb_texturecache_balance.cpp (engine library of SimTheme Park 2.04b3; the inventory files the
// texture cache under Wavelet.cpp and CycleFrequencyTable under tb_hfield.cpp): the last pieces of
// the texture cache's frame that the height field's callers wait for.
//
//   0x10009770 CycleFrequencyTable                          (360 bytes)
//   0x10038670 CTextureLink::CalcUsage                      (544)
//   0x10035244 CTextureCache::AutoMipmap                    (120)
//   0x1003514c CTextureCache::Entry_Compress                (176)
//   0x1003780c CTextureCache::BalanceCache_FreeLockedTextureSpace   (392)
//   0x100379e0 CTextureCache::BalanceCache_LevelSurfaceUsage        (2,024)
//
// CTextureCache::Execute runs the two balancing steps between sorting the pages and sorting the
// queue; SortEntries calls the two shrinkers; SortTextures and PackTextures call CalcUsage.
// The functions they call and that are not translated yet (Entry_Specific_Compress 0x10034e94 and
// BalanceCache_ClearTexture 0x1003770c) are declared in tb_texturecache.h.

namespace {

// The side of a page of each size (engine data 0x10051f40: 256 down to 8; a second copy of the
// table lies at 0x10051f58). The last one is also the smallest width a wrapping entry is cut to.
constexpr int32_t kPageSides[CTextureCache::kSizes] = {256, 128, 64, 32, 16, 8};
constexpr uint32_t kEntryCompressedImage = 0x10000000;
// The cache's lists of pages to clear: up to 16 (the original's arrays), one more for the slot a
// full list's insertion writes just past its end.
constexpr int32_t kMaxClear = 16;
constexpr int32_t kClearSlots = kMaxClear + 1;

// Whether cutting this entry down a level is still possible: it has a store filter level left,
// and a wrapping entry is also still wider than the smallest page.
bool HasCutLeft(CTextureEntry* entry) {
    if (entry->StoreAsWrapping() != 0) return (entry->mFlags & kEntryStoreLevel) != 0 && entry->mCacheWidth > kSmallestPage;
    return (entry->mFlags & kEntryStoreLevel) != 0;
}

// Puts (link, key) into the lists `links` and `keys`, which hold `count` items and stay in order
// of the key (smallest first, or largest first with `descending`); an item equal to others goes
// after them. The lists keep at most `limit` items: past that the last one falls off. The
// original moves limit - position items each time (the ones past the end are whatever the stack
// held) and writes the new item at its position even when that is `limit`, one past the list.
void InsertSorted(CTextureLink** links, int32_t* keys, int32_t& count, int32_t limit, CTextureLink* link, int32_t key,
                  bool descending) {
    int32_t position = count;
    while (position != 0 && (descending ? key > keys[position - 1] : key < keys[position - 1])) --position;
    // A negative limit would make the original's memcpy run with a size of about 4 GB; the
    // usage ratios cannot lead there (that takes a difference of eight or more).
    if (limit - position > 0) {
        std::memmove(links + position + 1, links + position, static_cast<size_t>(limit - position) * sizeof(links[0]));
        std::memmove(keys + position + 1, keys + position, static_cast<size_t>(limit - position) * sizeof(keys[0]));
    }
    if (count < limit) ++count;
    links[position] = link;
    keys[position] = key;
}

}  // namespace

// Compresses further: from level mF18E0 on (nothing when it is 12), each level asked to fit the
// pixels left (mF18C0 - mF18BC, worked out once), until the entries' area mF18C4, which each pass
// changes, fits or the levels (up to 11) are used up. With cache flag 4 the area must also be
// within mF18E4.
void CTextureCache::Entry_Compress(CTextureEntry** entries, int32_t count) {
    if (mF18E0 == 12) return;
    int32_t level = mF18E0;
    const int32_t available = static_cast<int32_t>(static_cast<uint32_t>(mF18C0) - static_cast<uint32_t>(mF18BC));
    bool again = true;
    while (again) {
        Entry_Specific_Compress(entries, count, level, available);
        if (mF18C4 > available || ((mFlags & 4) != 0 && mF18E4 < mF18C4)) {
            ++level;
            if (level >= 12) again = false;
        } else {
            again = false;
        }
    }
}

// Cuts the entries down one mipmap level at a time, level by level: levels 0 to mF18E0 - 1, each
// asked to fit no particular area (INT32_MIN).
void CTextureCache::AutoMipmap(CTextureEntry** entries, int32_t count) {
    for (int32_t level = 0; level < mF18E0; ++level) Entry_Specific_Compress(entries, count, level, INT32_MIN);
}

void CTextureCache::SortEntries() {
    constexpr int32_t limit = kSortLimit;
    CTextureEntry* entries[kSortLimit];
    void* scratch;
    int32_t count = 0;
    float highest = 0.0f;
    CTextureEntry* entry = mF18AC;
    while (entry != nullptr && count < limit) {
        entries[count++] = entry;
        if (highest < entry->mPriority) highest = entry->mPriority;
        entry = entry->mNext;
    }
    // What does not fit the array is dropped from the queue.
    for (; entry != nullptr; entry = entry->mNext) ResetEntry(entry);

    // Priority limits for the balancing that follows: from just under 0 to just over the
    // highest priority, and three points an eighth, a quarter and a half of the way.
    mF18EC = -0.001f;
    mF18FC = 0.001f + highest;
    mF18F0 = ppc_fmadds(0.125f, mF18FC - mF18EC, mF18EC);
    mF18F4 = ppc_fmadds(0.25f, mF18FC - mF18EC, mF18EC);
    mF18F8 = ppc_fmadds(0.5f, mF18FC - mF18EC, mF18EC);
    if (count > 1) {
        gQSort.pfDes(entries, &scratch, static_cast<int32_t>(sizeof(CTextureEntry)),
                     static_cast<int32_t>(offsetof(CTextureEntry, mPriority)), 0, count - 1);
    }

    mF18C4 = 0;
    for (int32_t k = 0; k < count; ++k) mF18C4 += entries[k]->mWidth * entries[k]->mHeight;
    AutoMipmap(entries, count);

    // The pages can hold 1.5 times the pixels asked for? If not, or if the settings put a
    // limit on the area and it is over, the entries are compressed and, with the limit, the
    // ones at the end of the priority order are flagged to be left out until it holds.
    bool overLimit = false;
    const int32_t area = mF18C4;
    const int32_t wanted = ppc_fctiwz(1.5f * static_cast<float>(area));
    const int32_t available = mF18C0 - mF18BC;
    if ((mFlags & kCacheLimited) != 0 && mF18E4 < area) overLimit = true;
    if (wanted > available || overLimit) {
        Entry_Compress(entries, count);
        if ((mFlags & kCacheLimited) != 0 && mF18E4 < mF18C4) {
            CTextureEntry** at = entries + count;
            int32_t left = count;
            for (;;) {
                const bool none = left == 0;
                --at;
                --left;
                if (none || mF18C4 <= mF18E4) break;
                CTextureEntry* dropped = *at;
                int32_t freed;
                if ((dropped->mFlags & kEntryBlendKind) == 0) {
                    dropped->mFlags |= kEntryDropped;
                    freed = CacheArea(dropped);
                } else {
                    freed = 0;
                }
                mF18C4 -= freed;
            }
        }
    }

    // The queue again, largest sort key first. Entries flagged to be left out are reset
    // (their chain pointer is left as it was).
    if (count - 1 > 0) {
        gQSort.pulAsc(entries, &scratch, static_cast<int32_t>(sizeof(CTextureEntry)),
                      static_cast<int32_t>(offsetof(CTextureEntry, mSortKey)), 0, count - 1);
    }
    mF18AC = nullptr;
    for (int32_t k = 0; k < count; ++k) {
        CTextureEntry* queued = entries[k];
        if ((queued->mFlags & kEntryDropped) == 0) {
            queued->mNext = mF18AC;
            mF18AC = queued;
        } else {
            ResetEntry(queued);
        }
    }
}

// ---- Lists of entries ----------------------------------------------------------------------

void CTextureCache::SortByCriteria(CTextureEntry** list) {
    constexpr int32_t limit = kSortLimit;
    CTextureEntry* entries[kSortLimit];
    int32_t count = 0;
    CTextureEntry* entry = *list;
    while (entry != nullptr && count < limit) {
        entries[count++] = entry;
        entry = entry->mNext;
    }
    // What does not fit the array loses its page bits and is not in the list any more.
    if (count == limit) {
        for (; entry != nullptr; entry = entry->mNext) entry->mFlags &= kEntryPageBits;
    }
    Entry_QSortInit(entries, 0, count - 1);
    CTextureEntry* previous = nullptr;
    for (int32_t k = 0; k < count; ++k) {
        entries[k]->mNext = previous;
        previous = entries[k];
    }
    *list = previous;
}

void CTextureCache::SortByAddress(CTextureEntry** list) {
    constexpr int32_t limit = kSortLimit;
    CTextureEntry* entries[kSortLimit];
    CTextureEntry* scratch;
    int32_t count = 0;
    CTextureEntry* entry = *list;
    while (entry != nullptr && count < limit) {
        entries[count++] = entry;
        entry = entry->mNext;
    }
    if (count == limit) {
        for (; entry != nullptr; entry = entry->mNext) entry->mFlags &= kEntryPageBits;
    }
    // The records are the pointers themselves, the key their low 32 bits.
    if (count > 1) gQSort.ulAsc(entries, &scratch, static_cast<int32_t>(sizeof(CTextureEntry*)), 0, 0, count - 1);
    CTextureEntry* next = nullptr;
    for (int32_t k = count - 1; k >= 0; --k) {
        entries[k]->mNext = next;
        next = entries[k];
    }
    *list = next;
}

void CTextureCache::MergeListsConditional(CTextureEntry** list, CTextureEntry* entries) {
    CTextureEntry* entry = entries;
    while (entry != nullptr) {
        // An entry must not be locked unless the cache is reshuffling. The assertion
        // fails and the function carries on.
        if ((mFlags & kCacheReshuffle) == 0 && (entry->mFlags & kEntryLocked) != 0) {
            msl_assertion_failed("(md_Flags&CACHEFLAG_RESHUFFLE) || (!curr_src->IsLocked())", "tb_texturecache_upload.cpp",
                                 0x95);
        }
        entry->FlushEntryFlags();
        entry->mTexture = nullptr;
        if ((entry->mFlags & kEntryAsked) == 0) {
            // Not asked for this frame: back to the state of an entry on no page, and this
            // frame's use leaves the place record (the count of the byte, the sum, half the score).
            ResetEntry(entry);
            STexturePlace* place = entry->mPlace;
            place->mF10 -= place->mF00[gFrequencyIndex];
            place->mF00[gFrequencyIndex] = 0;
            place->mF14 >>= 1;
            entry = entry->mNext;
        } else {
            // Asked for: queued, and put in its place in the list, behind the entries of a
            // larger sort key (signed) and in front of the first that is not larger.
            entry->mFlags |= kEntryQueued;
            CTextureEntry* before = nullptr;
            CTextureEntry* after = *list;
            while (after != nullptr && after->mSortKey > entry->mSortKey) {
                before = after;
                after = after->mNext;
            }
            CTextureEntry* following = entry->mNext;
            entry->mNext = after;
            if (before == nullptr) *list = entry;
            else before->mNext = entry;
            entry = following;
        }
    }
}

int32_t CTextureCache::CreateNonWrappingTextureList(CTextureEntry** out, CTextureEntry* entries, CTextureLink* page) {
    int32_t total = 0;
    CTextureEntry* onPage = page->mEntries;
    if (onPage != nullptr && onPage->StoreAsWrapping() != 0) onPage = onPage->mNext;
    // Both chains are in decreasing height, then width. Merge them, leaving out the entries
    // of the chain that are stored as wrapping and the ones on the page that were not asked
    // for. (The original also tracks the widest and the tallest cache size; nothing reads them.)
    while (onPage != nullptr && entries != nullptr) {
        while (onPage != nullptr && (onPage->mFlags & kEntryAsked) == 0) onPage = onPage->mNext;
        if (onPage == nullptr) continue;
        while (entries != nullptr && entries->StoreAsWrapping() != 0) entries = entries->mNext;
        if (entries == nullptr) continue;
        if (entries->mHeight > onPage->mHeight || (entries->mHeight == onPage->mHeight && entries->mWidth > onPage->mWidth)) {
            *out++ = entries;
            total += CacheArea(entries);
            entries = entries->mNext;
        } else {
            *out++ = onPage;
            total += CacheArea(onPage);
            onPage = onPage->mNext;
        }
    }
    for (; entries != nullptr; entries = entries->mNext) {
        if (entries->StoreAsWrapping() == 0) {
            *out++ = entries;
            total += CacheArea(entries);
        }
    }
    for (; onPage != nullptr; onPage = onPage->mNext) {
        if ((onPage->mFlags & kEntryAsked) != 0) {
            *out++ = onPage;
            total += CacheArea(onPage);
        }
    }
    *out = nullptr;
    return total;
}

// tb_texturecache_pack.cpp (engine library of SimTheme Park 2.04b3, the texture cache's
// packing; the inventory files the range under Wavelet.cpp, the original file was
// tb_texturecache_upload.cpp by its assertions): CTextureCache::ArrangeNonwrappingTextures
// 0x10036100 and PackTextures 0x1003678c.
//
// GenerateTexturePages calls PackTextures on the queue of entries the frame asked for
// (Prepare). PackTextures picks one page (the first of the entry's size, or of a larger size
// when the pages of that size are all taken), lets ArrangeNonwrappingTextures lay the queue's
// entries out on it, and uploads them with UploadToVRAM. An entry that is stored as
// wrapping takes a whole page by itself. When the entries do not fit they are cut down: each
// cut halves the cache size and takes 0x10 off the flags' "store level" (bits 4 and 5), so an
// entry can be cut three times.
//
// The sorting and list helpers (SortByCriteria, SortByAddress, MergeListsConditional,
// CreateNonWrappingTextureList, CQSort::ulDes) are not in this file.

int32_t gTextureQuadsPending = 0;
int32_t gTextureQuadsPlaced = 0;
int32_t gTextureSizeSteps = 0;
int32_t gTextureCutDowns = 0;

namespace {

// Quads one arrangement can hold, with the null one that ends them: ArrangeNonwrappingTextures
// stops after 0x7f entries.
constexpr int32_t kMaxQuads = 0x80;
constexpr int32_t kMaxPlaced = 0x7f;

// The list of entries for one page: up to 4096 pointers.
constexpr int32_t kMaxCandidates = 4096;

// The free rectangles ArrangeNonwrappingTextures cuts a page into: 0x14 bytes, the key sorts
// them (width * 65536 + height, largest first).
struct SFreeRect {
    int32_t mX;
    int32_t mY;
    int32_t mWidth;
    int32_t mHeight;
    uint32_t mKey;
};
static_assert(sizeof(SFreeRect) == 0x14, "SFreeRect layout");

uint32_t FreeKey(int32_t width, int32_t height) {
    return (static_cast<uint32_t>(width) << 16) + static_cast<uint32_t>(height);
}

// The cut itself, the next inline function of tb_textureentry.h (line 0x165): halves the cache
// size (rounding up), takes a store level off and builds the sort key again. A wrapping entry
// of the smallest size is not changed. Every caller has tested CanBeCutDown, so the assertion
// cannot fire there.
void CutDown(CTextureEntry* entry) {
    if ((entry->mFlags & kEntryStoreLevel) == 0) {
        msl_assertion_failed("(md_Flags&TEFLAG_ENTRY_MIPMAP_STORE_FILTER)>TEFLAG_ENTRY_MIPMAP_STORE_LEVEL0",
                             "tb_textureentry.h", 0x165);
    }
    if (entry->StoreAsWrapping() != 0 && entry->mCacheWidth == kSmallestPage) return;
    entry->mFlags -= kEntryStoreStep;
    entry->mCacheWidth = static_cast<int16_t>((entry->mCacheWidth + 1) >> 1);
    entry->mCacheHeight = static_cast<int16_t>((entry->mCacheHeight + 1) >> 1);
    RebuildSortKey_3(entry);
}

// The pages of `size` that the packing has not filled yet this frame.
int32_t PagesLeft(const CTextureCache& cache, int32_t size) {
    return cache.mUsable[size] - cache.mF1830[size];
}

}  // namespace

bool CTextureCache::ArrangeNonwrappingTextures(CTextureEntry** list, SAvailableQuad* quads, int32_t* count,
                                               CTextureBase* page, int32_t) {
    SFreeRect spaces[kMaxQuads + 1];
    SFreeRect scratch;
    int32_t rects = 1;
    int32_t placed = 0;
    bool complete = true;
    spaces[0].mX = 0;
    spaces[0].mY = 0;
    spaces[0].mWidth = page->mWidth;
    spaces[0].mHeight = page->mHeight;
    spaces[0].mKey = FreeKey(page->mWidth, page->mHeight);
    mReordered = 0;
    mTallest = 0;
    mWidest = 0;
    gTextureQuadsPending = 0;

    SAvailableQuad* quad = quads;
    CTextureEntry* entry;
    while ((entry = *list) != nullptr && rects != 0 && placed < kMaxPlaced) {
        // The smallest free rectangle the entry fits (they are sorted largest first). With
        // mipmaps the slot is the size rounded up with the mask.
        const int32_t width = entry->mCacheWidth;
        const int32_t height = entry->mCacheHeight;
        int32_t index = rects - 1;
        for (; index >= 0; --index) {
            const SFreeRect& rect = spaces[index];
            if (mMipmaps == 0) {
                if (width <= rect.mWidth && height <= rect.mHeight) break;
            } else {
                if (((width + mSizeMask) & ~mSizeMask) <= rect.mWidth &&
                    ((height + mSizeMask) & ~mSizeMask) <= rect.mHeight) {
                    break;
                }
            }
        }

        // The first entry fits nowhere on an empty page: cut it down, put it back among the
        // entries behind it by its new key and try the list again from the start.
        if (placed == 0 && index == -1 && (mFlags & kReshuffle) != 0 && CanBeCutDown(entry)) {
            CutDown(entry);
            CTextureEntry** at = list + 1;
            while (*at != nullptr && (*at)->mSortKey > entry->mSortKey) ++at;
            const std::ptrdiff_t moved = (at - list) - 1;
            if (moved != 0) {
                std::memmove(list, list + 1, static_cast<size_t>(moved) * sizeof(CTextureEntry*));
                at[-1] = entry;
            }
            mReordered = 1;
            continue;
        }

        if (mWidest < entry->mCacheWidth) mWidest = entry->mCacheWidth;
        if (mTallest < entry->mCacheHeight) mTallest = entry->mCacheHeight;
        if (index < 0) {
            complete = false;
        } else {
            SFreeRect& rect = spaces[index];
            int32_t quadWidth;
            int32_t quadHeight;
            if ((mFlags & kFlag2) == 0) {
                quadWidth = entry->mCacheWidth;
                quadHeight = entry->mCacheHeight;
                quad->mX = rect.mX;
                quad->mY = rect.mY;
            } else {
                // The slot is the size rounded up and the image sits in its middle.
                const int32_t padWidth = ((mSizeMask + entry->mCacheWidth) & ~mSizeMask) - entry->mCacheWidth;
                const int32_t padHeight = ((mSizeMask + entry->mCacheHeight) & ~mSizeMask) - entry->mCacheHeight;
                quad->mX = (padWidth >> 1) + rect.mX;
                quad->mY = (padHeight >> 1) + rect.mY;
                quadWidth = padWidth + entry->mCacheWidth;
                quadHeight = padHeight + entry->mCacheHeight;
            }
            quad->mWidth = quadWidth;
            quad->mHeight = quadHeight;
            quad->mEntry = entry;
            ++quad;
            ++placed;

            // What is left of the rectangle: a strip to the right (the full height), one below
            // (the quad's width), both, or nothing when each is 8 or less.
            const int32_t restWidth = rect.mWidth - quadWidth;
            const int32_t restHeight = rect.mHeight - quadHeight;
            const int32_t shape = (restWidth > 8 ? 2 : 0) | (restHeight > 8 ? 1 : 0);
            switch (shape) {
            case 3: {
                SFreeRect& below = spaces[rects++];
                below = rect;
                below.mY += quadHeight;
                below.mHeight -= quadHeight;
                below.mWidth = quadWidth;
                below.mKey = FreeKey(below.mWidth, below.mHeight);
            }
                [[fallthrough]];
            case 2:
                rect.mX += quadWidth;
                rect.mWidth -= quadWidth;
                rect.mKey = FreeKey(rect.mWidth, rect.mHeight);
                break;
            case 1:
                rect.mY += quadHeight;
                rect.mHeight -= quadHeight;
                rect.mKey = FreeKey(rect.mWidth, rect.mHeight);
                break;
            default:
                --rects;
                rect = spaces[rects];
                break;
            }
            if (rects - index > 1) {
                gQSort.ulDes(spaces, &scratch, static_cast<int32_t>(sizeof(SFreeRect)),
                              static_cast<int32_t>(offsetof(SFreeRect, mKey)), index, rects - 1);
            }
            ++gTextureQuadsPending;
            ++gTextureQuadsPlaced;
        }
        ++list;
    }
    quad->mEntry = nullptr;
    *count = placed;
    return complete && entry == nullptr;
}

int32_t CTextureCache::PackTextures(CTextureEntry** list) {
    tpw_probe::Hit(tpw_probe::kPackTextures);
    CTextureEntry* first = *list;

    // The size to start from: a wrapping entry's own, else the largest. Then the first size from
    // there on that still has a page to fill. (A best size of 6 or 7 from the flags would read past
    // the tables; the original does the same for 7.)
    int32_t size = first->StoreAsWrapping() != 0 ? first->GetBestSize() : 0;
    while (size < kSizes && mF1830[size] >= mUsable[size]) ++size;
    if (size >= kSizes) return -1;

    CTextureLink* link = mPages[size][mF1830[size]];
    CTextureBase* texture = link->mTexture;

    if (first->StoreAsWrapping() != 0) {
        // A wrapping entry takes a page of its own. Cut it down until it is not wider than the page.
        const int32_t pageWidth = texture->mWidth;
        while (CanBeCutDown(first) && pageWidth < first->mCacheWidth) {
            CutDown(first);
            ++gTextureCutDowns;
        }
        if (pageWidth < first->mCacheWidth) return -1;

        // Cut down once more, onto a page one size smaller, when that size has more of its pages
        // free (in proportion) than this one.
        if (CanBeCutDown(first) && size < kSizes - 1) {
            const float freeHere = static_cast<float>(PagesLeft(*this, size)) / static_cast<float>(mPageCount[size]);
            const float freeNext =
                static_cast<float>(PagesLeft(*this, size + 1)) / static_cast<float>(mPageCount[size + 1]);
            if (!(freeHere > freeNext)) {
                ++size;
                CutDown(first);
                link = mPages[size][mF1830[size]];
                texture = link->mTexture;
            }
        }

        // The page's entries go back on the queue and the entry takes the whole page.
        *list = (*list)->mNext;
        MergeListsConditional(list, link->mEntries);
        link->mEntries = nullptr;
        ++mF1830[size];
        texture->SetFlags(0);
        if (texture->LockSurface() == 0) return 0;
        SAvailableQuad wholePage;
        wholePage.mX = 0;
        wholePage.mY = 0;
        wholePage.mWidth = texture->mWidth;
        wholePage.mHeight = texture->mHeight;
        wholePage.mEntry = first;
        if (UploadToVRAM(link, &wholePage, nullptr) == 0) return 0;
        link->mEntries = first;
        first->mNext = nullptr;
        // Bit 1 of the page's flags: wrapping; 1: locked in place; 4: nothing left to cut.
        int32_t pageFlags = (first->mFlags & CTextureEntry::kLocked) != 0 ? 3 : 2;
        if (!CanBeCutDown(first)) pageFlags |= 4;
        texture->SetFlags(pageFlags);
        return link->PageUpdated();
    }

    // Other entries share the page: lay out the queue's and the page's own, on the page of this
    // size and then on those of each smaller size while the entries still fit.
    CTextureEntry* candidates[kMaxCandidates];
    SAvailableQuad quadsUsed[kMaxQuads];
    SAvailableQuad trialQuads[kMaxQuads];
    int32_t quadCount;
    int32_t trialCount;
    const int32_t preferredSize = size;
    int32_t bestSize = size;
    const int32_t candidateCount = CreateNonWrappingTextureList(candidates, first, link);
    bool fits = ArrangeNonwrappingTextures(candidates, quadsUsed, &quadCount, texture, 1);
    if (quadCount == 0) {
        SortByCriteria(list);
        return -1;
    }
    int32_t trialSize = size;
    while (fits) {
        fits = false;
        // Try the next smaller page only when the widest and tallest entry would still fit on half of this one.
        if ((texture->mWidth >> 1) >= mWidest && (texture->mHeight >> 1) >= mTallest) {
            ++gTextureSizeSteps;
            do {
                ++trialSize;
            } while (trialSize < kSizes && mF1830[trialSize] >= mUsable[trialSize]);
            if (trialSize < kSizes) {
                CTextureLink* trialLink = mPages[trialSize][mF1830[trialSize]];
                CTextureBase* trialTexture = trialLink->mTexture;
                CreateNonWrappingTextureList(candidates, first, trialLink);
                const int32_t savedPending = gTextureQuadsPending;
                fits = ArrangeNonwrappingTextures(candidates, trialQuads, &trialCount, trialTexture, 0);
                if (!fits) {
                    gTextureQuadsPending = savedPending;
                } else {
                    std::copy(trialQuads, trialQuads + trialCount + 1, quadsUsed);
                    quadCount = trialCount;
                    bestSize = trialSize;
                    link = trialLink;
                    texture = trialTexture;
                }
            }
        }
    }
    if (quadsUsed[quadCount].mEntry != nullptr) {
        msl_assertion_failed("sQuadsUsed[dwQuadsUsed].ps_TextureEntry==0", "tb_texturecache_upload.cpp", 0x34c);
    }
    if (mReordered != 0) SortByCriteria(list);

    // The arrangement was made for the page of bestSize (a smaller one than preferredSize). Use
    // that page unless the preferred size has far more of its pages free (in proportion, twice) and
    // the smaller pages are no more than a little over 3/4 of what it would take.
    if (preferredSize != bestSize) {
        if (!(bestSize != preferredSize && bestSize < kSizes && preferredSize < kSizes)) {
            msl_assertion_failed("(dwPreferredSize!=dwBestSize) && (dwPreferredSize<SURFACESIZES) && (dwBestSize<SURFACESIZES)",
                                 "tb_texturecache_upload.cpp", 0x120);
        }
        const float bestFree = static_cast<float>(PagesLeft(*this, bestSize)) / static_cast<float>(mPageCount[bestSize]);
        const float preferredFree =
            static_cast<float>(PagesLeft(*this, preferredSize)) / static_cast<float>(mPageCount[preferredSize]);
        const int32_t pagePixels = mPages[bestSize][0]->mTexture->GetPixelCount();
        bool keepBest = true;
        if (bestFree > 2.0f * preferredFree && mUsable[preferredSize] + 10 < mUsable[bestSize] &&
            pagePixels * 3 < candidateCount * 4) {
            keepBest = false;
        }
        bestSize = keepBest ? bestSize : preferredSize;
        link = mPages[bestSize][mF1830[bestSize]];
        texture = link->mTexture;
    }

    bool pageFree = true;
    if ((mFlags & kReshuffle) == 0 && texture->IsPositionLocked() != 0) pageFree = false;
    if (!pageFree) {
        msl_assertion_failed("(md_Flags&CACHEFLAG_RESHUFFLE) || (!psTexture->IsPositionLocked())",
                             "tb_texturecache_upload.cpp", 0x360);
    }

    // The page's entries go back on the queue; the quads are put in the order of their entries'
    // addresses (the queue is about to be sorted the same way).
    MergeListsConditional(list, link->mEntries);
    link->mEntries = nullptr;
    for (int32_t i = 0; i < quadCount - 1; ++i) {
        for (int32_t j = i + 1; j < quadCount; ++j) {
            if (reinterpret_cast<uintptr_t>(quadsUsed[i].mEntry) > reinterpret_cast<uintptr_t>(quadsUsed[j].mEntry)) {
                const SAvailableQuad swapped = quadsUsed[i];
                quadsUsed[i] = quadsUsed[j];
                quadsUsed[j] = swapped;
            }
        }
    }
    ++mF1830[bestSize];
    texture->SetFlags(0);
    if (texture->LockSurface() == 0) return 0;

    // Walk the queue in address order beside the quads: an entry that has a quad goes on the
    // page (at the end of its list) and off the queue.
    SortByAddress(list);
    SAvailableQuad* quad = quadsUsed;
    CTextureEntry* queueEntry = *list;
    CTextureEntry* queuePrevious = nullptr;
    CTextureEntry* pageLast = nullptr;
    bool nothingLeftToCut = true;
    CTextureEntry* target = quad->mEntry;
    while (queueEntry != nullptr && target != nullptr) {
        CTextureEntry* next = queueEntry->mNext;
        if (queueEntry == target) {
            if (CanBeCutDown(queueEntry)) nothingLeftToCut = false;
            if ((queueEntry->mFlags & CTextureEntry::kLocked) != 0) texture->PositionLock();
            if (UploadToVRAM(link, quad, nullptr) == 0) {
                texture->UnlockSurface();
                return 0;
            }
            if (queuePrevious != nullptr) {
                queuePrevious->mNext = next;
            } else {
                *list = (*list)->mNext;
            }
            if (pageLast != nullptr) {
                pageLast->mNext = queueEntry;
            } else {
                link->mEntries = queueEntry;
            }
            pageLast = queueEntry;
            queueEntry->mNext = nullptr;
            --gTextureQuadsPending;
            target = quad[1].mEntry;
            ++quad;
        } else {
            queuePrevious = queueEntry;
        }
        queueEntry = next;
    }
    if (nothingLeftToCut) texture->SetFlags(static_cast<int32_t>(texture->GetFlags() | 4));
    if (*list != nullptr) SortByCriteria(list);
    CTextureEntry* pageList = link->mEntries;
    SortByCriteria(&pageList);
    link->mEntries = pageList;
    return link->PageUpdated();
}

int32_t CTextureCache::GenerateTexturePages() {
    std::memset(mF1830, 0, sizeof mF1830);
    int32_t result = 1;
    // PackTextures takes entries off the queue; -1 means the first one fits nowhere, so
    // it is dropped back to the state of an entry on no page.
    while (mF18AC != nullptr && result != 0) {
        result = PackTextures(&mF18AC);
        if (result == -1) {
            ForgetPlacement(mF18AC);
            result = 1;
            mF18AC = mF18AC->mNext;
        }
    }
    // After a failure the rest of the queue is dropped the same way.
    for (CTextureEntry* entry = mF18AC; entry != nullptr; entry = entry->mNext) ForgetPlacement(entry);
    mF18AC = nullptr;
    return result;
}

// Takes every entry off the page: each loses the page bits of its flags and its texture, and one
// that was asked for this frame goes back on the front of the queue (mF18AC) flagged as queued
// (and locked again if it was locked). The page's texture loses its flags (the page is empty).
void CTextureCache::BalanceCache_ClearTexture(CTextureLink* link) {
    CTextureBase* texture = link->mTexture;
    CTextureEntry* entry = link->mEntries;
    link->mEntries = nullptr;
    while (entry != nullptr) {
        CTextureEntry* next = entry->mNext;
        const bool locked = (entry->mFlags & CTextureEntry::kLocked) != 0;
        entry->FlushEntryFlags();
        entry->mTexture = nullptr;
        if ((entry->mFlags & kEntryAskedFor) != 0) {
            entry->mFlags |= locked ? (kEntryQueued | CTextureEntry::kLocked) : kEntryQueued;
            entry->mNext = mF18AC;
            mF18AC = entry;
        }
        entry = next;
    }
    texture->SetFlags(0);
}

// Per size, up to mF1860 (but 16) pages that hold some locked entries, take less than a quarter
// of the page with them and have little the frame asked for (under half the page) are cleared,
// the ones asked for least first.
void CTextureCache::BalanceCache_FreeLockedTextureSpace() {
    int32_t limit = mF1860;
    if (limit > kMaxClear) limit = kMaxClear;
    for (int32_t size = 0; size < kSizes; ++size) {
        const int32_t pageArea = kPageSides[size] * kPageSides[size];
        CTextureLink* chosen[kClearSlots] = {};
        int32_t keys[kClearSlots] = {};
        int32_t count = 0;
        for (int32_t k = 0; k < mPageCount[size]; ++k) {
            CTextureLink* link = mPages[size][k];
            if (link->mEntries == nullptr) continue;
            const int32_t asked = link->mF0C;
            if (link->mF1C <= 0) continue;
            if (static_cast<int32_t>(static_cast<uint32_t>(link->mF1C) << 2) >= pageArea) continue;
            if (static_cast<int32_t>(static_cast<uint32_t>(asked) << 1) >= pageArea) continue;
            // The pages stay in increasing order of the pixels asked for; only `limit` are kept.
            int32_t position = count;
            while (position != 0 && asked < keys[position - 1]) --position;
            if (count - position != 0) {
                std::memmove(chosen + position + 1, chosen + position, static_cast<size_t>(count - position) * sizeof(chosen[0]));
                std::memmove(keys + position + 1, keys + position, static_cast<size_t>(count - position) * sizeof(keys[0]));
            }
            if (count < limit) ++count;
            chosen[position] = link;
            keys[position] = asked;
        }
        for (int32_t k = count - 1; k >= 0; --k) BalanceCache_ClearTexture(chosen[k]);
    }
}

// Evens out the use of the six page sizes. A size's share is mF1818 over its page count: SortTextures
// leaves in mF1818 the index of the last page that is at most half used this frame (the pages are in
// increasing use), so the share grows with the pages that still have room. Only a size with a share
// under 0.67 is touched, and then in up to two ways:
//  - its share is under two thirds of the previous size's (the larger pages): up to `limit` of its pages
//    from index mF1818 on, not wrapping and not locked (unless the cache is reshuffling), those with the
//    fewest entries asked for this frame, are cleared, so that their entries can move up a size;
//  - its share is under two thirds of the next size's (the smaller pages): up to `limit` of its pages from
//    that index on that are wrapping with room to cut, or not wrapping and under half used, ranked by the
//    first entry (far away, and with cuts left, comes first), have each entry cut down one level (a store
//    filter step, the cache size halved, the sort key built again) and are cleared, so that the entries
//    can move down a size.
// `limit` is a quarter of the difference of the two shares, plus one, at most 16. When any page was
// cleared the pages and the queue are sorted again. Only for the renderers 1 and 3. (The original leaves 1 in
// r3 whatever happens; the caller ignores it.)
void CTextureCache::BalanceCache_LevelSurfaceUsage() {
    if (gei_System.mRenderer != 1 && gei_System.mRenderer != 3) return;
    // How full each size is, in singles: the converted integers are exact below 2^24 and rounded
    // to the nearest above; 0 over 0 is a NaN, which no test below passes.
    float share[kSizes];
    for (int32_t size = 0; size < kSizes; ++size) share[size] = static_cast<float>(mF1818[size]) / static_cast<float>(mPageCount[size]);

    bool cleared = false;
    for (int32_t size = 0; size < kSizes; ++size) {
        const float here = share[size];
        if (!(here < 0.67f)) continue;
        const int32_t pageArea = kPageSides[size] * kPageSides[size];
        const bool fuller = size > 0 && 1.5f * here < share[size - 1];
        const bool emptier = size < kSizes - 1 && 1.5f * here < share[size + 1];

        if (fuller) {
            int32_t limit = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(0.25f * (here - share[size - 1]))) + 1u);
            if (limit > kMaxClear) limit = kMaxClear;
            int32_t first = mF1818[size];
            if (first < 0) first = 0;
            CTextureLink* chosen[kClearSlots] = {};
            int32_t keys[kClearSlots] = {};
            int32_t count = 0;
            for (int32_t k = mPageCount[size] - 1; k >= first; --k) {
                CTextureLink* link = mPages[size][k];
                CTextureBase* texture = link->mTexture;
                if (texture->IsPositionLocked() != 0 && (mFlags & kReshuffle) == 0) continue;
                if (texture->IsWrapping() != 0) continue;
                int32_t asked = 0;
                for (CTextureEntry* entry = link->mEntries; entry != nullptr; entry = entry->mNext) {
                    if ((entry->mFlags & kEntryAskedFor) != 0) ++asked;
                }
                InsertSorted(chosen, keys, count, limit, link, asked, false);
            }
            for (int32_t k = count - 1; k >= 0; --k) {
                BalanceCache_ClearTexture(chosen[k]);
                cleared = true;
            }
        }

        if (emptier) {
            int32_t limit = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(0.25f * (here - share[size + 1]))) + 1u);
            if (limit > kMaxClear) limit = kMaxClear;
            int32_t first = mF1818[size];
            if (first < 0) first = 0;
            CTextureLink* chosen[kClearSlots] = {};
            int32_t keys[kClearSlots] = {};
            int32_t count = 0;
            for (int32_t k = mPageCount[size] - 1; k >= first; --k) {
                CTextureLink* link = mPages[size][k];
                CTextureBase* texture = link->mTexture;
                if (texture->IsPositionLocked() != 0 && (mFlags & kReshuffle) == 0) continue;
                const bool wrapping = texture->IsWrapping() != 0;
                const bool wanted = wrapping ? texture->IsUsingMaxCompression() == 0
                                             : static_cast<int32_t>(static_cast<uint32_t>(link->mF0C) << 1) < pageArea;
                if (!wanted) continue;
                // The page's rank is its first entry's priority (higher for a nearer picture),
                // with the store filter bits as a tie-breaker; a page with no entry ranks 0.
                int32_t rank = 0;
                if (const CTextureEntry* head = link->mEntries) {
                    rank = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - head->mPriority))) +
                                                ((head->mFlags & kEntryStoreLevel) << 8));
                }
                InsertSorted(chosen, keys, count, limit, link, rank, true);
            }
            for (int32_t k = count - 1; k >= 0; --k) {
                CTextureLink* link = chosen[k];
                for (CTextureEntry* entry = link->mEntries; entry != nullptr; entry = entry->mNext) {
                    if (!HasCutLeft(entry)) continue;
                    // (the original asserts that a store filter step is left; it always is here)
                    if ((entry->mFlags & kEntryStoreLevel) == 0) {
                        msl_assertion_failed("(md_Flags&TEFLAG_ENTRY_MIPMAP_STORE_FILTER)>TEFLAG_ENTRY_MIPMAP_STORE_LEVEL0",
                                             "tb_textureentry.h", 0x165);
                    }
                    if (entry->StoreAsWrapping() != 0 && entry->mCacheWidth == kSmallestPage) continue;
                    entry->mFlags -= kEntryStoreStep;
                    entry->mCacheWidth = static_cast<int16_t>((entry->mCacheWidth + 1) >> 1);
                    entry->mCacheHeight = static_cast<int16_t>((entry->mCacheHeight + 1) >> 1);
                    RebuildSortKey_3(entry);
                }
                BalanceCache_ClearTexture(link);
                cleared = true;
            }
        }
    }
    if (cleared) {
        SortTextures();
        SortEntries();
    }
}

int32_t CTextureCache::UploadToVRAM(CTextureLink* link, SAvailableQuad* quad, CTextureEntry* entry) {
    tpw_probe::Hit(tpw_probe::kUploadToVRAM);
    if (link == nullptr) msl_assertion_failed("dest_link != NULL", "tb_texturecache_upload.cpp", 0x56f);
    if (entry == nullptr) entry = quad->mEntry;
    CTextureBase* texture = link->mTexture;
    texture->Download(entry, quad->mX, quad->mY);

    // The slot's corner and the size the image takes in it. An entry flagged to be centred
    // that is smaller than its cache size sits in the middle of the slot (once).
    int32_t width = entry->mCacheWidth;
    int32_t height = entry->mCacheHeight;
    int32_t x = quad->mX;
    int32_t y = quad->mY;
    if ((entry->mFlags & kEntryCentred) != 0) {
        if (width > entry->mWidth) {
            x += (width - entry->mWidth) >> 1;
            width = entry->mWidth;
        }
        if (height > entry->mHeight) {
            y += (height - entry->mHeight) >> 1;
            height = entry->mHeight;
        }
        entry->mFlags &= ~kEntryCentred;
    }
    entry->mFlags &= ~kEntryQueued;
    entry->mFlags |= CTextureEntry::kOnPage;
    entry->mPage = link;
    if (gei_System.mRenderer != CSystem::kRendererOpenGL) return 1;

    const CTextureSettings& settings = gei_TextureSettings;
    const float scale = mF18E8;
    const uint32_t flags = entry->mFlags;
    if ((flags & CTextureEntry::kWrapAny) != 0 && !((flags & kEntryFlag400) != 0 && entry->mF38 == nullptr)) {
        // A wrapping image covers the whole page: the offset is 0 along a direction that wraps
        // and mFloat18 texels (0.5) along the other; the size is what is left (1 - 2 * offset).
        float u = (flags & kEntryWrapU) != 0 ? 0.0f : settings.mFloat18;
        float v = (flags & kEntryWrapV) != 0 ? 0.0f : settings.mFloat18;
        u = u * (scale / static_cast<float>(texture->mWidth));
        v = v * (scale / static_cast<float>(texture->mHeight));
        entry->mUOffset = u;
        entry->mVOffset = v;
        entry->mUSize = ppc_fnmsubs(2.0f, u, 1.0f);
        entry->mVSize = ppc_fnmsubs(2.0f, v, 1.0f);
    } else {
        // The image's corner and size in fractions of the page, moved by the settings' fixups
        // when flag 0x4000000 asks (else by mFloat18, mFloat18, 0 and 0). The offsets are summed
        // as x + fixU first and lastU after (the decompiled C shows (lastU + x) + fixU, which
        // rounds differently).
        float fixU, fixV, lastU, lastV;
        if ((flags & kEntryFlag4000000) != 0) {
            fixU = settings.mUVFixup[0];
            fixV = settings.mUVFixup[1];
            lastU = settings.mUVFixup[2];
            lastV = settings.mUVFixup[3];
        } else {
            fixU = settings.mFloat18;
            fixV = fixU;
            lastU = 0.0f;
            lastV = lastU;
        }
        entry->mUOffset = scale * ((lastU + (static_cast<float>(x) + fixU)) / static_cast<float>(texture->mWidth));
        entry->mVOffset = scale * ((lastV + (static_cast<float>(y) + fixV)) / static_cast<float>(texture->mHeight));
        entry->mUSize =
            scale * (ppc_fnmsubs(2.0f, fixU, static_cast<float>(width)) / static_cast<float>(texture->mWidth));
        entry->mVSize =
            scale * (ppc_fnmsubs(2.0f, fixV, static_cast<float>(height)) / static_cast<float>(texture->mHeight));
    }
    // GetTexture is an empty function on the Mac (the OpenGL page's only), so its answer is
    // the object it was called on: the whole COpenGLTexture, which SetTexture takes back as
    // the page. (The field's type is the page's CTextureBase; the pointer is the complete
    // object's, as on the Mac.)
    texture->GetTexture();
    entry->mTexture = reinterpret_cast<CTextureBase*>(dynamic_cast<void*>(texture));
    return 1;
}

int32_t CTextureLink::PageUpdated() {
    ++gTexturePageUpdates;
    mF14 = gei_Display.mFrameCount;
    return mTexture->UnlockSurface();
}

// What a page holds, as the sorts and the packing want to know it (they call it on every page
// they order). mF08 gets the pixels left free, mF0C the pixels of the entries asked for this
// frame, mF1C the pixels of the locked entries and mF10 a score of what freeing the page would
// cost: eight times the pixels in use, four times the pixels asked for, twice the entries, the
// entries' pixels weighted by their recent use, sixteen for each compressed image, and the page's
// pixels for each of the sixteen frames since it was last updated. The texture's flags get 1
// when an entry is locked, 2 when one is stored as wrapping and 4 when the page is not empty and
// no entry on it can be cut down any more. The argument (the cache's flags) is not read.
void CTextureLink::CalcUsage(int32_t) {
    uint32_t entryCount = 0;
    uint32_t entryPixels = 0;     // the cache sizes' areas added up
    uint32_t weighted = 0;        // each area times its entry's recent use (the place record's sum)
    uint32_t compressed = 0;      // entries flagged as a compressed image
    bool anyLocked = false;
    bool anyWrapping = false;
    bool atMaxCompression = mEntries != nullptr;
    const int32_t pixelCount = mTexture->GetPixelCount();
    mF08 = pixelCount;
    mF0C = 0;
    mF1C = 0;
    for (CTextureEntry* entry = mEntries; entry != nullptr; entry = entry->mNext) {
        if ((entry->mFlags & CTextureEntry::kLocked) != 0) anyLocked = true;
        if (HasCutLeft(entry)) atMaxCompression = false;
        if (entry->StoreAsWrapping() != 0) anyWrapping = true;
        const uint32_t area = static_cast<uint32_t>(static_cast<int32_t>(entry->mCacheWidth) * entry->mCacheHeight);
        if ((entry->mFlags & kEntryAskedFor) != 0) mF0C = static_cast<int32_t>(static_cast<uint32_t>(mF0C) + area);
        if ((entry->mFlags & kEntryCompressedImage) != 0) ++compressed;
        entryPixels += area;
        weighted += area * static_cast<uint32_t>(entry->mPlace->mF10);
        if ((entry->mFlags & CTextureEntry::kLocked) != 0) mF1C = static_cast<int32_t>(static_cast<uint32_t>(mF1C) + area);
        ++entryCount;
    }
    int32_t textureFlags = anyLocked ? 1 : 0;
    if (anyWrapping) textureFlags |= 2;
    if (atMaxCompression) textureFlags |= 4;
    mTexture->SetFlags(textureFlags);
    mF08 = static_cast<int32_t>(static_cast<uint32_t>(mF08) - entryPixels);
    // The page's age in frames: 16 for a page updated this frame, down to none after 16 frames.
    const int32_t age = static_cast<int32_t>(static_cast<uint32_t>(gei_Display.mFrameCount) - static_cast<uint32_t>(mF14));
    const uint32_t recency = age < 16 ? static_cast<uint32_t>(16 - age) : 0u;
    mF10 = static_cast<int32_t>(entryPixels * 8u + static_cast<uint32_t>(mF0C) * 4u + entryCount * 2u + weighted +
                                compressed * 16u + recency * static_cast<uint32_t>(pixelCount));
}
