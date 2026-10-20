#include "tpw_probe.h"
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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_printf_opengl.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

int32_t gTextureQuadsPending = 0;
int32_t gTextureQuadsPlaced = 0;
int32_t gTextureSizeSteps = 0;
int32_t gTextureCutDowns = 0;

namespace {

// The entry's flags this file reads; the named ones are in tb_textureentry.h.
constexpr uint32_t kEntryStoreLevel = 0x00000030;   // TEFLAG_ENTRY_MIPMAP_STORE_FILTER: how many cuts are left (0..3)
constexpr uint32_t kEntryStoreStep = 0x00000010;    // one cut
constexpr uint32_t kEntrySortBlend = 0x00080000;    // the sort key gets 0x40000000

// The smallest page size: the last word of the page size table (engine data 0x10051f40..0x10051f54,
// 256 down to 8). A wrapping entry is cut down only while it is wider than that.
constexpr int32_t kSmallestPage = 8;

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

// An entry's sort key: the priority (nearer is larger) in bits 16..27, the cache size
// (width * 256 + height) below it, and a bit each for a locked entry and for the 0x80000 kind.
// The same as CTextureCache::Remove's and Prepare's (src/tb_texturecache_frame2.cpp).
void RebuildSortKey(CTextureEntry* entry) {
    entry->mSortKey = static_cast<int32_t>(
        (static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - entry->mPriority))) << 16) & 0x0fff0000u);
    entry->mSortKey = entry->mCacheWidth * 0x100 + (entry->mCacheHeight + entry->mSortKey);
    if ((entry->mFlags & CTextureEntry::kLocked) != 0) entry->mSortKey |= 0x20000000;
    if ((entry->mFlags & kEntrySortBlend) != 0) entry->mSortKey |= 0x40000000;
}

// The test the packing makes of an entry, inline in tb_textureentry.h: is there a cut left? A
// wrapping entry also stops at the smallest page size.
bool CanBeCutDown(CTextureEntry* entry) {
    if (entry->StoreAsWrapping() != 0) return (entry->mFlags & kEntryStoreLevel) != 0 && entry->mCacheWidth > kSmallestPage;
    return (entry->mFlags & kEntryStoreLevel) != 0;
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
    RebuildSortKey(entry);
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
