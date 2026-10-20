// The texture cache's last two helpers (engine library of SimTheme Park 2.04b3; the inventory
// files the cache under Wavelet.cpp):
//
//   0x10034e94 CTextureCache::Entry_Specific_Compress   (616 bytes)
//   0x1003770c CTextureCache::BalanceCache_ClearTexture (176)
//
// Entry_Specific_Compress is what AutoMipmap and Entry_Compress call (src/tb_texturecache_balance.cpp);
// BalanceCache_ClearTexture is what the two balancing steps call on the pages they free.
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// Entry flags this file reads; the named ones are in tb_textureentry.h.
constexpr uint32_t kEntryDetail = 0x000000c0;       // how large the entry's detail class is (0, 0x40, 0x80, 0xc0)
constexpr uint32_t kEntryStoreLevel = 0x00000030;   // TEFLAG_ENTRY_MIPMAP_STORE_FILTER: how many cuts are left (0..3)
constexpr uint32_t kEntryStoreStep = 0x00000010;    // one cut
constexpr uint32_t kEntrySortBlend = 0x00080000;    // the sort key gets 0x40000000
constexpr uint32_t kEntryAskedFor = 0x40000000;     // asked for this frame (Prepare sets it, Refresh clears it)
constexpr uint32_t kEntryQueued = 0x00002000;       // on the queue to get a page

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
void RebuildSortKey(CTextureEntry* entry) {
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
        RebuildSortKey(entry);

        const int32_t newArea = entries[i]->mCacheWidth * entries[i]->mCacheHeight;
        mF18C4 = mF18C4 - (oldArea - newArea);
        if (mF18C4 < available) i = count;
    }
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
