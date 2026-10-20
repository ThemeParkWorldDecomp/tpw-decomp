// Test entry point for the texture cache's packing (library unit_TexturePack:
// src/tb_texturecache_pack.cpp): CTextureCache::ArrangeNonwrappingTextures and PackTextures.
//
// A case is one call on a world that tests/test_TexturePack.py describes as a flat list of
// numbers (the order is World below): a few globals, the cache's scalars and page counts, 24
// pages (six sizes with four slots each: a link and an OpenGL texture), up to 140 entries, the
// queue's head and, for ArrangeNonwrappingTextures, the array of entries to lay out. The call's
// answer and the state of everything it could change are returned as another flat list, in the
// order the test reads the original's.
//
// Everything outside the two functions is a fake that records its arguments: the texture
// entry's StoreAsWrapping and GetBestSize, CTextureBase::SetFlags, the page's LockSurface,
// UnlockSurface and GetPixelCount, UploadToVRAM, CTextureLink::PageUpdated, the list helpers
// (SortByCriteria, SortByAddress, MergeListsConditional, CreateNonWrappingTextureList), the
// quick sorter and the assertion handler. The fakes that change something do what the
// helpers are taken to do (sort, append); the test describes each in its own comments.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "calllog.h"
#include "platform/msl.h"
#include "tb_printf_opengl.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

CTextureEntry* EntryByNumber(int32_t number);   // entry 1..15 of the world, else null

namespace {
constexpr int32_t kSlots = 4;
constexpr int32_t kPages = CTextureCache::kSizes * kSlots;
constexpr int32_t kMaxEntries = 140;
constexpr int32_t kChainLimit = 64;

// Labels of the world's objects (the test uses the same numbers).
enum : int32_t {
    kLabelCache = 1,
    kLabelSorter = 2,
    kLabelHead = 3,
    kLabelTexture = 10,   // + page
    kLabelLink = 40,      // + page
    kLabelEntry = 100,    // + entry (1-based)
};

// A NaN (as float bits) as one value: its sign and payload depend on the processor.
uint32_t canonical(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t cbits(float value) { return canonical(fbits(value)); }
float from_bits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}
}  // namespace

// ---- The engine around the unit -------------------------------------------------------------
CSystem::CSystem() {}
CSystem gei_System;
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
CQSort gQSort;

void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

namespace {
// The entries of a chain through mNext, up to the limit (a world never has a loop; the limit
// keeps a mistake from hanging the test).
std::vector<CTextureEntry*> Chain(CTextureEntry* head) {
    std::vector<CTextureEntry*> out;
    for (CTextureEntry* entry = head; entry != nullptr && out.size() < kChainLimit; entry = entry->mNext) out.push_back(entry);
    return out;
}

// Records a list helper's call: the cache, the list (a label, or -1 for the unit's own locals),
// how many entries it holds and the first nine of them.
void RecordList(const char* name, const CTextureCache* cache, CTextureEntry** list) {
    const std::vector<CTextureEntry*> chain = Chain(*list);
    int64_t labels[9] = {};
    for (size_t i = 0; i < chain.size() && i < 9; ++i) labels[i] = lab(chain[i]);
    ev(name, {lab(cache), lab(list), static_cast<int64_t>(chain.size()), labels[0], labels[1], labels[2], labels[3],
              labels[4], labels[5], labels[6], labels[7], labels[8]});
}

// Writes a chain back through mNext, in the order given.
void Relink(CTextureEntry** list, const std::vector<CTextureEntry*>& order) {
    *list = order.empty() ? nullptr : order[0];
    for (size_t i = 0; i < order.size(); ++i) order[i]->mNext = i + 1 < order.size() ? order[i + 1] : nullptr;
}
}  // namespace

// StoreAsWrapping and GetBestSize are taken from the entry's flags the way the translated ones
// (tb_textureentry.cpp) answer.
int32_t CTextureEntry::StoreAsWrapping() {
    ev("StoreAsWrapping", {lab(this)});
    return (mFlags & kWrapAny) != 0 && gei_System.mRenderer != 4;
}
int32_t CTextureEntry::GetBestSize() {
    ev("GetBestSize", {lab(this)});
    if ((mFlags & 0x30) == 0x30) return static_cast<int32_t>(mFlags & kSizeMask);
    const int32_t size = mCacheWidth;
    if (size > 0x80) return 0;
    if (size > 0x40) return 1;
    if (size > 0x20) return 2;
    if (size > 0x10) return 3;
    if (size > 8) return 4;
    return 5;
}

// The page's flags: SetFlags keeps bit 0x10000000 and takes the rest from the argument.
void CTextureBase::SetFlags(int32_t flags) {
    ev("SetFlags", {lab(dynamic_cast<void*>(this)), flags});
    mFlags = static_cast<uint32_t>(flags) | (mFlags & kKeptBySetFlags);
}
CTextureBase::~CTextureBase() {}

// The OpenGL page: LockSurface, UnlockSurface and GetPixelCount answer from queues.
COpenGLTexture::COpenGLTexture() {}
COpenGLTexture::~COpenGLTexture() {}
double COpenGLTexture::TestUploadSpeed() { return 0.0; }
int32_t COpenGLTexture::Create(int32_t, int32_t, int32_t, int32_t) { return 0; }
int32_t COpenGLTexture::Destroy() { return 0; }
int32_t COpenGLTexture::MipmapsUsed() { return 0; }
int32_t COpenGLTexture::Download(CTextureEntry*, int32_t, int32_t) { return 0; }
void COpenGLTexture::GetTexture() {}
int32_t COpenGLTexture::LockSurface() {
    ev("LockSurface", {lab(this)});
    return static_cast<int32_t>(ret("Lock", 1));
}
int32_t COpenGLTexture::UnlockSurface() {
    ev("UnlockSurface", {lab(this)});
    return static_cast<int32_t>(ret("Unlock", 1));
}
int32_t COpenGLTexture::GetPixelCount() {
    ev("GetPixelCount", {lab(this)});
    return static_cast<int32_t>(ret("Pixels", 0));
}

// The upload and the page's update: recorded with the quad's four numbers and its entry.
int32_t CTextureCache::UploadToVRAM(CTextureLink* link, SAvailableQuad* quad, CTextureEntry* entry) {
    ev("UploadToVRAM", {lab(this), lab(link), quad->mX, quad->mY, quad->mWidth, quad->mHeight, lab(quad->mEntry), lab(entry)});
    return static_cast<int32_t>(ret("Upload", 1));
}
int32_t CTextureLink::PageUpdated() {
    ev("PageUpdated", {lab(this)});
    return static_cast<int32_t>(ret("PageUpdated", 1));
}

// SortByCriteria puts the chain in order of the sort key, the largest first (signed, equal
// keys keep their order). SortByAddress puts it in order of the entries' addresses.
void CTextureCache::SortByCriteria(CTextureEntry** list) {
    RecordList("SortByCriteria", this, list);
    std::vector<CTextureEntry*> chain = Chain(*list);
    std::stable_sort(chain.begin(), chain.end(), [](CTextureEntry* a, CTextureEntry* b) { return a->mSortKey > b->mSortKey; });
    Relink(list, chain);
}
void CTextureCache::SortByAddress(CTextureEntry** list) {
    RecordList("SortByAddress", this, list);
    std::vector<CTextureEntry*> chain = Chain(*list);
    std::stable_sort(chain.begin(), chain.end(),
                     [](CTextureEntry* a, CTextureEntry* b) { return reinterpret_cast<uintptr_t>(a) < reinterpret_cast<uintptr_t>(b); });
    Relink(list, chain);
}

// MergeListsConditional appends the other chain to the end of the list.
void CTextureCache::MergeListsConditional(CTextureEntry** list, CTextureEntry* other) {
    std::vector<CTextureEntry*> chain = Chain(*list);
    const std::vector<CTextureEntry*> extra = Chain(other);
    int64_t labels[6] = {};
    for (size_t i = 0; i < extra.size() && i < 6; ++i) labels[i] = lab(extra[i]);
    ev("MergeListsConditional", {lab(this), lab(list), lab(other), static_cast<int64_t>(chain.size()),
                                 static_cast<int64_t>(extra.size()), labels[0], labels[1], labels[2], labels[3], labels[4],
                                 labels[5]});
    chain.insert(chain.end(), extra.begin(), extra.end());
    Relink(list, chain);
}

// CreateNonWrappingTextureList fills `out` from the "CreateList" queue (the entries as 4-bit
// numbers 1..15 from the low end, ended by a 0 nibble) and answers the "CreateCount" queue,
// by default the number of entries it listed.
int32_t CTextureCache::CreateNonWrappingTextureList(CTextureEntry** out, CTextureEntry* first, CTextureLink* link) {
    ev("CreateNonWrappingTextureList", {lab(this), lab(first), lab(link)});
    const int64_t packed = ret("CreateList", 0);
    int32_t n = 0;
    for (int32_t shift = 0; shift < 60; shift += 4) {
        const int32_t number = static_cast<int32_t>((packed >> shift) & 0xf);
        if (number == 0) break;
        out[n++] = EntryByNumber(number);
    }
    out[n] = nullptr;
    return static_cast<int32_t>(ret("CreateCount", n));
}

// The quick sorter: records the call, the first four records and a hash of all of them, then
// puts records lo..hi in order of their unsigned key, the largest first (equal keys keep their
// order).
void CQSort::ulDes(void* array, void* scratch, int32_t size, int32_t keyOffset, int32_t lo, int32_t hi) {
    ev("QSort", {lab(this), size, keyOffset, lo, hi});
    auto* base = static_cast<uint8_t*>(array);
    const int32_t words = size / 4;
    uint64_t hash = 0;
    for (int32_t i = lo; i <= hi; ++i) {
        uint32_t record[16] = {};
        std::memcpy(record, base + static_cast<size_t>(i) * size, static_cast<size_t>(size));
        if (i - lo < 4 && (i - lo) % 2 == 0) {
            uint32_t next[16] = {};
            if (i + 1 <= hi && i - lo + 1 < 4) std::memcpy(next, base + static_cast<size_t>(i + 1) * size, static_cast<size_t>(size));
            ev("QSortRecords", {record[0], record[1], record[2], record[3], record[4], next[0], next[1], next[2], next[3], next[4]});
        }
        uint64_t sum = 0;
        for (int32_t k = 0; k < words; ++k) sum += static_cast<uint64_t>(record[k]) * static_cast<uint64_t>(k + 1);
        hash = hash * 1000003u + sum;
    }
    ev("QSortHash", {hi - lo + 1, static_cast<int64_t>(hash)});
    std::vector<std::vector<uint8_t>> records;
    for (int32_t i = lo; i <= hi; ++i) records.emplace_back(base + static_cast<size_t>(i) * size, base + static_cast<size_t>(i + 1) * size);
    std::stable_sort(records.begin(), records.end(), [keyOffset](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
        uint32_t ka, kb;
        std::memcpy(&ka, a.data() + keyOffset, 4);
        std::memcpy(&kb, b.data() + keyOffset, 4);
        return ka > kb;
    });
    for (int32_t i = lo; i <= hi; ++i) std::memcpy(base + static_cast<size_t>(i) * size, records[static_cast<size_t>(i - lo)].data(), static_cast<size_t>(size));
    (void)scratch;
}

// ---- The world --------------------------------------------------------------------------------
namespace {
alignas(16) unsigned char gCacheStore[sizeof(CTextureCache) + 16];
CTextureCache* gCache;
CTextureLink gLinks[kPages];
COpenGLTexture gTextures[kPages];
CTextureEntry gEntries[kMaxEntries];
CTextureEntry* gHead;                                // the queue
CTextureEntry* gArray[kMaxEntries + 2];              // the entries to lay out
SAvailableQuad gQuads[0x80 + 1];                     // ArrangeNonwrappingTextures' answer (and one past it)
int32_t gQuadCount;
int32_t gEntryCount;

// The description is read in order.
struct Reader {
    const int64_t* data;
    int32_t at;
    int64_t next() { return data[at++]; }
    int32_t i32() { return static_cast<int32_t>(next()); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    float f32() { return from_bits(u32()); }
};

CTextureEntry* EntryAt(int32_t index) { return index > 0 && index <= gEntryCount ? &gEntries[index - 1] : nullptr; }

int64_t* gOut;
int32_t gCount, gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOut[gCount] = value;
    ++gCount;
}
}  // namespace

CTextureEntry* EntryByNumber(int32_t number) { return EntryAt(number); }

extern "C" {

// Builds the world from the description, runs operation `op` with arguments a[0..2] and puts
// the state afterwards (as numbers) in out; returns the operation's answer. The caller has
// reset the call log and set its queues.
//   1 ArrangeNonwrappingTextures(the array, quads, count, page a[0] * 4 + a[1], a[2])
//   2 PackTextures(&queue)
int64_t tpw_tp_run(int32_t op, const int64_t* a, const int64_t* description, int64_t* out, int32_t capacity) {
    Reader in{description, 0};
    gOut = out;
    gCapacity = capacity;
    gCount = 0;

    // Globals.
    gei_System.mRenderer = in.i32();
    gTextureCutDowns = in.i32();
    gTextureQuadsPending = in.i32();
    gTextureQuadsPlaced = in.i32();
    gTextureSizeSteps = in.i32();

    // The cache.
    gCache = reinterpret_cast<CTextureCache*>(gCacheStore);
    std::memset(static_cast<void*>(gCache), 0xa5, sizeof(CTextureCache));
    std::memset(static_cast<void*>(gCache->mPages), 0, sizeof gCache->mPages);
    gCache->mFlags = in.u32();
    gCache->mSizeMask = in.i32();
    gCache->mMipmaps = in.i32();
    gCache->mWidest = in.i32();
    gCache->mTallest = in.i32();
    gCache->mReordered = in.i32();
    for (int32_t& count : gCache->mPageCount) count = in.i32();
    for (int32_t& value : gCache->mUsable) value = in.i32();
    for (int32_t& value : gCache->mF1830) value = in.i32();
    for (int32_t size = 0; size < CTextureCache::kSizes; ++size) {
        for (int32_t slot = 0; slot < kSlots; ++slot) gCache->mPages[size][slot] = &gLinks[size * kSlots + slot];
    }

    // The pages. The entries a link holds are read with the entries, below.
    int32_t linkHeads[kPages];
    for (int32_t i = 0; i < kPages; ++i) {
        CTextureLink& link = gLinks[i];
        link.mTexture = &gTextures[i];
        linkHeads[i] = in.i32();
        link.mF08 = in.i32();
        link.mF0C = in.i32();
        link.mF10 = in.i32();
        link.mF14 = in.i32();
        link.mF18 = in.i32();
        link.mF1C = in.i32();
        gTextures[i].mFlags = in.u32();
        gTextures[i].mWidth = in.i32();
        gTextures[i].mHeight = in.i32();
        gTextures[i].mPitch = in.i32();
    }

    // The queue, the entries and the array to lay out.
    const int32_t head = in.i32();
    gEntryCount = in.i32();
    for (int32_t i = 0; i < gEntryCount; ++i) {
        CTextureEntry& entry = gEntries[i];
        std::memset(static_cast<void*>(&entry), 0, sizeof entry);
        entry.mFlags = in.u32();
        entry.mWidth = static_cast<int16_t>(in.i32());
        entry.mHeight = static_cast<int16_t>(in.i32());
        entry.mCacheWidth = static_cast<int16_t>(in.i32());
        entry.mCacheHeight = static_cast<int16_t>(in.i32());
        entry.mPriority = in.f32();
        entry.mSortKey = in.i32();
        entry.mNext = EntryAt(in.i32());
    }
    for (int32_t i = 0; i < kPages; ++i) gLinks[i].mEntries = EntryAt(linkHeads[i]);
    gHead = EntryAt(head);
    const int32_t listLength = in.i32();
    for (int32_t i = 0; i < kMaxEntries + 2; ++i) gArray[i] = nullptr;
    for (int32_t i = 0; i < listLength; ++i) gArray[i] = EntryAt(in.i32());
    for (SAvailableQuad& quad : gQuads) {
        quad.mX = quad.mY = quad.mWidth = quad.mHeight = 0x5a5a5a5a;
        quad.mEntry = reinterpret_cast<CTextureEntry*>(static_cast<uintptr_t>(0x5a5a5a5a5a5a5a5aull));
    }
    gQuadCount = 0x5a5a5a5a;

    // The labels.
    tpw_cl_label(kLabelCache, gCache);
    tpw_cl_label(kLabelSorter, &gQSort);
    tpw_cl_label(kLabelHead, &gHead);
    for (int32_t i = 0; i < kPages; ++i) {
        tpw_cl_label(kLabelTexture + i, dynamic_cast<void*>(&gTextures[i]));
        tpw_cl_label(kLabelLink + i, &gLinks[i]);
    }
    for (int32_t i = 0; i < gEntryCount; ++i) tpw_cl_label(kLabelEntry + 1 + i, &gEntries[i]);

    // The call.
    int64_t result = 0;
    if (op == 1) {
        CTextureBase* page = &gTextures[a[0] * kSlots + a[1]];
        result = gCache->ArrangeNonwrappingTextures(gArray, gQuads, &gQuadCount, page, static_cast<int32_t>(a[2])) ? 1 : 0;
    } else {
        result = gCache->PackTextures(&gHead);
    }

    // The state, in the order test_TexturePack.py reads the original's.
    put(gTextureCutDowns);
    put(gTextureQuadsPending);
    put(gTextureQuadsPlaced);
    put(gTextureSizeSteps);
    put(gCache->mFlags);
    put(gCache->mSizeMask);
    put(gCache->mMipmaps);
    put(gCache->mWidest);
    put(gCache->mTallest);
    put(gCache->mReordered);
    for (int32_t value : gCache->mPageCount) put(value);
    for (int32_t value : gCache->mUsable) put(value);
    for (int32_t value : gCache->mF1830) put(value);
    put(lab(gHead));
    for (int32_t i = 0; i < kPages; ++i) {
        const CTextureLink& link = gLinks[i];
        put(lab(link.mEntries));
        put(link.mF08);
        put(link.mF0C);
        put(link.mF10);
        put(link.mF14);
        put(link.mF18);
        put(link.mF1C);
        put(gTextures[i].mFlags);
        put(gTextures[i].mWidth);
        put(gTextures[i].mHeight);
        put(gTextures[i].mPitch);
    }
    for (int32_t i = 0; i < gEntryCount; ++i) {
        const CTextureEntry& e = gEntries[i];
        put(lab(e.mNext));
        put(e.mFlags);
        put(e.mCacheWidth);
        put(e.mCacheHeight);
        put(e.mSortKey);
        put(cbits(e.mPriority));
        put(e.mWidth);
        put(e.mHeight);
    }
    put(gQuadCount);
    for (int32_t i = 0; i <= listLength; ++i) put(lab(gArray[i]));
    for (const SAvailableQuad& quad : gQuads) {
        put(quad.mX);
        put(quad.mY);
        put(quad.mWidth);
        put(quad.mHeight);
        put(lab(quad.mEntry));
    }
    return result;
}

// How many numbers the last run wanted to put (more than the capacity it was given if the
// test's buffer was too small).
int32_t tpw_tp_count() { return gCount; }
}
