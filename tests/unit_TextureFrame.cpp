// Test entry point for the texture cache's frame calls (library unit_TextureFrame:
// src/tb_texturecache_frame2.cpp): CTextureCache::Prepare, UploadToVRAM, GenerateTexturePages,
// ReInitialize, DumpTCStreamToLog_Prepare and _Execute, and CTextureLink::PageUpdated.
//
// A case is one call on a small world that tests/test_TextureFrame.py describes as a flat list
// of numbers (the order is World below): the cache, two pages (a link and a texture each),
// four entries in one array, eight place records (four of their own, four the entries carry),
// a quad and the engine's globals. The call's answer and the state of everything it could
// change are returned as another flat list, in the order the test reads the original's.
//
// Everything outside the unit is a fake that records its arguments: the texture's Download,
// GetTexture and UnlockSurface, the cache's Close, AutoCreateSurfaces and PackTextures, the
// assertion handler and the C library's file calls (the debug stream's fopen, fwrite, fflush
// and fclose, overridden in this library).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/msl.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

int32_t LabelOfLow32(uint32_t pointer);   // the label of the world's entry at that address, else -1

namespace {
constexpr int32_t kLinks = 2;
constexpr int32_t kEntries = 4;
constexpr int32_t kPlaces = 8;   // 0..3 their own, 4..7 the entries' own records

// Labels of the world's objects (the test uses the same numbers).
enum : int32_t {
    kLabelCache = 1,
    kLabelFile = 5,
    kLabelTexture = 11,   // + page
    kLabelLink = 21,      // + page
    kLabelEntry = 31,     // + entry
    kLabelPlace = 41,     // + place
    kLabelQuad = 51,
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
CDisplay::CDisplay() {}
CDisplay gei_Display;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
uint32_t gFrequencyIndex = 0;   // defined in src/tb_texturecache_frame.cpp (another unit)
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}

void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

// The cache's other members the frame calls.
int32_t CTextureCache::Close() {
    ev("Close", {lab(this)});
    return static_cast<int32_t>(ret("Close", 0));
}
int32_t CTextureCache::AutoCreateSurfaces() {
    ev("AutoCreateSurfaces", {lab(this)});
    return static_cast<int32_t>(ret("AutoCreateSurfaces", 0));
}

// PackTextures takes entries off the front of the list it is given and answers a number. Each
// call uses the next value of the "Pack" queue: the entries to take (the low byte) and the
// answer plus 8 above it.
int32_t CTextureCache::PackTextures(CTextureEntry** list) {
    int32_t length = 0;
    int64_t labels[12];
    for (CTextureEntry* entry = *list; entry != nullptr && length < 12; entry = entry->mNext) labels[length++] = lab(entry);
    ev("PackTextures", {lab(this), list == &mF18AC, length});
    ev("PackList", {length > 0 ? labels[0] : 0, length > 1 ? labels[1] : 0, length > 2 ? labels[2] : 0,
                    length > 3 ? labels[3] : 0, length > 4 ? labels[4] : 0, length > 5 ? labels[5] : 0,
                    length > 6 ? labels[6] : 0, length > 7 ? labels[7] : 0, length > 8 ? labels[8] : 0,
                    length > 9 ? labels[9] : 0, length > 10 ? labels[10] : 0, length > 11 ? labels[11] : 0});
    const int64_t action = ret("Pack", 0);
    for (int64_t taken = action & 0xff; taken > 0 && *list != nullptr; --taken) *list = (*list)->mNext;
    return static_cast<int32_t>((action >> 8) - 8);
}

// The OpenGL page: only the three virtual members the frame calls record.
CTextureBase::~CTextureBase() {}
COpenGLTexture::COpenGLTexture() {}
COpenGLTexture::~COpenGLTexture() {}
double COpenGLTexture::TestUploadSpeed() { return 0.0; }
int32_t COpenGLTexture::Create(int32_t, int32_t, int32_t, int32_t) { return 0; }
int32_t COpenGLTexture::Destroy() { return 0; }
int32_t COpenGLTexture::LockSurface() { return 0; }
int32_t COpenGLTexture::MipmapsUsed() { return 0; }
int32_t COpenGLTexture::GetPixelCount() { return 0; }
int32_t COpenGLTexture::Download(CTextureEntry* entry, int32_t x, int32_t y) {
    ev("Download", {lab(this), lab(entry), x, y});
    return static_cast<int32_t>(ret("Download", 0));
}
void COpenGLTexture::GetTexture() { ev("GetTexture", {lab(this)}); }
int32_t COpenGLTexture::UnlockSurface() {
    ev("UnlockSurface", {lab(this)});
    return static_cast<int32_t>(ret("UnlockSurface", 0));
}

namespace {
alignas(8) char gFileHandle[8];   // what fopen hands out (label 5)

// The bytes of a write as the test compares them: a byte, the stream's 16-byte record (the
// entry's address as its label, flags, size, priority) or 32-bit words, eight to an event.
// The words are read in the host's byte order, the PowerPC side reads them in its own.
void RecordData(const uint8_t* data, size_t bytes) {
    if (bytes == 1) {
        ev("fbyte", {data[0]});
    } else if (bytes == 0x10) {
        uint32_t flags, pointer, priority;
        int16_t width, height;
        std::memcpy(&pointer, data, 4);
        std::memcpy(&flags, data + 4, 4);
        std::memcpy(&width, data + 8, 2);
        std::memcpy(&height, data + 10, 2);
        std::memcpy(&priority, data + 12, 4);
        // The entry's address is written as its low 32 bits: find the entry by them.
        const int32_t label = pointer == 0 ? 0 : LabelOfLow32(pointer);
        ev("frecord", {label, flags, width, height, canonical(priority)});
    } else {
        for (size_t first = 0; first < bytes; first += 32) {
            int64_t words[8] = {};
            const size_t count = (bytes - first < 32 ? bytes - first : 32) / 4;
            for (size_t i = 0; i < count; ++i) {
                uint32_t word;
                std::memcpy(&word, data + first + 4 * i, 4);
                words[i] = word;
            }
            ev("fwords", {static_cast<int64_t>(count), words[0], words[1], words[2], words[3], words[4], words[5], words[6],
                          words[7]});
        }
    }
}
}  // namespace

// The C library's file calls, overridden for this library only.
extern "C" {
FILE* fopen(const char* name, const char* mode) {
    ev("fopen", {}, {name, mode});
    return ret("fopen", 1) != 0 ? reinterpret_cast<FILE*>(gFileHandle) : nullptr;
}
size_t fwrite(const void* data, size_t size, size_t count, FILE* file) {
    ev("fwrite", {static_cast<int64_t>(size), static_cast<int64_t>(count), lab(file)});
    RecordData(static_cast<const uint8_t*>(data), size * count);
    return count;
}
int fflush(FILE* file) {
    ev("fflush", {lab(file)});
    return 0;
}
int fclose(FILE* file) {
    ev("fclose", {lab(file)});
    return 0;
}
}

// ---- The world --------------------------------------------------------------------------------
namespace {
alignas(16) unsigned char gCacheStore[sizeof(CTextureCache) + 16];
CTextureCache* gCache;
CTextureLink gLinks[kLinks];
COpenGLTexture gTextures[kLinks];
CTextureEntry gEntries[kEntries];
STexturePlace gPlaces[kPlaces - kEntries];
SAvailableQuad gQuad;
char gSecondBlock[4];   // what an entry's mF38 points at, when it points at something

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

STexturePlace* PlaceAt(int32_t index, int32_t entry) { return index > 0 ? &gPlaces[index - 1] : &gEntries[entry].mOwnPlace; }

// Entry i's place records are 0..3 for separate ones and 4 + i for its own.
void ReadPlace(Reader& in, STexturePlace* place) {
    for (uint8_t& byte : place->mF00) byte = static_cast<uint8_t>(in.next());
    place->mF10 = in.i32();
    place->mF14 = in.i32();
}

int64_t* gOut;
int32_t gCount, gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOut[gCount] = value;
    ++gCount;
}

void PutPlace(const STexturePlace* place) {
    for (uint8_t byte : place->mF00) put(byte);
    put(place->mF10);
    put(place->mF14);
}
}  // namespace

int32_t LabelOfLow32(uint32_t pointer) {
    for (int32_t i = 0; i < kEntries; ++i) {
        if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gEntries[i])) == pointer) return kLabelEntry + i;
    }
    return -1;
}

extern "C" {

// Builds the world from the description, runs operation `op` with arguments a[0..3] and puts
// the state afterwards (as numbers) in out; returns the operation's answer. The caller has
// reset the call log and set its queues.
//   1 Prepare(entry, priority bits)        2 UploadToVRAM(link, quad, entry)
//   3 GenerateTexturePages()               4 DumpTCStreamToLog_Prepare(entry)
//   5 DumpTCStreamToLog_Execute()          6 PageUpdated(link)
//   7 ReInitialize(format)
// An index of an entry is 1..4 (0: null); a link's is 1 or 2.
int64_t tpw_tf_run(int32_t op, const int64_t* a, const int64_t* description, int64_t* out, int32_t capacity) {
    Reader in{description, 0};
    gOut = out;
    gCapacity = capacity;
    gCount = 0;

    // Globals.
    gei_System.mFlags = in.u32();
    gei_System.mRenderer = in.i32();
    uint32_t settings[20];
    for (uint32_t& word : settings) word = in.u32();
    std::memcpy(static_cast<void*>(&gei_TextureSettings), settings, sizeof settings);
    gei_Display.mFlags = in.u32();
    gei_Display.mFrameCount = in.i32();
    gFrequencyIndex = in.u32();
    gTexturePageUpdates = in.i32();

    // The cache.
    gCache = reinterpret_cast<CTextureCache*>(gCacheStore);
    std::memset(static_cast<void*>(gCache), 0xa5, sizeof(CTextureCache));
    gCache->mF18AC = EntryAt(in.i32());
    gCache->mFlags = in.u32();
    gCache->mSizeMask = in.i32();
    gCache->mF18E8 = in.f32();
    for (int32_t& value : gCache->mF1830) value = in.i32();
    gCache->mF1860 = in.i32();
    gCache->mF18B0 = in.i32();
    gCache->mF18B8 = in.i32();
    gCache->mF18BC = in.i32();
    gCache->mMipmaps = in.i32();
    gCache->mLargestSize = in.i32();
    gCache->mUITextureSize = in.i32();
    gCache->mF18DC = in.i32();
    gCache->mF18E0 = in.i32();
    gCache->mF1908 = in.i32();
    for (int32_t& count : gCache->mPageCount) count = in.i32();

    // The pages.
    for (int32_t i = 0; i < kLinks; ++i) {
        CTextureLink& link = gLinks[i];
        link.mTexture = &gTextures[i];
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
    }

    // The place records, then the entries.
    for (STexturePlace& place : gPlaces) ReadPlace(in, &place);
    for (int32_t i = 0; i < kEntries; ++i) {
        CTextureEntry& entry = gEntries[i];
        entry.mFlags = in.u32();
        entry.mWidth = static_cast<int16_t>(in.i32());
        entry.mHeight = static_cast<int16_t>(in.i32());
        entry.mCacheWidth = static_cast<int16_t>(in.i32());
        entry.mCacheHeight = static_cast<int16_t>(in.i32());
        const int32_t page = in.i32();
        entry.mPage = page > 0 ? &gLinks[page - 1] : nullptr;
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
        entry.mF38 = in.i32() != 0 ? gSecondBlock : nullptr;
        entry.mData = nullptr;
        const int32_t place = in.i32();
        entry.mPlace = PlaceAt(place, i);
        ReadPlace(in, &entry.mOwnPlace);
    }
    gQuad.mX = in.i32();
    gQuad.mY = in.i32();
    gQuad.mWidth = in.i32();
    gQuad.mHeight = in.i32();
    gQuad.mEntry = EntryAt(in.i32());

    // The labels.
    tpw_cl_label(kLabelCache, gCache);
    tpw_cl_label(kLabelFile, gFileHandle);
    tpw_cl_label(kLabelQuad, &gQuad);
    for (int32_t i = 0; i < kLinks; ++i) {
        tpw_cl_label(kLabelTexture + i, dynamic_cast<void*>(&gTextures[i]));
        tpw_cl_label(kLabelLink + i, &gLinks[i]);
    }
    for (int32_t i = 0; i < kEntries; ++i) {
        tpw_cl_label(kLabelEntry + i, &gEntries[i]);
        tpw_cl_label(kLabelPlace + i, &gPlaces[i]);
        tpw_cl_label(kLabelPlace + kEntries + i, &gEntries[i].mOwnPlace);
    }

    // The call.
    int64_t result = 0;
    switch (op) {
    case 1: gCache->Prepare(EntryAt(static_cast<int32_t>(a[0])), from_bits(static_cast<uint32_t>(a[1]))); break;
    case 2:
        result = gCache->UploadToVRAM(a[0] > 0 ? &gLinks[a[0] - 1] : nullptr, a[1] != 0 ? &gQuad : nullptr, EntryAt(static_cast<int32_t>(a[2])));
        break;
    case 3: result = gCache->GenerateTexturePages(); break;
    case 4: gCache->DumpTCStreamToLog_Prepare(EntryAt(static_cast<int32_t>(a[0]))); break;
    case 5: gCache->DumpTCStreamToLog_Execute(); break;
    case 6: result = gLinks[a[0] - 1].PageUpdated(); break;
    case 7: result = gCache->ReInitialize(static_cast<int32_t>(a[0])); break;
    }

    // The state, in the order test_TextureFrame.py reads the original's.
    put(gTexturePageUpdates);
    put(gFrequencyIndex);
    put(gei_TextureSettings.mFormat);
    put(gei_TextureSettings.mFlags);
    put(gei_System.mFlags);
    put(lab(gCache->mF18AC));
    for (int32_t value : gCache->mF1830) put(value);
    put(gCache->mF1860);
    put(gCache->mF18B0);
    put(gCache->mFlags);
    put(gCache->mF18B8);
    put(gCache->mF18BC);
    put(gCache->mSizeMask);
    put(gCache->mMipmaps);
    put(gCache->mLargestSize);
    put(gCache->mUITextureSize);
    put(gCache->mF18DC);
    put(gCache->mF18E0);
    put(cbits(gCache->mF18E8));
    put(gCache->mF1908);
    for (int32_t count : gCache->mPageCount) put(count);
    for (int32_t i = 0; i < kLinks; ++i) {
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
    for (int32_t i = 0; i < kEntries; ++i) {
        const CTextureEntry& e = gEntries[i];
        put(lab(e.mPlace));
        put(lab(e.mNext));
        put(e.mFlags);
        put(e.mWidth);
        put(e.mHeight);
        put(e.mCacheWidth);
        put(e.mCacheHeight);
        put(lab(e.mPage));
        put(lab(e.mTexture));
        put(cbits(e.mUSize));
        put(cbits(e.mVSize));
        put(cbits(e.mUOffset));
        put(cbits(e.mVOffset));
        put(cbits(e.mPriority));
        put(e.mPitch);
        put(e.mSortKey);
        put(e.mF38 != nullptr);
        PutPlace(&e.mOwnPlace);
    }
    for (const STexturePlace& place : gPlaces) PutPlace(&place);
    put(gQuad.mX);
    put(gQuad.mY);
    put(gQuad.mWidth);
    put(gQuad.mHeight);
    put(lab(gQuad.mEntry));
    return result;
}

// How many numbers the last run wanted to put (more than the capacity it was given if the
// test's buffer was too small).
int32_t tpw_tf_count() { return gCount; }
}
