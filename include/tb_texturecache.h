// tb_texturecache.h: texture pages and the cache that fills them (engine library; the
// inventory files the range, 0x10032b58..0x10034b48 and 0x100385a8, under Wavelet.cpp; the
// original files were tb_texturecache_*.cpp).
//
// A page is one square hardware texture (CTextureBase, made by the renderer) with the
// list of texture entries placed on it (CTextureLink). The cache keeps up to 256 pages of
// each of six sizes, 256 down to 8 pixels.
//
// PARTIAL: start-up, shutdown, Flush and Remove are translated (src/tb_texturecache.cpp);
// the frame's calls Prepare, UploadToVRAM, GenerateTexturePages, PageUpdated and ReInitialize
// are in src/tb_texturecache_frame2.cpp; the sorting and the entry lists are in
// src/tb_texturecache_sort.cpp (with CQSort, include/tb_qsort.h); the packing between them
// is still to do.
// are in src/tb_texturecache_frame2.cpp, the packing (PackTextures, ArrangeNonwrappingTextures)
// in src/tb_texturecache_pack.cpp; the sorting between them is still to do.
// Mac offsets in the comments; the pointers make the host objects larger.
#pragma once
#include <cstdint>

class CTextureEntry;

// The renderer's side of a page. 0x18 bytes on the Mac, the vtable at +0x10. The slots are
// in the original's order.
class CTextureBase {
public:
    enum : uint32_t {
        kPositionLocked = 1,
        kWrapping = 2,
        kMaxCompression = 4,
        kKeptBySetFlags = 0x10000000,
    };

    CTextureBase() : mFlags(0) {}                        // 0x10032b58
    uint32_t GetFlags() { return mFlags; }               // 0x10032b98
    // Replaces every flag but kKeptBySetFlags.
    void SetFlags(int32_t flags);                        // 0x10032bd0
    uint32_t IsPositionLocked() { return mFlags & kPositionLocked; }          // 0x10032c14
    uint32_t IsWrapping() { return mFlags & kWrapping; }                      // 0x10032c58
    uint32_t IsUsingMaxCompression() { return mFlags & kMaxCompression; }     // 0x10032c94
    void PositionLock() { mFlags |= kPositionLocked; }                        // 0x10032cdc
    void PositionUnlock() { mFlags &= ~static_cast<uint32_t>(kPositionLocked); }   // 0x10032d20

    virtual int32_t Create(int32_t width, int32_t height, int32_t format, int32_t mipmaps) = 0;
    virtual int32_t Destroy() = 0;
    virtual void GetTexture() = 0;
    virtual int32_t GetPixelCount() = 0;
    virtual int32_t LockSurface() = 0;
    virtual int32_t UnlockSurface() = 0;
    virtual int32_t MipmapsUsed() = 0;
    // Copies an entry's image into the page's upload buffer at (x, y).
    virtual int32_t Download(CTextureEntry* entry, int32_t x, int32_t y) = 0;
    virtual double TestUploadSpeed() = 0;
    virtual ~CTextureBase();                             // 0x10032d64

    uint32_t mFlags;       // 0x00
    int32_t mWidth;        // 0x04
    int32_t mHeight;       // 0x08
    int32_t mPitch;        // 0x0c  bytes per row of the upload buffer
};

// One page of the cache: 0x20 bytes on the Mac.
class CTextureLink {
public:
    CTextureLink() : mTexture(nullptr), mEntries(nullptr), mF08(0), mF0C(0), mF14(-1) {}   // 0x100385a8
    // The page's image changed: counts it, notes the frame in mF14 and sends the upload buffer
    // to the page. Answers what the texture's UnlockSurface answers (src/tb_texturecache_frame2.cpp).
    int32_t PageUpdated();                           // 0x100385f4
    // Works out what is on the page (src/tb_texturecache_balance.cpp; SortTextures calls it with the
    // cache's flags, which it does not read):
    // mF08 the pixels left free, mF0C the pixels of the entries asked for this frame, mF10 a
    // weighted score of what the page holds (the cost of freeing it), mF1C the pixels of locked
    // entries; and sets the texture's flags (1 when an entry is locked, 2 when one is stored as
    // wrapping).
    void CalcUsage(int32_t cacheFlags);              // 0x10038670

    CTextureBase* mTexture;      // 0x00
    CTextureEntry* mEntries;     // 0x04  the entries on the page, chained through their mNext
    int32_t mF08;                // 0x08  0
    int32_t mF0C;                // 0x0c  0
    int32_t mF10;                // 0x10  not set by the constructor
    int32_t mF14;                // 0x14  -1
    int32_t mF18;                // 0x18  not set by the constructor
    int32_t mF1C;                // 0x1c
};

// Where the packing puts one entry on a page: the corner, the size it takes and the entry.
// 0x14 bytes on the Mac (PackTextures and ArrangeNonwrappingTextures fill an array of them,
// ending with a null entry); the pointer makes the host copy larger.
struct SAvailableQuad {
    int32_t mX;                  // 0x00
    int32_t mY;                  // 0x04
    int32_t mWidth;              // 0x08
    int32_t mHeight;             // 0x0c
    CTextureEntry* mEntry;       // 0x10
};

#include "tb_qsort.h"   // CQSort and gQSort, the engine's quick sorter the packing uses

class CTextureCache {
public:
    enum : int32_t { kSizes = 6, kPagesPerSize = 256 };
    enum : uint32_t {
        kReshuffle = 0x00000001,        // CACHEFLAG_RESHUFFLE: Refresh sets it when the view or the lights changed
        kFlag2 = 0x00000002,            // Init: the texture settings ask for it (settings flag 2); the packing rounds
                                        //   each slot up with mSizeMask and centres the image in it
        kReady = 0x20000000,            // set by Init, cleared by Close
        // Refresh: the frame redraws the static pool (the view or the lights changed), so every
        // entry and page is unlocked for it to be placed again.
        kUnlocking = 0x00000001,
    };

    CTextureCache();                    // 0x100338d4
    // Unit v5092: times an upload of `count` textures of `size`-pixel width (arguments as SMachineParameters::Suggest passes
    // 0x100 and 0x10); answers the speed. Not translated yet.
    double TestUploadSpeed(int32_t size, int32_t count);   // 0x100339c8
    // Works out how many pages of each size to make from what the card can hold.
    int32_t AutoDetectPageCreation();   // 0x10033b68
    // Makes them; a size that gets fewer pages than wanted passes the rest on to the next
    // smaller size. 0 when a size that wanted pages got none (everything is destroyed).
    int32_t AutoCreateSurfaces();       // 0x10033ecc
    // Up to `count` pages of `size` pixels; `count` comes back as the number made.
    int32_t CreateSurfaces(int32_t size, int32_t& count, int32_t format, int32_t mipmaps);   // 0x1003407c
    int32_t Init(int32_t format);       // 0x1003439c
    int32_t Close();                    // 0x10034514
    int32_t Remove(CTextureEntry* entry);   // 0x10034610
    // Takes entries off their pages: all of them, or with bit 1 of `mode` only the ones
    // flagged 0x200000; bit 0 also clears each entry's place record.
    void Flush(int32_t mode);           // 0x100347f4
    void SetUITextureSize();            // 0x10034b48
    // Asks for the entry to be on a page this frame: an entry that is on none is queued for
    // GenerateTexturePages (mF18AC) with `distance` as its priority, one that is on a page or
    // queued already keeps the smaller distance. An entry flagged kNextIsPlane takes the next
    // entry of its array along. (src/tb_texturecache_frame2.cpp)
    void Prepare(CTextureEntry* entry, float distance);   // 0x10032ddc
    // Texture settings flag 0x80 writes the cache's requests to the file "tcache.log": Prepare
    // appends one record per entry (the first call also writes the settings), Execute a marker.
    void DumpTCStreamToLog_Prepare(CTextureEntry* entry);   // 0x10034bc4
    void DumpTCStreamToLog_Execute();   // 0x10034d50
    // Packs the queue (mF18AC) onto pages through PackTextures; entries that fit nowhere
    // go back to the state Remove leaves them in. Answers the last answer of PackTextures (1
    // when it never ran or the last was -1); Execute tests it. (The decompiler calls it void.)
    int32_t GenerateTexturePages();     // 0x10037494
    // Takes the entry's place on `link`'s page (quad, or the quad's own entry when `entry` is
    // null) and works out its texture coordinates. Always answers 1.
    int32_t UploadToVRAM(CTextureLink* link, SAvailableQuad* quad, CTextureEntry* entry);   // 0x1003820c
    // Puts the entries at the front of the queue `*list` on one page (the first page of the first
    // size from the entry's own that still has one to fill) and takes them off the queue: -1 when
    // the first entry fits nowhere, 0 on a failure (GenerateTexturePages stops), else what
    // PageUpdated answers. (src/tb_texturecache_pack.cpp)
    int32_t PackTextures(CTextureEntry** list);   // 0x1003678c
    // The sorting (src/tb_texturecache_sort.cpp). SortTextures puts the pages of each size in
    // order, SortEntries the queue (mF18AC), the others work on a list of entries chained
    // through their mNext (a list head, or an array).
    // Per size: the pages that are not position locked first, in increasing mF0C (the pixels
    // asked for this frame) unless cache flag 1 asks for all of them to be ordered; then the pages
    // up to the last one that is at most half full in increasing mF10, and mF1818 gets the index
    // of that last one. CalcUsage runs on every page it orders, twice; the sums are in mF18B8, mF18BC
    // and mF18C0.
    void SortTextures();                // 0x1003335c
    // Orders pages by mF0C, or by mF10, ascending; links[low..high], both ends included.
    void Textures_QSort(CTextureLink** links, int32_t low, int32_t high);       // 0x10033098
    void Textures_QSortFree(CTextureLink** links, int32_t low, int32_t high);   // 0x100331f8
    // Orders the queue by its priority values, largest first, and works out the sum of the
    // entries' areas. When the pages cannot hold them (AutoMipmap and Entry_Compress shrink
    // them first) entries are flagged 0x8000 from the END of that order until the cache
    // flag 4 limit mF18E4 is met; then the queue is chained again in increasing sort key
    // (so the largest key is first), leaving the flagged entries out and resetting them.
    void SortEntries();                 // 0x10035300
    // Sorts entries[low..high] by sort key, ascending (does nothing under two entries).
    void Entry_QSortInit(CTextureEntry** entries, int32_t low, int32_t high);   // 0x10034e08
    // Chains the list again in decreasing sort key (the first 4096 entries; the others lose
    // their page bits).
    void SortByCriteria(CTextureEntry** list);   // 0x100358ec
    // Chains the list again in increasing address (the first 4096 entries).
    void SortByAddress(CTextureEntry** list);    // 0x10035ab0
    // Takes every entry of the chain `entries` that was asked for this frame (flag
    // 0x40000000) into `list`, which stays in decreasing sort key; the others are reset to
    // the state of an entry on no page.
    void MergeListsConditional(CTextureEntry** list, CTextureEntry* entries);   // 0x10035be0
    // Merges a page's entries with the chain `entries` (both in decreasing height, then width)
    // into the array `out`, ending it with null. Skips what is stored as wrapping and what
    // on the page was not asked for. Answers the sum of the cache sizes' areas.
    int32_t CreateNonWrappingTextureList(CTextureEntry** out, CTextureEntry* entries, CTextureLink* page);   // 0x10035e6c
    // Called by SortEntries (src/tb_texturecache_balance.cpp): shrinks entries by the mipmap steps
    // the cache allows (AutoMipmap: levels 0 to mF18E0 - 1, each with no limit), and the compression
    // that follows when the pages cannot hold them all (Entry_Compress: from level mF18E0 up to 11,
    // while the entries' area mF18C4 is over the pixels left, mF18C0 - mF18BC, or over the limit
    // mF18E4 with cache flag 4). Both call Entry_Specific_Compress.
    void AutoMipmap(CTextureEntry** entries, int32_t count);        // 0x10035244
    void Entry_Compress(CTextureEntry** entries, int32_t count);    // 0x1003514c
    // Cuts entries[0..count) down one level `level` at a time until their area fits `available`
    // (untranslated; AutoMipmap passes INT32_MIN, which is no limit). Changes mF18C4.
    void Entry_Specific_Compress(CTextureEntry** entries, int32_t count, int32_t level, int32_t available);   // 0x10034e94
    // Empties a page of its entries (untranslated; the balancing calls it on the pages it frees).
    void BalanceCache_ClearTexture(CTextureLink* link);   // 0x1003770c
    // The frame's members and the alt-tab recovery's; Execute and ReInitialize return 0 on
    // failure. Refresh is void in the original (the caller ignores the result): it answers 1.
    // Called once per window before anything is prepared: ages every entry on a page (src/
    // tb_texturecache_frame.cpp).
    // Lays the entries of `list` (an array of pointers ending with null, largest first) out on the
    // page `page` with a guillotine packer: `quads` gets one SAvailableQuad per entry placed, ending
    // with a null entry (room for 128), `count` the number placed. Answers whether the whole list
    // was placed. A first entry that fits nowhere is cut down a size at a time (when mFlags has
    // kReshuffle). The last argument is not used. (src/tb_texturecache_pack.cpp)
    bool ArrangeNonwrappingTextures(CTextureEntry** list, SAvailableQuad* quads, int32_t* count, CTextureBase* page,
                                    int32_t unused);   // 0x10036100
    int32_t Refresh();                  // 0x10034204
    // Places the entries asked for since Refresh on pages; pushes error 0xb0754011 and answers 0
    // when no page could be generated (src/tb_texturecache_frame.cpp).
    int32_t Execute();                  // 0x10033688
    // Closes the cache when it is up, sets the texture format (-1: keeps it) and makes the pages
    // again; answers what AutoCreateSurfaces answers (src/tb_texturecache_frame2.cpp).
    int32_t ReInitialize(int32_t format);   // 0x1003447c
    // Execute's balancing steps (src/tb_texturecache_balance.cpp). Execute calls FreeLockedTextureSpace
    // only when the view or the lights changed: size by size it clears up to mF1860 (at most 16)
    // pages that hold some locked entries (less than a quarter of the page) and little that the
    // frame asked for (under half), the ones asked for least first. LevelSurfaceUsage compares the
    // sizes by the share of their pages that are at most half full (mF1818 over the page count).
    // For a size under 0.67 whose share is less than two thirds of the previous (larger) size's, it
    // clears some of the size's fuller pages so their entries can move up a size; when the next
    // (smaller) size has the larger share, it cuts the entries of some pages down a level and
    // clears the pages so they move down. When it cleared any page it sorts the pages and the
    // queue again. Only for renderers 1 and 3.
    void BalanceCache_FreeLockedTextureSpace();   // 0x1003780c
    void BalanceCache_LevelSurfaceUsage();   // 0x100379e0 (the original leaves 1 in r3; nothing reads it)

    CTextureLink* mPages[kSizes][kPagesPerSize];   // 0x0000
    int32_t mPageCount[kSizes];         // 0x1800
    union {                             // 0x1818  one Mac array, two readings of it:
        int32_t mF1818[kSizes];         //         SortTextures writes per size the index of the last page
                                        //         that is at most half full (0 when none is), and
        int32_t mUsable[kSizes];        //         the packing reads it as how many pages of each size it
                                        //         may fill this frame; mF1830 counts the ones used
    };
    int32_t mF1830[kSizes];             // 0x1830  0
    uint8_t mPad1848[0x1860 - 0x1848];
    int32_t mF1860;                     // 0x1860  4
    float mShare[kSizes];               // 0x1864  0.5, 0.25, 0.17, 0.065, 0.012, 0.003
    int32_t mWanted[kSizes];            // 0x187c  pages of each size (AutoDetectPageCreation)
    int32_t mF1894[kSizes];             // 0x1894  0x100, then 0x80 five times
    CTextureEntry* mF18AC;              // 0x18ac  null; the entries asked for since Refresh (through
                                        //         their mNext); Execute works only when there are some
    int32_t mF18B0;                     // 0x18b0  0
    uint32_t mFlags;                    // 0x18b4
    int32_t mF18B8;                     // 0x18b8  0
    int32_t mF18BC;                     // 0x18bc  0
    int32_t mF18C0;                     // 0x18c0  SortTextures: the sum of the pages' pixel counts
    int32_t mF18C4;                     // 0x18c4  SortEntries: the sum of the entries' width * height
    uint8_t mPad18C8[4];
    int32_t mSizeMask;                  // 0x18cc  7; Flush rounds an entry's size up with it
    int32_t mMipmaps;                   // 0x18d0  0, or 4 with kFlag2: CreateSurfaces' last argument
    int32_t mLargestSize;               // 0x18d4  the largest page size that has pages
    int32_t mUITextureSize;             // 0x18d8
    int32_t mF18DC;                     // 0x18dc  EngineStartup: 3 - TEXTUREQUALITY (0..3)
    int32_t mF18E0;                     // 0x18e0
    int32_t mF18E4;                     // 0x18e4  the most pixels the entries may take (cache flag 4)
    float mF18E8;                       // 0x18e8  1.0
    // 0x18ec..0x18fc  SortEntries: priority limits, the lowest (-0.001), then 1/8, 1/4 and 1/2 of
    // the way up to the highest (0.001 over the largest priority queued).
    float mF18EC;
    float mF18F0;
    float mF18F4;
    float mF18F8;
    float mF18FC;
    uint8_t mPad1900[0x1908 - 0x1900];
    uint8_t mPad18EC[0x1900 - 0x18ec];
    int32_t mWidest;                    // 0x1900  the widest cache width ArrangeNonwrappingTextures went past
    int32_t mTallest;                   // 0x1904  the same for the height
    int32_t mF1908;                     // 0x1908  the same value as mF18DC, when the game sets one
    int32_t mReordered;                 // 0x190c  ArrangeNonwrappingTextures cut an entry down and moved it in its list
};
// 0x1910 bytes on the Mac.
extern CTextureCache* gei_TCache;   // engine export
// How many times a page's image was sent to the card: CTextureLink::PageUpdated counts them.
// Engine BSS 0x10173194 (not exported; name invented); defined in src/tb_texturecache_frame2.cpp.
extern int32_t gTexturePageUpdates;
// Counters of the packing, none read anywhere else: gTextureQuadsPending is zeroed by every
// ArrangeNonwrappingTextures and counts its quads, PackTextures counts each placed entry off again
// (and puts the value back after a trial that failed); gTextureQuadsPlaced counts every quad ever
// made; gTextureSizeSteps counts the packing's moves to the next smaller page size; gTextureCutDowns
// counts the cuts of a whole-page entry. Engine BSS 0x1017318c, 0x10173190, 0x10173198 and 0x10173188
// (not exported; names invented); defined in src/tb_texturecache_pack.cpp.
extern int32_t gTextureQuadsPending;
extern int32_t gTextureQuadsPlaced;
extern int32_t gTextureSizeSteps;
extern int32_t gTextureCutDowns;

// The value Flush gives an entry to sort by is built with this table... (see the .cpp).
void InitFrequencyTable();          // 0x1000967c
extern int32_t gFrequencyTable[16]; // engine data 0x10052d78 (not exported; name invented)
// Every frame CTextureCache::Refresh calls this first: it writes the display's frame count & 15
// to gFrequencyIndex and refills gFrequencyTable starting there (src/tb_texturecache_balance.cpp).
void CycleFrequencyTable();         // 0x10009770
// Which byte of an entry's place record belongs to this frame (0..15). Engine data 0x10052d74
// (not exported; name invented); defined in src/tb_texturecache_frame.cpp.
extern uint32_t gFrequencyIndex;

int32_t TCache_Init(int32_t format);   // 0x1003377c
int32_t TCache_Close();                // 0x10033854
