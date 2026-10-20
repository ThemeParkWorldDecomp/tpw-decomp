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
#include <cstring>

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// The side of a page of each size (engine data 0x10051f40: 256 down to 8; a second copy of the
// table lies at 0x10051f58). The last one is also the smallest width a wrapping entry is cut to.
constexpr int32_t kPageSides[CTextureCache::kSizes] = {256, 128, 64, 32, 16, 8};
constexpr int32_t kSmallestPage = 8;

// Entry flags this file reads: the store filter (how many cuts are left, 0..3), "asked for this
// frame", the compressed image and the 0x80000 kind (the sort key gets 0x40000000).
constexpr uint32_t kEntryStoreLevel = 0x00000030;
constexpr uint32_t kEntryStoreStep = 0x00000010;
constexpr uint32_t kEntryAskedFor = 0x40000000;
constexpr uint32_t kEntryCompressedImage = 0x10000000;
constexpr uint32_t kEntrySortBlend = 0x00080000;

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

// Every frame CTextureCache::Refresh calls this: the index of this frame's use count in each
// entry's place record is the display's frame count modulo 16, and the table of weights
// (16 for this frame, 15 for the one that was current a frame ago ... 1) turns round with it.
void CycleFrequencyTable() {
    gFrequencyIndex = static_cast<uint32_t>(gei_Display.mFrameCount) & 0xf;
    for (int32_t age = 0; age < 16; ++age) gFrequencyTable[(gFrequencyIndex + static_cast<uint32_t>(age)) & 0xf] = 16 - age;
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

// Cuts the entries down one mipmap level at a time, level by level: levels 0 to mF18E0 - 1, each
// asked to fit no particular area (INT32_MIN).
void CTextureCache::AutoMipmap(CTextureEntry** entries, int32_t count) {
    for (int32_t level = 0; level < mF18E0; ++level) Entry_Specific_Compress(entries, count, level, INT32_MIN);
}

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
                    RebuildSortKey(entry);
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
