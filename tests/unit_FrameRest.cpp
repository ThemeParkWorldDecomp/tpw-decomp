// Harness for the last stand-ins of the lobby's frame (library unit_FrameRest: src/engine/tb_hfield.cpp
// and src/engine/Gamut.cpp, src/engine/TextureCache.cpp): CHField::TestConQuad, CycleFrequencyTable, CTextureLink::CalcUsage,
// CTextureCache::AutoMipmap, Entry_Compress, BalanceCache_FreeLockedTextureSpace and
// BalanceCache_LevelSurfaceUsage.
//
// Entry points (tests/test_FrameRest.py builds the same worlds in emulator memory):
//   tpw_fr_conquad  one TestConQuad call on a height field object with the box, the rows and the far plane
//                   the test describes; the fakes are the math unit's strided Transform, CClipping::CPP_TestData
//                   and the assertion.
//   tpw_fr_cycle    one CycleFrequencyTable call.
//   tpw_fr_run      one call of a cache function on a small world described as a flat list: the cache, pages
//                   (a link and an OpenGL texture each), entries, an array of entries. The fakes are the
//                   entries' StoreAsWrapping, the textures' SetFlags and GetPixelCount, the cache's
//                   Entry_Specific_Compress, BalanceCache_ClearTexture, SortTextures and SortEntries, and the
//                   assertion. The answer and the state of everything the call could change come back as a
//                   flat list in the order the test reads the original's.
//
//   tpw_fr_render   the render memory's deferred drawing (src/engine/RenderMemory.cpp): one call of
//                   ExecuteUnrenderedPrimitives or of one of the three sorts on a table of primitives described
//                   by the test. The fakes are CDrawPrimitive::ExecutePrimitiveIfInstant and ExternExecute,
//                   the engine's malloc and free callbacks and CErrorHandler::Push.
//
// NaNs are one value wherever a word is recorded, dumped or compared: the two processors make
// different ones.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "Wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t CBits(float value) {
    uint32_t word;
    std::memcpy(&word, &value, 4);
    return Canon(word);
}
float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// The description is read in order.
struct Reader {
    const int64_t* data;
    int32_t at;
    int64_t next() { return data[at++]; }
    int32_t i32() { return static_cast<int32_t>(next()); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    float f32() { return FromBits(u32()); }
};

// The numbers a call puts out.
int64_t* gOutData;
int32_t gCount, gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOutData[gCount] = value;
    ++gCount;
}

// What the fakes saw, in words.
std::vector<uint32_t> gDump;

struct AssertionRaised {};
bool gAssertionThrows = false;

}  // namespace

// ---- The engine objects the unit reads --------------------------------------------------------------
CSystem::CSystem() {}
CSystem gei_System;
CDisplay::CDisplay() {}
CDisplay gei_Display;
uint32_t gFrequencyIndex = 0;   // defined in src/engine/TextureCache.cpp (another unit)
int32_t gFrequencyTable[16];    // defined in src/engine/Gamut.cpp (another unit)

// The assertion records. In a case that takes the math unit away it also throws: the original goes on to
// read through the NULL pointer (the PowerPC side faults there), which the host must not do.
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
    if (gAssertionThrows) throw AssertionRaised();
}

// ================================================================================================
// CHField::TestConQuad
// ================================================================================================
namespace {

alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
sHField gRecord;
CClipping gClipping;
CMatrix gFinalMatrix;

// The case's fakes: the depths (third word) Transform gives the corners, cyclic over the list; the AND and
// OR CPP_TestData answers, one pair per call from the list and then from a hash of what it was given.
std::vector<uint32_t> gDepths;
std::vector<uint32_t> gAnswers;   // pairs: AND, OR
int32_t gThresholdOutside, gThresholdPartial;
int32_t gTransformCalls, gTestCalls, gDepthAt;

uint32_t HashWords(const uint32_t* words, int32_t count) {
    uint32_t h = 0;
    for (int32_t i = 0; i < count; ++i) h = h * 1000003u + words[i];
    return h;
}

// The math unit: only the strided Transform is called. It records, dumps the eight points it was given
// (x, y, z each), and writes x, y, w from those words and z from the case's list.
class FakeMath : public CTransformBase {
public:
    ~FakeMath() override {}
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override { ev("Transform6"); }
    void Transform(sMatrix* m, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                   int32_t type) override {
        ev("Transform", {lab(this), lab(m), outStride, inStride, count, type});
        ++gTransformCalls;
        for (int32_t i = 0; i < count; ++i) {
            uint32_t w[3];
            std::memcpy(w, static_cast<const uint8_t*>(in) + static_cast<int64_t>(i) * inStride, sizeof w);
            for (uint32_t& word : w) {
                word = Canon(word);
                gDump.push_back(word);
            }
            const uint32_t depth = gDepths.empty() ? 0u : gDepths[static_cast<size_t>(gDepthAt++) % gDepths.size()];
            const uint32_t r[4] = {w[0] * 3 + 1, w[1] * 5 + 2, depth, w[0] + w[1] + w[2] + static_cast<uint32_t>(i)};
            std::memcpy(static_cast<uint8_t*>(out) + static_cast<int64_t>(i) * outStride, r, sizeof r);
        }
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMath gMath;

}  // namespace

CTransformBase::~CTransformBase() {}
CTransformBase* gei_MathUnit = &gMath;
CClipping* gei_Clipping = &gClipping;
CMatrix* gei_FinalMatrix = &gFinalMatrix;

// Records its arguments, dumps the vertices it was given (x, y, z, w) and writes the AND and OR of the
// codes: from the case's list of pairs, then from a hash of the vertices with the case's two thresholds
// (a byte under the first threshold: all eight outside one plane; under the second: partly outside).
void CClipping::CPP_TestData(uint8_t* result, void* clipping, uint8_t* codes, const void* vertices, int32_t count,
                             int32_t stride, uint32_t mask) {
    ev("CPP_TestData", {lab(clipping), count, stride, mask});
    uint32_t words[32];
    int32_t n = 0;
    for (int32_t i = 0; i < count; ++i) {
        uint32_t w[4];
        std::memcpy(w, static_cast<const uint8_t*>(vertices) + static_cast<int64_t>(i) * stride, sizeof w);
        for (uint32_t& word : w) {
            word = Canon(word);
            gDump.push_back(word);
            if (n < 32) words[n++] = word;
        }
        codes[i] = static_cast<uint8_t>(w[2] >> 7) & mask;
    }
    const size_t call = static_cast<size_t>(gTestCalls++);
    uint32_t anded, ored;
    if (2 * call + 1 < gAnswers.size()) {
        anded = gAnswers[2 * call];
        ored = gAnswers[2 * call + 1];
    } else {
        const uint32_t h = HashWords(words, n);
        anded = static_cast<int32_t>(h & 0xff) < gThresholdOutside ? 0x20u : 0u;
        ored = static_cast<int32_t>((h >> 8) & 0xff) < gThresholdPartial ? 0x04u : 0u;
    }
    result[0] = static_cast<uint8_t>(anded);
    result[1] = static_cast<uint8_t>(ored);
}

extern "C" {

// Addresses the log labels: 0 the math unit, 1 the matrix, 2 the clipper.
void* tpw_fr_pointer(int32_t kind) {
    switch (kind) {
    case 0: return gei_MathUnit;
    case 1: return gei_FinalMatrix;
    default: return gei_Clipping;
    }
}

uint32_t tpw_fr_dump_n() { return static_cast<uint32_t>(gDump.size()); }
void tpw_fr_dump_copy(uint32_t* out) { std::memcpy(out, gDump.data(), gDump.size() * 4); }

// One TestConQuad call. The description: the record's origin x, z, cell size x, z and the lowest and
// highest height (float bits); the far plane mF1C (bits), mF20, mF24, the 128 low and 128 high column
// bytes; the box x0, z0, x1, z1; whether the math unit is missing; the depth list (count, bits); the answer
// pairs (count of pairs, then AND, OR each); the two thresholds. out: whether the assertion stopped the
// call, mF1C (canonical bits), mF20, mF24, the 256 column bytes, whether the rest of the object is
// unchanged, whether the record is unchanged. Returns the number of numbers put.
int32_t tpw_fr_conquad(const int64_t* description, int64_t* out, int32_t capacity) {
    Reader in{description, 0};
    gOutData = out;
    gCapacity = capacity;
    gCount = 0;
    gDump.clear();
    gTransformCalls = gTestCalls = gDepthAt = 0;
    std::memset(static_cast<void*>(&gRecord), 0xa5, sizeof gRecord);
    gRecord.mF08[0] = in.f32();
    gRecord.mF08[1] = in.f32();
    gRecord.mF08[2] = in.f32();
    gRecord.mF08[3] = in.f32();
    gRecord.mF20[0] = in.f32();
    gRecord.mF20[1] = in.f32();
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    field.mF00 = &gRecord;
    field.mF1C = in.f32();
    field.mF20 = in.i32();
    field.mF24 = in.i32();
    SGamutQuads* rows = field.VertexRows();
    for (int8_t& low : rows->mLow) low = static_cast<int8_t>(in.i32());
    for (int8_t& high : rows->mHigh) high = static_cast<int8_t>(in.i32());
    const int32_t x0 = in.i32();
    const int32_t z0 = in.i32();
    const int32_t x1 = in.i32();
    const int32_t z1 = in.i32();
    const bool noMath = in.i32() != 0;
    gDepths.clear();
    for (int32_t n = in.i32(), i = 0; i < n; ++i) gDepths.push_back(in.u32());
    gAnswers.clear();
    for (int32_t n = in.i32(), i = 0; i < n; ++i) {
        gAnswers.push_back(in.u32());
        gAnswers.push_back(in.u32());
    }
    gThresholdOutside = in.i32();
    gThresholdPartial = in.i32();

    unsigned char objectBefore[sizeof(CHField)];
    std::memcpy(objectBefore, static_cast<const void*>(&field), sizeof field);
    unsigned char recordBefore[sizeof(sHField)];
    std::memcpy(recordBefore, static_cast<const void*>(&gRecord), sizeof gRecord);

    gei_MathUnit = noMath ? nullptr : &gMath;
    gAssertionThrows = noMath;
    bool stopped = false;
    try {
        field.TestConQuad(x0, z0, x1, z1);
    } catch (const AssertionRaised&) {
        stopped = true;
    }
    gAssertionThrows = false;
    gei_MathUnit = &gMath;

    put(stopped ? 1 : 0);
    put(CBits(field.mF1C));
    put(field.mF20);
    put(field.mF24);
    for (int8_t low : rows->mLow) put(low);
    for (int8_t high : rows->mHigh) put(high);
    // The rest of the object: everything but the far plane and the rows' block.
    unsigned char objectAfter[sizeof(CHField)];
    std::memcpy(objectAfter, static_cast<const void*>(&field), sizeof field);
    const size_t farPlane = reinterpret_cast<unsigned char*>(&field.mF1C) - gFieldStorage;
    const size_t rowsAt = reinterpret_cast<unsigned char*>(rows) - gFieldStorage;
    std::memset(objectBefore + farPlane, 0, 4);
    std::memset(objectAfter + farPlane, 0, 4);
    std::memset(objectBefore + rowsAt, 0, sizeof(SGamutQuads));
    std::memset(objectAfter + rowsAt, 0, sizeof(SGamutQuads));
    put(std::memcmp(objectBefore, objectAfter, sizeof objectBefore) == 0 ? 1 : 0);
    put(std::memcmp(recordBefore, static_cast<const void*>(&gRecord), sizeof gRecord) == 0 ? 1 : 0);
    return gCount;
}

// One CycleFrequencyTable call: the display's frame count, the index and the table before; puts the
// index and the 16 weights afterwards.
void tpw_fr_cycle(int32_t frame, uint32_t index, const int32_t* table, int64_t* out) {
    gei_Display.mFrameCount = frame;
    gFrequencyIndex = index;
    for (int32_t i = 0; i < 16; ++i) gFrequencyTable[i] = table[i];
    CycleFrequencyTable();
    out[0] = gFrequencyIndex;
    for (int32_t i = 0; i < 16; ++i) out[1 + i] = gFrequencyTable[i];
}

}  // extern "C"

// ================================================================================================
// The cache functions on a world
// ================================================================================================
namespace {

constexpr int32_t kMaxLinks = 96;
constexpr int32_t kMaxEntries = 480;
constexpr int32_t kPlaces = 4;
constexpr int32_t kSizes = CTextureCache::kSizes;

alignas(16) unsigned char gCacheStore[sizeof(CTextureCache) + 16];
CTextureCache* gCache;
CTextureLink gLinks[kMaxLinks];
COpenGLTexture gTextures[kMaxLinks];
CTextureBase* gBases[kMaxLinks];   // each texture as the links and the fakes hold it
const void* gWhole[kMaxLinks];     // each texture as an entry holds it (the complete COpenGLTexture)
CTextureEntry gEntries[kMaxEntries];
STexturePlace gPlaces[kPlaces];
CTextureEntry* gArray[kMaxEntries];
int32_t gLinkCount, gEntryCount, gArrayCount;

// What the fakes answer or do.
int32_t gPixels[kMaxLinks];            // GetPixelCount's answer per page
int32_t gWrapAnswer[kMaxEntries];      // StoreAsWrapping's answer per entry (the first call)
int32_t gWrapLater[kMaxEntries];       // and for the later calls
int32_t gWrapCalls[kMaxEntries];       // calls so far
int32_t gCompressCalls;                // Entry_Specific_Compress calls so far
std::vector<int64_t> gCompressEffects; // 8 numbers a call: (assign, value) for mF18C4, mF18E0, mFlags, mF18E4; the last call's repeat
int32_t gClearMode;                    // BalanceCache_ClearTexture: 0 only records, 1 also empties the page

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
int32_t BaseNo(const CTextureBase* base) {
    if (base == nullptr) return 0;
    for (int32_t i = 0; i < kMaxLinks; ++i) {
        if (gBases[i] == base) return i + 1;
    }
    return -1;
}
int32_t WholeNo(const void* whole) {
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
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
CTextureBase::~CTextureBase() {}

int32_t CTextureEntry::StoreAsWrapping() {
    const int32_t number = EntryNo(this);
    ev("StoreAsWrapping", {number});
    if (number <= 0) return 0;
    return gWrapCalls[number - 1]++ == 0 ? gWrapAnswer[number - 1] : gWrapLater[number - 1];
}

// SetFlags records the texture and the flags and does what the original does (all flags but 0x10000000
// replaced).
void CTextureBase::SetFlags(int32_t flags) {
    ev("SetFlags", {BaseNo(this), flags});
    mFlags = static_cast<uint32_t>(flags) | (mFlags & kKeptBySetFlags);
}

// The cache's untranslated callees: Entry_Specific_Compress records the array and the counters it sees and
// may assign mF18C4 (so the loop that calls it ends where the case wants); ClearTexture records the page
// and in mode 1 empties it; the two sorts record.
void CTextureCache::Entry_Specific_Compress(CTextureEntry** entries, int32_t count, int32_t level, int32_t available) {
    ev("Entry_Specific_Compress", {count, HashEntries(entries, count), EntryNo(count > 0 ? entries[0] : nullptr), level,
                                   available, mF18C4, mF18E0, static_cast<int64_t>(mFlags)});
    if (!gCompressEffects.empty()) {
        const size_t calls = gCompressEffects.size() / 8;
        const size_t at = static_cast<size_t>(gCompressCalls) < calls ? static_cast<size_t>(gCompressCalls) : calls - 1;
        const int64_t* effect = &gCompressEffects[8 * at];
        if (effect[0] != 0) mF18C4 = static_cast<int32_t>(effect[1]);
        if (effect[2] != 0) mF18E0 = static_cast<int32_t>(effect[3]);
        if (effect[4] != 0) mFlags = static_cast<uint32_t>(effect[5]);
        if (effect[6] != 0) mF18E4 = static_cast<int32_t>(effect[7]);
    }
    ++gCompressCalls;
}

void CTextureCache::BalanceCache_ClearTexture(CTextureLink* link) {
    ev("ClearTexture", {LinkNo(link)});
    if (gClearMode == 1) {
        link->mEntries = nullptr;
        link->mF0C = 0;
        link->mF1C = 0;
    }
}

void CTextureCache::SortTextures() { ev("SortTextures"); }
void CTextureCache::SortEntries() { ev("SortEntries"); }

// The OpenGL page: only GetPixelCount records (through the page's number).
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
    const int32_t number = BaseNo(this);
    ev("GetPixelCount", {number});
    return number > 0 ? gPixels[number - 1] : 0;
}

namespace {

CTextureEntry* EntryAt(int32_t index) { return index > 0 ? &gEntries[index - 1] : nullptr; }
CTextureLink* LinkAt(int32_t index) { return index > 0 ? &gLinks[index - 1] : nullptr; }

void ReadPlace(Reader& in, STexturePlace* place) {
    for (uint8_t& byte : place->mF00) byte = static_cast<uint8_t>(in.next());
    place->mF10 = in.i32();
    place->mF14 = in.i32();
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

}  // namespace

extern "C" {

// Builds the world from the description, runs operation `op` with arguments a[0..1] and puts the state
// afterwards (as numbers) in out; returns the operation's answer. The caller has reset the call log.
//   1 CalcUsage(link a0, flags a1)           2 AutoMipmap(array, count a0)   3 Entry_Compress(array, count a0)
//   4 BalanceCache_FreeLockedTextureSpace()  5 BalanceCache_LevelSurfaceUsage() (void; the original leaves 1 in r3)
int64_t tpw_fr_run(int32_t op, const int64_t* a, const int64_t* description, int64_t* out, int32_t capacity) {
    Reader in{description, 0};
    gOutData = out;
    gCapacity = capacity;
    gCount = 0;
    gLinkCount = in.i32();
    gEntryCount = in.i32();
    gArrayCount = in.i32();

    // The globals and the fakes' behaviour.
    gei_Display.mFrameCount = in.i32();
    gei_System.mRenderer = in.i32();
    gClearMode = in.i32();
    gCompressEffects.clear();
    for (int32_t n = in.i32(), i = 0; i < n; ++i) {
        for (int32_t k = 0; k < 8; ++k) gCompressEffects.push_back(in.next());
    }
    gCompressCalls = 0;

    // The cache.
    gCache = reinterpret_cast<CTextureCache*>(gCacheStore);
    std::memset(static_cast<void*>(gCache), 0xa5, sizeof(CTextureCache));
    gCache->mFlags = in.u32();
    gCache->mF18E0 = in.i32();
    gCache->mF18E4 = in.i32();
    gCache->mF18BC = in.i32();
    gCache->mF18C0 = in.i32();
    gCache->mF18C4 = in.i32();
    gCache->mF1860 = in.i32();
    for (int32_t& value : gCache->mF1818) value = in.i32();
    for (int32_t& count : gCache->mPageCount) count = in.i32();
    for (int32_t size = 0; size < kSizes; ++size) {
        for (CTextureLink*& page : gCache->mPages[size]) page = nullptr;
        for (int32_t k = 0; k < gCache->mPageCount[size]; ++k) gCache->mPages[size][k] = LinkAt(in.i32());
    }

    // The pages.
    for (int32_t i = 0; i < gLinkCount; ++i) {
        CTextureLink& link = gLinks[i];
        gBases[i] = &gTextures[i];
        link.mTexture = gBases[i];
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
        gPixels[i] = in.i32();
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
        gWrapLater[i] = in.i32();
        gWrapCalls[i] = 0;
        const int32_t place = in.i32();
        entry.mPlace = place > 0 ? &gPlaces[place - 1] : &entry.mOwnPlace;
        ReadPlace(in, &entry.mOwnPlace);
    }
    for (int32_t i = 0; i < gArrayCount; ++i) gArray[i] = EntryAt(in.i32());

    // The call.
    int64_t result = 0;
    switch (op) {
    case 1: gLinks[a[0] - 1].CalcUsage(static_cast<int32_t>(a[1])); break;
    case 2: gCache->AutoMipmap(gArray, static_cast<int32_t>(a[0])); break;
    case 3: gCache->Entry_Compress(gArray, static_cast<int32_t>(a[0])); break;
    case 4: gCache->BalanceCache_FreeLockedTextureSpace(); break;
    case 5:
        gCache->BalanceCache_LevelSurfaceUsage();
        result = 1;   // what the original leaves in r3 (the test compares the PowerPC side's)
        break;
    }

    // The state, in the order test_FrameRest.py reads the original's.
    put(gCache->mFlags);
    put(gCache->mF18E0);
    put(gCache->mF18E4);
    put(gCache->mF18BC);
    put(gCache->mF18C0);
    put(gCache->mF18C4);
    put(gCache->mF1860);
    for (int32_t value : gCache->mF1818) put(value);
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
        put(WholeNo(reinterpret_cast<const void*>(e.mTexture)));
        put(CBits(e.mUSize));
        put(CBits(e.mVSize));
        put(CBits(e.mUOffset));
        put(CBits(e.mVOffset));
        put(CBits(e.mPriority));
        put(e.mPitch);
        put(e.mSortKey);
        PutPlace(&e.mOwnPlace);
    }
    for (int32_t i = 0; i < gArrayCount; ++i) put(EntryNo(gArray[i]));
    return result;
}

// How many numbers the last run wanted to put (more than the capacity it was given if the test's buffer was
// too small).
int32_t tpw_fr_count() { return gCount; }

}  // extern "C"

// ================================================================================================
// The render memory's deferred drawing
// ================================================================================================
namespace {

constexpr int32_t kMaxRecords = 96;
constexpr int32_t kTextures = 24;

alignas(CRenderMemory) unsigned char gMemoryStorage[sizeof(CRenderMemory)];
CRenderMemory& Memory() { return *reinterpret_cast<CRenderMemory*>(gMemoryStorage); }
SRenderLoadInfo gInfos[kMaxRecords];
char gTexture[kTextures + 1];   // a texture is only an address: the later the label, the higher it lies

// What the fakes do: the records ExecutePrimitiveIfInstant marks as drawn, ExternExecute's answers (the last
// repeats; 1 when there are none), whether malloc fails, and the block malloc made last.
std::vector<int32_t> gInstantDrawn;
std::vector<int32_t> gExecuteAnswers;
int32_t gExecuteCalls;
bool gMallocFails;
void* gLastBlock;
uint32_t gLastBytes;

SPrimitive* Prim(int32_t number) { return reinterpret_cast<SPrimitive*>(&gInfos[number]); }
int32_t RecordNo(const SPrimitive* primitive) {
    const SRenderLoadInfo* record = reinterpret_cast<const SRenderLoadInfo*>(primitive);
    if (record >= gInfos && record < gInfos + kMaxRecords) return static_cast<int32_t>(record - gInfos);
    return -1;
}
int32_t TextureNo(const void* texture) {
    if (texture == nullptr) return 0;
    const char* at = static_cast<const char*>(texture);
    if (at >= gTexture && at <= gTexture + kTextures) return static_cast<int32_t>(at - gTexture);
    return -1;
}
int64_t RecordHash(const uint32_t* words, uint32_t count) {
    uint64_t hash = 0;
    for (uint32_t i = 0; i < count; ++i) hash = (hash * 1000003u + words[i]) & ((1ull << 60) - 1);
    return static_cast<int64_t>(hash);
}

// The record as the fakes log it: number, flags, blending mode, texture, depth.
void LogRecord(const char* name, const void* object, const SPrimitive* primitive) {
    ev(name, {lab(object), RecordNo(primitive), primitive->mFlags, primitive->mBlendingMode, TextureNo(primitive->mTexture),
              CBits(primitive->mDepth)});
}

void* FakeMalloc(uint32_t bytes) {
    ev("Malloc", {bytes});
    if (gMallocFails) return nullptr;
    gLastBytes = bytes;
    gLastBlock = std::malloc(bytes);
    std::memset(gLastBlock, 0xa5, bytes);
    return gLastBlock;
}

// Records the block's size and a hash of what it holds (the sorted list) and clears the caller's pointer.
void FakeFree(void*& block) {
    const bool theBlock = block == gLastBlock;
    ev("Free", {theBlock ? 1 : 0, gLastBytes / 4, theBlock ? RecordHash(static_cast<const uint32_t*>(block), gLastBytes / 4) : 0});
    if (theBlock) std::free(block);
    block = nullptr;
}

// The renderer: only the two calls the unit makes are fakes; the pure members are never called.
class FakeDraw : public CDrawPrimitive {
public:
    int32_t Init() override { return 1; }
    int32_t SetBlendingMode(int32_t) override { return 1; }
    int32_t SetTexture(int32_t, void*) override { return 1; }
    int32_t Execute(SPrimitive*) override { return 1; }
    int32_t ExecuteShadow(SPrimitive*) override { return 1; }
    int32_t SetFogParameters(float, float, uint32_t) override { return 1; }
    int32_t BeginDraw() override { return 1; }
    int32_t EndDraw() override { return 1; }
    int32_t DrawTestPoly() override { return 1; }
    int32_t DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) override { return 1; }
};
FakeDraw gDraw;

}  // namespace

CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
TbMallocFn gMalloc = FakeMalloc;
TbFreeFn gFree = FakeFree;
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }

// Records the record; the ones the case names come back drawn (flag 0x10).
int32_t CDrawPrimitive::ExecutePrimitiveIfInstant(SPrimitive* primitive) {
    LogRecord("Instant", this, primitive);
    for (int32_t number : gInstantDrawn) {
        if (number == RecordNo(primitive)) primitive->mFlags |= SPrimitive::kDrawn;
    }
    return 1;
}

// Records the record and answers from the case's list.
int32_t CDrawPrimitive::ExternExecute(SPrimitive* primitive) {
    LogRecord("ExternExecute", this, primitive);
    int32_t answer = 1;
    if (!gExecuteAnswers.empty()) {
        const size_t at = static_cast<size_t>(gExecuteCalls) < gExecuteAnswers.size() ? static_cast<size_t>(gExecuteCalls) : gExecuteAnswers.size() - 1;
        answer = gExecuteAnswers[at];
    }
    ++gExecuteCalls;
    return answer;
}

extern "C" {

// Addresses the log labels: the draw primitive object.
void* tpw_fr_draw_pointer() { return gei_DrawPrimitive; }

// One call on a table of primitives. The description: the number of records and the table's own counts
// (mInfoCount, mNextDynamic, mNextStatic), the renderer, the display's flags, whether malloc fails, the records
// ExecutePrimitiveIfInstant marks as drawn (count, numbers), ExternExecute's answers (count, values), then per record
// the flags, the blending mode, the texture (0: null, else a label 1..24 whose address grows with it) and the depth
// (float bits); the list of record numbers (count, numbers) and the range low, high for the sorts.
//   op 1 ExecuteUnrenderedPrimitives()   2 qSortPrimitivesAscending_Texture   3 qSortPrimitivesAscending_ZPos
//   4 qSortPrimitivesDescending_ZPos     (2 to 4 on the list and the range)
// out: the answer (1 for the sorts), the three counts, the records (flags, blending mode, texture, depth, whether the
// rest of the record is unchanged), the list. Returns the number of numbers put.
int32_t tpw_fr_render(int32_t op, const int64_t* description, int64_t* out, int32_t capacity) {
    Reader in{description, 0};
    gOutData = out;
    gCapacity = capacity;
    gCount = 0;
    const int32_t records = in.i32();
    CRenderMemory& memory = Memory();
    std::memset(static_cast<void*>(&memory), 0xa5, sizeof memory);
    memory.mInfos = gInfos;
    memory.mInfoCount = in.i32();
    memory.mNextDynamic = in.i32();
    memory.mNextStatic = in.i32();
    gei_System.mRenderer = in.i32();
    gei_Display.mFlags = in.u32();
    gMallocFails = in.i32() != 0;
    gInstantDrawn.clear();
    for (int32_t n = in.i32(), i = 0; i < n; ++i) gInstantDrawn.push_back(in.i32());
    gExecuteAnswers.clear();
    for (int32_t n = in.i32(), i = 0; i < n; ++i) gExecuteAnswers.push_back(in.i32());
    gExecuteCalls = 0;
    gLastBlock = nullptr;
    gLastBytes = 0;
    std::memset(static_cast<void*>(gInfos), 0xa5, sizeof gInfos);
    for (int32_t i = 0; i < records; ++i) {
        SPrimitive* primitive = Prim(i);
        primitive->mFlags = in.u32();
        primitive->mBlendingMode = in.u32();
        const int32_t texture = in.i32();
        primitive->mTexture = texture == 0 ? nullptr : static_cast<void*>(gTexture + texture);
        primitive->mDepth = in.f32();
    }
    std::vector<int32_t> list;
    for (int32_t n = in.i32(), i = 0; i < n; ++i) list.push_back(in.i32());
    const int32_t low = in.i32();
    const int32_t high = in.i32();
    // The records before the call, without the flags the fake may set (to see that nothing else changes).
    std::vector<unsigned char> before(sizeof gInfos);
    std::memcpy(before.data(), static_cast<const void*>(gInfos), sizeof gInfos);

    int64_t result = 1;
    switch (op) {
    case 1: result = memory.ExecuteUnrenderedPrimitives(); break;
    case 2: memory.qSortPrimitivesAscending_Texture(list.data(), low, high); break;
    case 3: memory.qSortPrimitivesAscending_ZPos(list.data(), low, high); break;
    case 4: memory.qSortPrimitivesDescending_ZPos(list.data(), low, high); break;
    }

    put(result);
    put(memory.mInfoCount);
    put(memory.mNextDynamic);
    put(memory.mNextStatic);
    for (int32_t i = 0; i < records; ++i) {
        const SPrimitive* primitive = Prim(i);
        put(primitive->mFlags);
        put(primitive->mBlendingMode);
        put(TextureNo(primitive->mTexture));
        put(CBits(primitive->mDepth));
        // Everything but the flags word is as it was.
        unsigned char now[sizeof(SRenderLoadInfo)];
        unsigned char was[sizeof(SRenderLoadInfo)];
        std::memcpy(now, static_cast<const void*>(&gInfos[i]), sizeof now);
        std::memcpy(was, before.data() + static_cast<size_t>(i) * sizeof was, sizeof was);
        std::memset(now, 0, 4);
        std::memset(was, 0, 4);
        put(std::memcmp(now, was, sizeof now) == 0 ? 1 : 0);
    }
    for (int32_t number : list) put(number);
    return gCount;
}

}  // extern "C"
