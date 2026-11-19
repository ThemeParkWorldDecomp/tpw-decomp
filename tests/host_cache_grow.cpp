// host_cache_grow.cpp: the texture cache's pages when a frame asks for more than they hold. CTextureCache::GenerateTexturePages
// packs the frame's queue onto the pages SortTextures left to fill (mUsable of each size); an entry PackTextures finds no page
// for (-1) is dropped, drawn without its texture this frame and asked for again the next. The cache has the pages
// AutoDetectPageCreation made at the start, 248 at the 64 MB the port reports, and at the far views of the launcher's Free
// camera, Zoom out further and long Draw distances the ground, the build menu and the Park Information panel ask for more:
// what did not fit flickered white. With the port's fixes (cachegrow, src/engine/TextureCache.cpp), when one of those
// settings is on and no page of the frame of a size that holds the entry is left, PortCacheGrow adds an empty page and the
// entry is packed there:
//   - all three settings off: the same entries dropped as the original, no page made (both builds);
//   - each setting on: no entry dropped, as many pages added as the queue needs, each put where the frame's pages to fill
//     end (the page there goes last);
//   - another entry grows the smallest size its cache width and height fit, else the next larger one with fewer than
//     kPagesPerSize, also when smaller pages it does not fit are left (the far views' case: only pages of 8 were left);
//   - a wrapping entry grows its own best size, never a larger one, also when smaller pages too narrow for it are left; when
//     that size has kPagesPerSize pages, the next of them past those to fill is opened instead, and once all are open it is
//     dropped as before;
//   - every size that holds the entry at kPagesPerSize, a page that cannot be made, or an entry dropped while pages that
//     hold it are left: dropped as before;
//   - PackTextures failing (0): the frame stops as before, no page made.
// The program runs the real GenerateTexturePages and PortCacheGrow; PackTextures, CreateSurfaces and the entry's size
// calls are stand-ins. Made in both builds: without the port's fixes it checks only the settings off. Exits nonzero on a
// failure.
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <vector>

#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#ifdef TPW_TEST_PORT_FIX
#include "TPCamera.h"
#endif

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%ld)\n", what, value);
}

// The objects the unit reaches, as raw storage where the program links no constructor.
alignas(CTextureCache) unsigned char gCacheStorage[sizeof(CTextureCache)];
CTextureCache& Cache() { return *reinterpret_cast<CTextureCache*>(gCacheStorage); }
constexpr int32_t kEntries = 4000;
alignas(CTextureEntry) unsigned char gEntryStorage[kEntries * sizeof(CTextureEntry)];
CTextureEntry* Entry(int32_t i) { return reinterpret_cast<CTextureEntry*>(gEntryStorage) + i; }

// The stand-in pages: CreateSurfaces makes them, the test's pages at the start are made the same way.
std::vector<CTextureLink*> gLinks;
// How many entries one page takes in the stand-in packing, and how many each page has taken this frame.
int32_t gPerPage = 4;
std::map<CTextureLink*, int32_t> gFill;
// The entries the stand-in packing placed, and the page each went on.
std::map<CTextureEntry*, CTextureLink*> gPlaced;
// The entries that store as wrapping (their best size), and those PackTextures gives up on with pages left (too wide).
std::map<CTextureEntry*, int32_t> gWrapping;
std::set<CTextureEntry*> gTooWide;
// PackTextures answers 0 (an upload failed) at this call, or never at -1.
int32_t gPackFailAt = -1;
int32_t gPackCalls = 0;
// CreateSurfaces: whether it makes a page, and the calls (size, count, format, mipmaps).
bool gCreateWorks = true;
struct SCreate {
    int32_t mSize, mCount, mFormat, mMipmaps;
};
std::vector<SCreate> gCreates;

CTextureLink* NewLink() {
    CTextureLink* link = new CTextureLink;
    gLinks.push_back(link);
    return link;
}

}  // namespace

CTextureSettings gei_TextureSettings;
CTextureSettings::CTextureSettings() {}

int32_t CTextureEntry::StoreAsWrapping() { return gWrapping.count(this) != 0 ? 1 : 0; }
int32_t CTextureEntry::GetBestSize() {
    auto found = gWrapping.find(this);
    return found != gWrapping.end() ? found->second : 0;
}

// As the original: from the entry's start size, the first size with a page left to fill; -1 when none, when the entry is
// too wide, when a wrapping entry is wider than that page (the stand-in cuts nothing down) or when another entry's cache width
// or height is larger than it (the original arranges no quad on it).
// The entry goes on that page, gPerPage to a page, and a full page moves the size on to its next page.
int32_t CTextureCache::PackTextures(CTextureEntry** list) {
    if (gPackCalls++ == gPackFailAt) return 0;
    CTextureEntry* first = *list;
    int32_t size = first->StoreAsWrapping() != 0 ? first->GetBestSize() : 0;
    while (size < kSizes && mF1830[size] >= mUsable[size]) ++size;
    if (size >= kSizes) return -1;
    if (gTooWide.count(first) != 0) return -1;
    if (first->StoreAsWrapping() != 0 && first->mCacheWidth > (0x100 >> size)) return -1;
    if (first->StoreAsWrapping() == 0 && std::max(first->mCacheWidth, first->mCacheHeight) > (0x100 >> size)) return -1;
    CTextureLink* link = mPages[size][mF1830[size]];
    *list = first->mNext;
    gPlaced[first] = link;
    if (++gFill[link] >= gPerPage) ++mF1830[size];
    return 1;
}

// As the original: up to `count` pages of `size` pixels, while the size has fewer than kPagesPerSize.
int32_t CTextureCache::CreateSurfaces(int32_t size, int32_t& count, int32_t format, int32_t mipmaps) {
    gCreates.push_back({size, count, format, mipmaps});
    int32_t index = 0;
    while (index < kSizes && (0x100 >> index) != size) ++index;
    int32_t made = 0;
    while (made < count && gCreateWorks && index < kSizes && mPageCount[index] < kPagesPerSize) {
        mPages[index][mPageCount[index]++] = NewLink();
        ++made;
    }
    count = made;
    return count > 0;
}

namespace {

// A cache with `pages` pages of each size, the first `usable` of them to fill this frame, and a queue of `count` entries
// held at `wide` pixels square.
void Setup(const int32_t (&pages)[CTextureCache::kSizes], const int32_t (&usable)[CTextureCache::kSizes], int32_t count,
           int16_t wide = 256) {
    std::memset(gCacheStorage, 0, sizeof gCacheStorage);
    std::memset(gEntryStorage, 0, sizeof gEntryStorage);
    for (CTextureLink* link : gLinks) delete link;
    gLinks.clear();
    gFill.clear();
    gPlaced.clear();
    gWrapping.clear();
    gTooWide.clear();
    gCreates.clear();
    gPackFailAt = -1;
    gPackCalls = 0;
    gCreateWorks = true;
    gPerPage = 4;
    CTextureCache& cache = Cache();
    cache.mMipmaps = 4;
    for (int32_t size = 0; size < CTextureCache::kSizes; ++size) {
        for (int32_t k = 0; k < pages[size]; ++k) cache.mPages[size][k] = NewLink();
        cache.mPageCount[size] = pages[size];
        cache.mUsable[size] = usable[size];
    }
    for (int32_t i = 0; i < count; ++i) {
        Entry(i)->mWidth = 16;
        Entry(i)->mHeight = 16;
        Entry(i)->mCacheWidth = wide;
        Entry(i)->mCacheHeight = wide;
        Entry(i)->mNext = i + 1 < count ? Entry(i + 1) : nullptr;
    }
    cache.mF18AC = count > 0 ? Entry(0) : nullptr;
}

int32_t Dropped(int32_t count) {
    int32_t dropped = 0;
    for (int32_t i = 0; i < count; ++i) dropped += gPlaced.count(Entry(i)) == 0 ? 1 : 0;
    return dropped;
}

#ifdef TPW_TEST_PORT_FIX
void SetView(bool farZoom, bool freeCamera, int32_t drawDistance) {
    gPortFarZoom = farZoom;
    gPortFreeCamera = freeCamera;
    gPortDrawDistance = drawDistance;
}
#endif

// Twelve pages of 256 with ten to fill and 4 entries a page take 40 of 60 entries: 20 dropped, as the original.
void Original() {
#ifdef TPW_TEST_PORT_FIX
    SetView(false, false, kPortDrawOriginal);
    const int32_t added = gPortCachePagesAdded;
#endif
    Setup({12, 0, 0, 0, 0, 0}, {10, 0, 0, 0, 0, 0}, 60);
    const int32_t result = Cache().GenerateTexturePages();
    Check(result == 1, "original: the frame packs", result);
    Check(Dropped(60) == 20, "original: 20 entries dropped", Dropped(60));
    Check(gCreates.empty(), "original: no page made", static_cast<long>(gCreates.size()));
    Check(Cache().mPageCount[0] == 12 && Cache().mUsable[0] == 10, "original: the pages as they were");
    Check(Cache().mF18AC == nullptr, "original: the queue emptied");
    Check((Entry(59)->mCacheWidth & 7) == 0 && Entry(59)->mCacheWidth == 16, "original: a dropped entry forgotten");
#ifdef TPW_TEST_PORT_FIX
    Check(gPortCachePagesAdded == added, "original: none counted");
#endif
}

#ifdef TPW_TEST_PORT_FIX
// Each far view setting adds the five pages the 20 entries need, of 256, where the frame's pages to fill end.
void Grows() {
    struct SView {
        bool mFarZoom, mFreeCamera;
        int32_t mDraw;
        const char* mName;
    };
    const SView views[] = {{true, false, kPortDrawOriginal, "zoom out further"},
                           {false, true, kPortDrawOriginal, "free camera"},
                           {false, false, kPortDrawFarther, "draw distance farther"},
                           {false, false, kPortDrawWhole, "draw distance whole"}};
    for (const SView& view : views) {
        SetView(view.mFarZoom, view.mFreeCamera, view.mDraw);
        Setup({12, 0, 0, 0, 0, 0}, {10, 0, 0, 0, 0, 0}, 60);
        std::vector<CTextureLink*> before(Cache().mPages[0], Cache().mPages[0] + 12);
        gei_TextureSettings.mFormat = 5;
        const int32_t added = gPortCachePagesAdded;
        const int32_t result = Cache().GenerateTexturePages();
        Check(result == 1, view.mName, result);
        Check(Dropped(60) == 0, view.mName, Dropped(60));
        Check(Cache().mPageCount[0] == 17 && Cache().mUsable[0] == 15, view.mName, Cache().mPageCount[0]);
        Check(gPortCachePagesAdded - added == 5, view.mName, gPortCachePagesAdded - added);
        bool calls = gCreates.size() == 5;
        for (const SCreate& create : gCreates)
            calls = calls && create.mSize == 0x100 && create.mCount == 1 && create.mFormat == 5 && create.mMipmaps == 4;
        Check(calls, view.mName, static_cast<long>(gCreates.size()));
        // The first ten pages stay where they were, pages 10 to 14 are the new ones (each packed), and the two pages that
        // were past the pages to fill are still there.
        bool kept = true;
        for (int32_t k = 0; k < 10; ++k) kept = kept && Cache().mPages[0][k] == before[k];
        for (int32_t k = 10; k < 15; ++k)
            kept = kept && Cache().mPages[0][k] != before[10] && Cache().mPages[0][k] != before[11] && gFill[Cache().mPages[0][k]] == 4;
        std::set<CTextureLink*> tail(Cache().mPages[0] + 15, Cache().mPages[0] + 17);
        kept = kept && tail.count(before[10]) == 1 && tail.count(before[11]) == 1;
        Check(kept, view.mName);
    }
}

// The size grown is the smallest that holds the entry, else the next larger one with fewer than kPagesPerSize.
void Sizes() {
    SetView(false, true, kPortDrawOriginal);
    // Entries of 16 once the pages of 256 are filled: pages of 16, not of 256.
    Setup({10, 0, 0, 0, 0, 0}, {10, 0, 0, 0, 0, 0}, 48, 16);
    Check(Cache().GenerateTexturePages() == 1, "smallest: the frame packs");
    Check(Dropped(48) == 0, "smallest: none dropped", Dropped(48));
    Check(Cache().mPageCount[0] == 10 && Cache().mPageCount[4] == 2, "smallest: two pages of 16", Cache().mPageCount[4]);
    Check(gCreates.size() == 2 && gCreates[0].mSize == 0x10, "smallest: made at 16");

    // The far views' case: 256, 128 and 64 filled, pages of 8 left. Entries of 64 grow pages of 64, those of 8 stay empty.
    Setup({10, 10, 10, 0, 0, 20}, {10, 10, 10, 0, 0, 20}, 30 * 4 + 12, 64);
    Check(Cache().GenerateTexturePages() == 1, "small pages left: the frame packs");
    Check(Dropped(132) == 0, "small pages left: none dropped", Dropped(132));
    Check(Cache().mPageCount[2] == 13 && Cache().mPageCount[5] == 20 && Cache().mF1830[5] == 0,
          "small pages left: three pages of 64", Cache().mPageCount[2]);
    Check(gCreates.size() == 3 && gCreates[0].mSize == 0x40, "small pages left: made at 64");

    // Entries of 128 with 128 full at kPagesPerSize: the page added is a 256.
    Setup({10, 256, 0, 0, 0, 0}, {10, 256, 0, 0, 0, 0}, 40 + 256 * 4 + 8, 128);
    Check(Cache().GenerateTexturePages() == 1, "larger: the frame packs");
    Check(Dropped(40 + 256 * 4 + 8) == 0, "larger: none dropped", Dropped(40 + 256 * 4 + 8));
    Check(Cache().mPageCount[0] == 12 && Cache().mPageCount[1] == 256, "larger: two pages of 256", Cache().mPageCount[0]);
    Check(gCreates.size() == 2 && gCreates[0].mSize == 0x100, "larger: made at 256");

    // Entries of 256 with 256 full at kPagesPerSize and smaller pages left: dropped as before, nothing made.
    Setup({256, 0, 0, 0, 0, 4}, {1, 0, 0, 0, 0, 4}, 4 + 2);
    Check(Cache().GenerateTexturePages() == 1, "too large: the frame packs");
    Check(Dropped(6) == 2, "too large: two dropped", Dropped(6));
    Check(gCreates.empty(), "too large: no page made", static_cast<long>(gCreates.size()));

    // Every size full at kPagesPerSize: dropped as before, nothing made.
    Setup({256, 256, 256, 256, 256, 256}, {1, 1, 1, 1, 1, 1}, 6 * 4 + 3, 8);
    Check(Cache().GenerateTexturePages() == 1, "full: the frame packs");
    Check(Dropped(27) == 3, "full: three dropped", Dropped(27));
    Check(gCreates.empty(), "full: no page made", static_cast<long>(gCreates.size()));

    // A wrapping entry of best size 2 (64) grows a 64, though 256 has room.
    Setup({10, 0, 2, 0, 0, 0}, {10, 0, 2, 0, 0, 0}, 9, 64);
    for (int32_t i = 0; i < 9; ++i) gWrapping[Entry(i)] = 2;
    Check(Cache().GenerateTexturePages() == 1, "wrapping: the frame packs");
    Check(Dropped(9) == 0, "wrapping: none dropped", Dropped(9));
    Check(Cache().mPageCount[0] == 10 && Cache().mPageCount[2] == 3, "wrapping: a page of 64", Cache().mPageCount[2]);
    Check(gCreates.size() == 1 && gCreates[0].mSize == 0x40, "wrapping: made at 64");

    // The same with pages of 16 and 8 left, too narrow for it: still a 64, the small pages untouched.
    Setup({10, 0, 2, 0, 20, 20}, {10, 0, 2, 0, 20, 20}, 9, 64);
    for (int32_t i = 0; i < 9; ++i) gWrapping[Entry(i)] = 2;
    Check(Cache().GenerateTexturePages() == 1, "wrapping, narrow pages left: the frame packs");
    Check(Dropped(9) == 0, "wrapping, narrow pages left: none dropped", Dropped(9));
    Check(Cache().mPageCount[2] == 3 && Cache().mF1830[4] == 0 && Cache().mF1830[5] == 0,
          "wrapping, narrow pages left: a page of 64", Cache().mPageCount[2]);
    Check(gCreates.size() == 1 && gCreates[0].mSize == 0x40, "wrapping, narrow pages left: made at 64");
}

// A wrapping entry whose best size has kPagesPerSize pages opens the next of them past those to fill, nothing made, and once
// every page is open the rest are dropped as before.
void Opens() {
    SetView(false, true, kPortDrawOriginal);
    Setup({10, 0, 256, 0, 0, 20}, {10, 0, 2, 0, 0, 20}, 9, 64);
    for (int32_t i = 0; i < 9; ++i) gWrapping[Entry(i)] = 2;
    CTextureLink* next = Cache().mPages[2][2];
    const int32_t opened = gPortCachePagesOpened;
    Check(Cache().GenerateTexturePages() == 1, "opens: the frame packs");
    Check(Dropped(9) == 0, "opens: none dropped", Dropped(9));
    Check(gCreates.empty(), "opens: no page made", static_cast<long>(gCreates.size()));
    Check(Cache().mPageCount[2] == 256 && Cache().mUsable[2] == 3, "opens: one more page to fill", Cache().mUsable[2]);
    Check(gPlaced[Entry(8)] == next, "opens: the next page in order");
    Check(gPortCachePagesOpened - opened == 1, "opens: one counted", gPortCachePagesOpened - opened);

    // One page a wrapping entry, 255 of 256 to fill and 258 entries: one page opened, two dropped.
    Setup({0, 0, 256, 0, 0, 0}, {0, 0, 255, 0, 0, 0}, 258, 64);
    for (int32_t i = 0; i < 258; ++i) gWrapping[Entry(i)] = 2;
    gPerPage = 1;
    Check(Cache().GenerateTexturePages() == 1, "all open: the frame packs");
    Check(Dropped(258) == 2, "all open: two dropped", Dropped(258));
    Check(Cache().mUsable[2] == 256 && gCreates.empty(), "all open: every page to fill, none made", Cache().mUsable[2]);

    // Settings off: nothing opened.
    SetView(false, false, kPortDrawOriginal);
    Setup({10, 0, 256, 0, 0, 20}, {10, 0, 2, 0, 0, 20}, 9, 64);
    for (int32_t i = 0; i < 9; ++i) gWrapping[Entry(i)] = 2;
    Check(Cache().GenerateTexturePages() == 1, "opens, original: the frame packs");
    Check(Dropped(9) == 1 && Cache().mUsable[2] == 2, "opens, original: one dropped, none opened", Dropped(9));
}

// What the packing does not hand over to the growing.
void Limits() {
    SetView(false, true, kPortDrawOriginal);
    // No page made (the texture could not be created): dropped as before, and no loop.
    Setup({2, 0, 0, 0, 0, 0}, {2, 0, 0, 0, 0, 0}, 12);
    gCreateWorks = false;
    Check(Cache().GenerateTexturePages() == 1, "no texture: the frame packs");
    Check(Dropped(12) == 4, "no texture: four dropped", Dropped(12));
    Check(Cache().mPageCount[0] == 2 && Cache().mUsable[0] == 2, "no texture: the pages as they were");

    // An entry too wide while pages are left is dropped, nothing made.
    Setup({4, 0, 0, 0, 0, 0}, {4, 0, 0, 0, 0, 0}, 6);
    gTooWide.insert(Entry(2));
    Check(Cache().GenerateTexturePages() == 1, "too wide: the frame packs");
    Check(Dropped(6) == 1 && gPlaced.count(Entry(2)) == 0, "too wide: that one dropped", Dropped(6));
    Check(gCreates.empty(), "too wide: no page made", static_cast<long>(gCreates.size()));

    // The same for a wrapping entry while pages of its best size are left: dropped, nothing made or opened.
    Setup({0, 0, 4, 0, 0, 0}, {0, 0, 2, 0, 0, 0}, 6, 64);
    for (int32_t i = 0; i < 6; ++i) gWrapping[Entry(i)] = 2;
    gTooWide.insert(Entry(2));
    Check(Cache().GenerateTexturePages() == 1, "too wide, wrapping: the frame packs");
    Check(Dropped(6) == 1 && gPlaced.count(Entry(2)) == 0, "too wide, wrapping: that one dropped", Dropped(6));
    Check(gCreates.empty() && Cache().mUsable[2] == 2, "too wide, wrapping: none made or opened", static_cast<long>(gCreates.size()));

    // PackTextures failing stops the frame: the rest is forgotten, nothing made.
    Setup({1, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0}, 10);
    gPackFailAt = 6;
    Check(Cache().GenerateTexturePages() == 0, "failure: the frame stops");
    Check(gCreates.size() == 1, "failure: one page made before it", static_cast<long>(gCreates.size()));
    Check(Dropped(10) == 5 && Cache().mF18AC == nullptr, "failure: the rest forgotten", Dropped(10));

    SetView(false, false, kPortDrawOriginal);
}
#endif

}  // namespace

int main() {
    Original();
#ifdef TPW_TEST_PORT_FIX
    Grows();
    Sizes();
    Opens();
    Limits();
#else
    std::printf("without the port's fixes only the original's settings are checked: the cache never grows\n");
#endif
    std::printf("%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
