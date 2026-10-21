// Test entry points for the texture cache's sorting and lists (library unit_TextureSort:
// src/engine/TextureCache.cpp and src/engine/QSort.cpp): CQSort's eight sorts, and the cache's
// Textures_QSort, Textures_QSortFree, SortTextures, Entry_QSortInit, SortEntries,
// SortByCriteria, SortByAddress, MergeListsConditional and CreateNonWrappingTextureList.
//
// Two entry points.
//   tpw_ts_qsort: one CQSort call on a buffer of records, or on a list of pointers into them.
//   tpw_ts_run: one call of a cache function on a small world that tests/test_TextureSort.py
//   describes as a flat list of numbers (the order is World below): the cache with six rows
//   of pages, up to 128 pages (a link and an OpenGL texture each), up to 4,400 texture entries
//   in one array, four spare place records, an array of entries to sort and a list head. The
//   call's answer and the state of everything it could change come back as another flat list,
//   in the order the test reads the original's.
//
// Everything outside the unit is a fake that records its arguments: the entries'
// FlushEntryFlags and StoreAsWrapping, the pages' CalcUsage and (virtual) GetPixelCount, the
// cache's AutoMipmap and Entry_Compress and the assertion handler. Entries and pages are
// named in the records by their number (1 for the first), not by a call-log label, since the
// world holds thousands of entries.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/msl.h"
#include "tb_printf_opengl.h"
#include "tb_qsort.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {
constexpr int32_t kMaxLinks = 128;
constexpr int32_t kMaxEntries = 4400;
constexpr int32_t kPlaces = 4;
constexpr int32_t kSizes = CTextureCache::kSizes;

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

// ---- The engine around the unit ---------------------------------------------------------------
uint32_t gFrequencyIndex = 0;   // defined in src/engine/TextureCache.cpp (another unit)
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
CTextureBase::~CTextureBase() {}

// ---- The world --------------------------------------------------------------------------------
namespace {
alignas(16) unsigned char gCacheStore[sizeof(CTextureCache) + 16];
CTextureCache* gCache;
CTextureLink gLinks[kMaxLinks];
COpenGLTexture gTextures[kMaxLinks];
CTextureEntry gEntries[kMaxEntries];
STexturePlace gPlaces[kPlaces];
CTextureEntry* gArray[kMaxEntries];
CTextureEntry* gOut[kMaxEntries + 8];
CTextureEntry* gHead;
int32_t gLinkCount, gEntryCount, gArrayCount;

// What the fakes answer or do, per page and per entry, and for the cache's two steps.
struct LinkFake {
    int32_t pixels;       // GetPixelCount's answer
    int32_t usageSet;     // CalcUsage assigns mF08, mF0C and mF10 from the first or later triple
    int32_t first[3];
    int32_t later[3];
    int32_t calls;        // CalcUsage calls so far
};
LinkFake gLinkFake[kMaxLinks];
int32_t gWrapAnswer[kMaxEntries];       // StoreAsWrapping's answer per entry
int32_t gMipmapSets, gMipmapValue;      // AutoMipmap assigns mF18C4
int32_t gCompressSets, gCompressValue;  // Entry_Compress assigns mF18C4

int32_t EntryNo(const CTextureEntry* entry) {
    if (entry == nullptr) return 0;
    if (entry >= gEntries && entry < gEntries + kMaxEntries) return static_cast<int32_t>(entry - gEntries) + 1;
    return -1;
}
int32_t LinkNo(const CTextureLink* link) {
    if (link == nullptr) return 0;
    if (link >= gLinks && link < gLinks + kMaxLinks) return static_cast<int32_t>(link - gLinks) + 1;
    return -1;
}
// The page whose COpenGLTexture (the complete object) is at `whole`.
const void* gWhole[kMaxLinks];
int32_t TextureNo(const void* whole) {
    if (whole == nullptr) return 0;
    for (int32_t i = 0; i < kMaxLinks; ++i) {
        if (gWhole[i] == whole) return i + 1;
    }
    return -1;
}

// A hash of the entries a fake was given, in order (the test computes the same).
int64_t HashEntries(CTextureEntry** entries, int32_t count) {
    uint64_t hash = 0;
    for (int32_t i = 0; i < count; ++i) hash = (hash * 1000003u + static_cast<uint64_t>(EntryNo(entries[i]) + 1)) & ((1ull << 60) - 1);
    return static_cast<int64_t>(hash);
}
}  // namespace

// ---- The fakes -------------------------------------------------------------------------------
// FlushEntryFlags records the flags it sees and does nothing (the original clears 0x7000).
void CTextureEntry::FlushEntryFlags() { ev("FlushEntryFlags", {EntryNo(this), mFlags}); }

int32_t CTextureEntry::StoreAsWrapping() {
    const int32_t number = EntryNo(this);
    ev("StoreAsWrapping", {number});
    return number > 0 ? gWrapAnswer[number - 1] : 0;
}

// CalcUsage records the page and the flags and may assign the three counters the sorts read.
void CTextureLink::CalcUsage(int32_t cacheFlags) {
    const int32_t number = LinkNo(this);
    ev("CalcUsage", {number, cacheFlags});
    LinkFake& fake = gLinkFake[number - 1];
    if (fake.usageSet != 0) {
        const int32_t* triple = fake.calls == 0 ? fake.first : fake.later;
        mF08 = triple[0];
        mF0C = triple[1];
        mF10 = triple[2];
    }
    ++fake.calls;
}

void CTextureCache::AutoMipmap(CTextureEntry** entries, int32_t count) {
    ev("AutoMipmap", {count, HashEntries(entries, count), EntryNo(count > 0 ? entries[0] : nullptr), mF18C4, mF18E0,
                      static_cast<int64_t>(mFlags)});
    if (gMipmapSets != 0) mF18C4 = gMipmapValue;
}

void CTextureCache::Entry_Compress(CTextureEntry** entries, int32_t count) {
    ev("Entry_Compress", {count, HashEntries(entries, count), EntryNo(count > 0 ? entries[0] : nullptr), mF18C4, mF18E0,
                          static_cast<int64_t>(mFlags)});
    if (gCompressSets != 0) mF18C4 = gCompressValue;
}

// The OpenGL page: only GetPixelCount records.
COpenGLTexture::COpenGLTexture() {}
COpenGLTexture::~COpenGLTexture() {}
double COpenGLTexture::TestUploadSpeed() { return 0.0; }
int32_t COpenGLTexture::Create(int32_t, int32_t, int32_t, int32_t) { return 0; }
int32_t COpenGLTexture::Destroy() { return 0; }
int32_t COpenGLTexture::LockSurface() { return 0; }
int32_t COpenGLTexture::UnlockSurface() { return 0; }
int32_t COpenGLTexture::MipmapsUsed() { return 0; }
int32_t COpenGLTexture::Download(CTextureEntry*, int32_t, int32_t) { return 0; }
void COpenGLTexture::GetTexture() {}
int32_t COpenGLTexture::GetPixelCount() {
    const int32_t number = TextureNo(this);
    ev("GetPixelCount", {number});
    return number > 0 ? gLinkFake[number - 1].pixels : 0;
}

// ---- CQSort on a buffer ---------------------------------------------------------------------------
extern "C" {

// One CQSort call. `records` holds the records (count * recordSize bytes) and `order` the
// indexes of the records a pointer sort starts from (the host builds the pointers). op:
//   1 ulAsc 2 ulDes 3 pulAsc 4 pfDes (the three-argument forms, after the object is set)
//   5 ulAsc 6 ulDes 7 pulAsc 8 pfDes (the entry points that set it).
// The record sorts work in place; the pointer sorts leave the new order in `order`.
// out: the object's size, key offset and temp afterwards (a record number from 1 for a
// pointer into the records, -2 for `tmp`, 0 for null, -3 for anything else).
void tpw_ts_qsort(int32_t op, uint8_t* records, int32_t recordSize, int32_t* order, uint8_t* tmp, int32_t size,
                  int32_t keyOffset, int32_t low, int32_t high, int32_t count, int64_t* out) {
    CQSort sorter;
    sorter.mSize = 0x1111;
    sorter.mKeyOffset = 0x2222;
    sorter.mTemp = nullptr;
    void* pointers[512];
    for (int32_t i = 0; i < count && i < 512; ++i) pointers[i] = records + static_cast<size_t>(order[i]) * recordSize;
    const bool pointerSort = op == 3 || op == 4 || op == 7 || op == 8;
    void* data = pointerSort ? static_cast<void*>(pointers) : static_cast<void*>(records);
    if (op <= 4) {
        sorter.mSize = size;
        sorter.mKeyOffset = keyOffset;
        sorter.mTemp = tmp;
    }
    switch (op) {
    case 1: sorter.ulAsc(data, low, high); break;
    case 2: sorter.ulDes(data, low, high); break;
    case 3: sorter.pulAsc(data, low, high); break;
    case 4: sorter.pfDes(data, low, high); break;
    case 5: sorter.ulAsc(data, tmp, size, keyOffset, low, high); break;
    case 6: sorter.ulDes(data, tmp, size, keyOffset, low, high); break;
    case 7: sorter.pulAsc(data, tmp, size, keyOffset, low, high); break;
    case 8: sorter.pfDes(data, tmp, size, keyOffset, low, high); break;
    }
    if (pointerSort) {
        for (int32_t i = 0; i < count && i < 512; ++i) {
            const auto offset = static_cast<const uint8_t*>(pointers[i]) - records;
            order[i] = static_cast<int32_t>(offset / recordSize);
        }
    }
    out[0] = sorter.mSize;
    out[1] = sorter.mKeyOffset;
    const uint8_t* temp = static_cast<const uint8_t*>(sorter.mTemp);
    if (temp == nullptr) out[2] = 0;
    else if (temp == tmp) out[2] = -2;
    else if (temp >= records && temp < records + static_cast<size_t>(count) * recordSize) out[2] = (temp - records) / recordSize + 1;
    else out[2] = -3;
}

}  // extern "C"

// ---- The cache functions on a world -----------------------------------------------------------------
namespace {

// The description is read in order.
struct Reader {
    const int64_t* data;
    int32_t at;
    int64_t next() { return data[at++]; }
    int32_t i32() { return static_cast<int32_t>(next()); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    float f32() { return from_bits(u32()); }
};

CTextureEntry* EntryAt(int32_t index) { return index > 0 ? &gEntries[index - 1] : nullptr; }
CTextureLink* LinkAt(int32_t index) { return index > 0 ? &gLinks[index - 1] : nullptr; }

void ReadPlace(Reader& in, STexturePlace* place) {
    for (uint8_t& byte : place->mF00) byte = static_cast<uint8_t>(in.next());
    place->mF10 = in.i32();
    place->mF14 = in.i32();
}

int64_t* gOutData;
int32_t gCount, gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOutData[gCount] = value;
    ++gCount;
}

void PutPlace(const STexturePlace* place) {
    for (uint8_t byte : place->mF00) put(byte);
    put(place->mF10);
    put(place->mF14);
}

// Which place record an entry uses: 0 its own, 1..4 a spare one, -1 anything else.
int32_t PlaceCode(const CTextureEntry& entry) {
    if (entry.mPlace == &entry.mOwnPlace) return 0;
    for (int32_t i = 0; i < kPlaces; ++i) {
        if (entry.mPlace == &gPlaces[i]) return i + 1;
    }
    return -1;
}

// The sorter's words as the Mac holds them: the host passes its own record sizes and key
// offsets, which are mapped back to the Mac's.
int64_t MacSize(int32_t size) {
    if (size == static_cast<int32_t>(sizeof(CTextureEntry))) return 0x5c;
    if (size == static_cast<int32_t>(sizeof(CTextureEntry*))) return 4;
    return size;
}
int64_t MacKeyOffset(int32_t offset) {
    if (offset == static_cast<int32_t>(offsetof(CTextureEntry, mSortKey))) return 0x34;
    if (offset == static_cast<int32_t>(offsetof(CTextureEntry, mPriority))) return 0x2c;
    return offset;
}

}  // namespace

extern "C" {

// Builds the world from the description, runs operation `op` with arguments a[0..3] and puts the
// state afterwards (as numbers) in out; returns the operation's answer. The caller has reset the
// call log.
//   1 Textures_QSort(row a0, low a1, high a2)      2 Textures_QSortFree(row a0, low a1, high a2)
//   3 SortTextures()                               4 Entry_QSortInit(the array, low a0, high a1)
//   5 SortEntries()                                6 SortByCriteria(&head)
//   7 SortByAddress(&head)                         8 MergeListsConditional(&head, entry a0)
//   9 CreateNonWrappingTextureList(out, entry a0, link a1)
int64_t tpw_ts_run(int32_t op, const int64_t* a, const int64_t* description, int64_t* out, int32_t capacity) {
    Reader in{description, 0};
    gOutData = out;
    gCapacity = capacity;
    gCount = 0;
    gLinkCount = in.i32();
    gEntryCount = in.i32();
    gArrayCount = in.i32();

    // The globals: the frame index, the sorter's words before the call (the unit never uses
    // the values), the fakes' answers.
    gFrequencyIndex = in.u32();
    gQSort.mSize = in.i32();
    gQSort.mKeyOffset = in.i32();
    gQSort.mTemp = nullptr;
    gMipmapSets = in.i32();
    gMipmapValue = in.i32();
    gCompressSets = in.i32();
    gCompressValue = in.i32();

    // The cache.
    gCache = reinterpret_cast<CTextureCache*>(gCacheStore);
    std::memset(static_cast<void*>(gCache), 0xa5, sizeof(CTextureCache));
    gCache->mFlags = in.u32();
    gCache->mF18E0 = in.i32();
    gCache->mF18E4 = in.i32();
    gCache->mF18B8 = in.i32();
    gCache->mF18BC = in.i32();
    gCache->mF18C0 = in.i32();
    gCache->mF18C4 = in.i32();
    gCache->mF18AC = EntryAt(in.i32());
    for (int32_t& value : gCache->mF1818) value = in.i32();
    gCache->mF18EC = in.f32();
    gCache->mF18F0 = in.f32();
    gCache->mF18F4 = in.f32();
    gCache->mF18F8 = in.f32();
    gCache->mF18FC = in.f32();
    for (int32_t& count : gCache->mPageCount) count = in.i32();
    for (int32_t size = 0; size < kSizes; ++size) {
        for (CTextureLink*& page : gCache->mPages[size]) page = nullptr;
        for (int32_t k = 0; k < gCache->mPageCount[size]; ++k) gCache->mPages[size][k] = LinkAt(in.i32());
    }

    // The pages.
    for (int32_t i = 0; i < gLinkCount; ++i) {
        CTextureLink& link = gLinks[i];
        link.mTexture = &gTextures[i];
        gWhole[i] = dynamic_cast<void*>(&gTextures[i]);
        link.mEntries = EntryAt(in.i32());
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
        LinkFake& fake = gLinkFake[i];
        fake.pixels = in.i32();
        fake.usageSet = in.i32();
        for (int32_t& value : fake.first) value = in.i32();
        for (int32_t& value : fake.later) value = in.i32();
        fake.calls = 0;
    }

    // The spare place records, then the entries.
    for (STexturePlace& place : gPlaces) ReadPlace(in, &place);
    for (int32_t i = 0; i < gEntryCount; ++i) {
        CTextureEntry& entry = gEntries[i];
        entry.mFlags = in.u32();
        entry.mWidth = static_cast<int16_t>(in.i32());
        entry.mHeight = static_cast<int16_t>(in.i32());
        entry.mCacheWidth = static_cast<int16_t>(in.i32());
        entry.mCacheHeight = static_cast<int16_t>(in.i32());
        entry.mPage = LinkAt(in.i32());
        const int32_t texture = in.i32();
        entry.mTexture = texture > 0 ? reinterpret_cast<CTextureBase*>(dynamic_cast<void*>(&gTextures[texture - 1])) : nullptr;
        entry.mUSize = in.f32();
        entry.mVSize = in.f32();
        entry.mUOffset = in.f32();
        entry.mVOffset = in.f32();
        entry.mPriority = in.f32();
        entry.mPitch = static_cast<int16_t>(in.i32());
        entry.mSortKey = in.i32();
        entry.mNext = EntryAt(in.i32());
        entry.mF38 = nullptr;
        entry.mData = nullptr;
        gWrapAnswer[i] = in.i32();
        const int32_t place = in.i32();
        entry.mPlace = place > 0 ? &gPlaces[place - 1] : &entry.mOwnPlace;
        ReadPlace(in, &entry.mOwnPlace);
    }
    for (int32_t i = 0; i < gArrayCount; ++i) gArray[i] = EntryAt(in.i32());
    gHead = EntryAt(in.i32());
    for (CTextureEntry*& slot : gOut) slot = reinterpret_cast<CTextureEntry*>(1);

    // The call.
    int64_t result = 0;
    switch (op) {
    case 1: gCache->Textures_QSort(gCache->mPages[a[0]], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 2: gCache->Textures_QSortFree(gCache->mPages[a[0]], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 3: gCache->SortTextures(); break;
    case 4: gCache->Entry_QSortInit(gArray, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])); break;
    case 5: gCache->SortEntries(); break;
    case 6: gCache->SortByCriteria(&gHead); break;
    case 7: gCache->SortByAddress(&gHead); break;
    case 8: gCache->MergeListsConditional(&gHead, EntryAt(static_cast<int32_t>(a[0]))); break;
    case 9:
        result = gCache->CreateNonWrappingTextureList(gOut, EntryAt(static_cast<int32_t>(a[0])), LinkAt(static_cast<int32_t>(a[1])));
        break;
    }

    // The state, in the order test_TextureSort.py reads the original's.
    put(MacSize(gQSort.mSize));
    put(MacKeyOffset(gQSort.mKeyOffset));
    {
        const void* temp = gQSort.mTemp;
        put(temp == nullptr ? 0 : EntryNo(static_cast<const CTextureEntry*>(temp)));
    }
    put(EntryNo(gCache->mF18AC));
    put(gCache->mFlags);
    put(gCache->mF18E0);
    put(gCache->mF18E4);
    put(gCache->mF18B8);
    put(gCache->mF18BC);
    put(gCache->mF18C0);
    put(gCache->mF18C4);
    for (int32_t value : gCache->mF1818) put(value);
    put(cbits(gCache->mF18EC));
    put(cbits(gCache->mF18F0));
    put(cbits(gCache->mF18F4));
    put(cbits(gCache->mF18F8));
    put(cbits(gCache->mF18FC));
    for (int32_t count : gCache->mPageCount) put(count);
    for (int32_t size = 0; size < kSizes; ++size) {
        for (int32_t k = 0; k < gCache->mPageCount[size] + 2; ++k) put(LinkNo(gCache->mPages[size][k]));
    }
    for (int32_t i = 0; i < gLinkCount; ++i) {
        const CTextureLink& link = gLinks[i];
        put(EntryNo(link.mEntries));
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
    for (const STexturePlace& place : gPlaces) PutPlace(&place);
    for (int32_t i = 0; i < gEntryCount; ++i) {
        const CTextureEntry& e = gEntries[i];
        put(PlaceCode(e));
        put(EntryNo(e.mNext));
        put(e.mFlags);
        put(e.mWidth);
        put(e.mHeight);
        put(e.mCacheWidth);
        put(e.mCacheHeight);
        put(LinkNo(e.mPage));
        put(TextureNo(reinterpret_cast<const void*>(e.mTexture)));
        put(cbits(e.mUSize));
        put(cbits(e.mVSize));
        put(cbits(e.mUOffset));
        put(cbits(e.mVOffset));
        put(cbits(e.mPriority));
        put(e.mPitch);
        put(e.mSortKey);
        PutPlace(&e.mOwnPlace);
    }
    for (int32_t i = 0; i < gArrayCount; ++i) put(EntryNo(gArray[i]));
    put(EntryNo(gHead));
    for (int32_t i = 0; i < gEntryCount + 3; ++i) put(EntryNo(gOut[i]));
    return result;
}

// How many numbers the last run wanted to put (more than the capacity it was given if the
// test's buffer was too small).
int32_t tpw_ts_count() { return gCount; }

// 1 when the unit is built with the port fixes on (unit v5L47): SortEntries, SortByCriteria and
// SortByAddress then take a list longer than 4096 entries whole.
int32_t tpw_ts_portfix() {
    return 0;
}
}
