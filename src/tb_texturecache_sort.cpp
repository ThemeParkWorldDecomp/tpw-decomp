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
#include <cstddef>
#include <cstdint>

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_qsort.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// Longest list the entry sorts take (the Mac keeps the array on the stack).
constexpr int32_t kSortLimit = 0x1000;

// Cache flags (CTextureCache::mFlags).
constexpr uint32_t kCacheReshuffle = 0x00000001;   // CACHEFLAG_RESHUFFLE: locked pages and entries are sorted too
constexpr uint32_t kCacheLimited = 0x00000004;     // SortEntries: the entries' area is held to mF18E4

// Entry flags (CTextureEntry::mFlags).
constexpr uint32_t kEntryWanted = 0x00000030;      // the pair 0xc0 is copied down into it when an entry is reset
constexpr uint32_t kEntryQueued = 0x00002000;
constexpr uint32_t kEntryLocked = 0x00004000;      // the sort key gets 0x20000000
constexpr uint32_t kEntryDropped = 0x00008000;     // SortEntries: left out of the queue
constexpr uint32_t kEntryBlendKind = 0x00080000;   // the sort key gets 0x40000000
constexpr uint32_t kEntryAsked = 0x40000000;       // Prepare asked for it this frame
constexpr uint32_t kEntryPageBits = 0xffff0fcfu;   // clears 0x1000..0x8000 (page, queued, locked, dropped) and 0x30

// An entry's sort key: the priority (nearer is larger) in bits 16..27, the cache size
// (width * 256 + height) below it, and a bit each for a locked entry and for the 0x80000
// kind. Prepare, Remove and GenerateTexturePages build the same.
void RebuildSortKey(CTextureEntry* entry) {
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
    RebuildSortKey(entry);
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

// ---- The queue ---------------------------------------------------------------------------

void CTextureCache::Entry_QSortInit(CTextureEntry** entries, int32_t low, int32_t high) {
    if (low < high) {
        void* scratch;
        gQSort.pulAsc(entries, &scratch, static_cast<int32_t>(sizeof(CTextureEntry)),
                      static_cast<int32_t>(offsetof(CTextureEntry, mSortKey)), low, high);
    }
}

void CTextureCache::SortEntries() {
    CTextureEntry* entries[kSortLimit];
    void* scratch;
    int32_t count = 0;
    float highest = 0.0f;
    CTextureEntry* entry = mF18AC;
    while (entry != nullptr && count < kSortLimit) {
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
    CTextureEntry* entries[kSortLimit];
    int32_t count = 0;
    CTextureEntry* entry = *list;
    while (entry != nullptr && count < kSortLimit) {
        entries[count++] = entry;
        entry = entry->mNext;
    }
    // What does not fit the array loses its page bits and is not in the list any more.
    if (count == kSortLimit) {
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
    CTextureEntry* entries[kSortLimit];
    CTextureEntry* scratch;
    int32_t count = 0;
    CTextureEntry* entry = *list;
    while (entry != nullptr && count < kSortLimit) {
        entries[count++] = entry;
        entry = entry->mNext;
    }
    if (count == kSortLimit) {
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
