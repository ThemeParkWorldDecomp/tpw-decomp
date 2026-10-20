// Test entry points for the texture entry and the wavelet texture file loader (library
// unit_TextureEntry: tb_textureentry.cpp, tb_waveload.cpp). A test runs a script of
// operations on an array of two entries and one wave loader; after each operation it
// reads their state as a flat list of numbers, in the order tests/test_TextureEntry.py
// reads the original's. The engine allocator, the file callbacks, the texture cache, the
// wavelet decompressor and the object a compressed entry unpacks from are fakes.
#include <cstdlib>
#include <cstring>
#include <new>

#include "Wavelet.h"
#include "calllog.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "tb_decode_wavelet.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_proctexture.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

// ---- The engine around the unit -------------------------------------------------------------
namespace {
void* FakeMalloc(uint32_t size) {
    ev("Malloc", {size});
    // A size no texture file of the test can need (a header of random bytes asks for up to
    // 4 GB) fails as the real allocator would.
    if (!ret("Malloc", 1) || size > 0x2000) return nullptr;
    void* block = std::calloc(1, size + 16);
    newlab(block);
    return block;
}
void FakeFree(void*& block) {
    const int32_t label = lab(block);
    ev("Free", {label});
    forget(block);
    if (label >= 100) std::free(block);   // only what FakeMalloc gave out: the test's image is static
    block = nullptr;
}

// The file the wave loader reads: the test's bytes.
uint8_t gFile[4096];
uint32_t gFileSize, gFilePosition;
char gFileHandle;
void* FakeOpen(const char* name, const char* mode) {
    ev("Open", {}, {name, mode});
    gFilePosition = 0;
    return ret("Open", 1) ? &gFileHandle : nullptr;
}
uint32_t FakeRead(void* dest, uint32_t size, uint32_t count, void* file) {
    ev("Read", {size, count, lab(file)});
    uint32_t bytes = size * count;
    if (bytes > gFileSize - gFilePosition) bytes = gFileSize - gFilePosition;
    std::memcpy(dest, gFile + gFilePosition, bytes);
    gFilePosition += bytes;
    return size != 0 ? bytes / size : 0;
}
uint32_t FakeClose(void* file) {
    ev("CloseFile", {lab(file)});
    return 0;
}
}  // namespace

TbMallocFn gMalloc = FakeMalloc;
TbFreeFn gFree = FakeFree;
TbFileOpenFn gFileOpen = FakeOpen;
TbFileReadFn gFileRead = FakeRead;
TbFileCloseFn gFileClose = FakeClose;
uint8_t* gProcTextureBuffer = nullptr;
const int32_t gTextureBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};

CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }
CSystem::CSystem() {}
CSystem gei_System;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
alignas(8) char gCacheStore[16];
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("Cache.Remove", {lab(this), lab(entry)});
    return 1;
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
namespace SamsUtilities {
// A little-endian host has the file's words the right way round already.
void _USwapBlock16(uint16_t*, int32_t) {}
void _USwapBlock32(uint32_t*, int32_t) {}
}  // namespace SamsUtilities

// The decompressor: records what it is asked to unpack.
CWavelet::~CWavelet() {}
namespace {
uint32_t fb(float value) { return fbits(value); }
class FakeWavelet : public CWavelet {
public:
    int32_t Decompress(uint32_t, uint32_t, bool, const uint8_t*, uint8_t*, float, float, float) override { return 0; }
    int32_t Decompress(int32_t version, int32_t width, int32_t height, int32_t bytes, void* data, void* alpha,
                       void* dest, float a, float b, float c, float d) override {
        ev("Wavelet.Decompress", {version, width, height, bytes, lab(data), lab(alpha), lab(dest), fb(a), fb(b), fb(c), fb(d)});
        return 1;
    }
    int32_t Decompress(int32_t version, int32_t width, int32_t height, int32_t destWidth, int32_t destHeight,
                       int32_t pitch, int32_t bytes, void* data, void* alpha, void* dest, float a, float b, float c,
                       float d) override {
        ev("Wavelet.Blit", {version, width, height, destWidth, destHeight, pitch, bytes, lab(data), lab(alpha),
                            fb(a), fb(b), fb(c)});
        ev("Wavelet.Blit2", {fb(d), static_cast<int64_t>(static_cast<uint8_t*>(dest) - gProcTextureBuffer)});
        return 1;
    }
    int32_t AddRef() override { return 1; }
    int32_t GetRefCount() override { return 1; }
    int32_t Release() override { return 1; }
};
FakeWavelet gWavelet;

// What a compressed entry unpacks from.
class FakeSource : public CCompBase {
public:
    ~FakeSource() override { ev("Source.dtor", {lab(this)}); }
    int32_t Blit(SUnpackDestination*, int32_t) override { return 0; }
    int32_t Decompress(SUnpackDestination* dest) override {
        ev("Source.Decompress", {lab(this), lab(dest->mData), dest->mWidth, dest->mHeight, dest->mPitch});
        return 1;
    }
    uint32_t AverageColor() override { return 0; }
    int32_t IsNaturallyFlipped() override {
        ev("Source.Flipped", {lab(this)});
        return static_cast<int32_t>(ret("Flipped", 0));
    }
};

enum { kEntries = 2, kPixels = 0x400 };
alignas(16) unsigned char gEntryStore[sizeof(CTextureEntry) * kEntries];
CTextureEntry* gEntries = reinterpret_cast<CTextureEntry*>(gEntryStore);
alignas(16) unsigned char gWaveStore[sizeof(CWaveLoad)];
CWaveLoad* gWave = reinterpret_cast<CWaveLoad*>(gWaveStore);
bool gWaveBuilt;
uint8_t gScratch[kPixels];       // the engine's unpack buffer (label 4)
uint8_t gPixels[kPixels];        // an uncompressed image (label 5)
char gPage, gTexture, gNext;     // labels 6, 7, 8
int32_t gKind[kEntries];         // what each entry holds: 0 nothing, 1 the image, 2 an engine block, 3 a source

// The test keeps an entry's flags in step with what it holds (the compressed flag only
// with a source object) and never marks the last entry as having a further plane.
void Reconcile(int32_t index) {
    CTextureEntry& entry = gEntries[index];
    if (gKind[index] == 3) entry.mFlags |= CTextureEntry::kCompressed;
    else entry.mFlags &= ~static_cast<uint32_t>(CTextureEntry::kCompressed);
    if (index == kEntries - 1) entry.mFlags &= ~static_cast<uint32_t>(CTextureEntry::kNextIsPlane);
}
bool SizeFits(const CTextureEntry& entry) {
    return entry.mWidth >= 1 && entry.mWidth <= 16 && entry.mHeight >= 1 && entry.mHeight <= 16 &&
           entry.mHeight <= 2 * entry.mWidth;
}

int64_t* gOut;
int32_t gCount;
void put(int64_t value) { gOut[gCount++] = value; }
}  // namespace
CWavelet* gWaveLoadWavelet = &gWavelet;

extern "C" {

void* tpw_te_address(int32_t which) {
    switch (which) {
    case 1: return &gEntries[0];
    case 2: return &gEntries[1];
    case 3: return gCacheStore;
    case 4: return gScratch;
    case 5: return gPixels;
    case 6: return &gPage;
    case 7: return &gTexture;
    case 8: return &gNext;
    case 9: return &gFileHandle;
    default: return &gWavelet;
    }
}

// Recording must be off. Both entries and the wave loader become blocks of 0xa5, the
// image the test's pattern, the file the test's bytes.
void tpw_te_reset(const uint8_t* file, uint32_t fileSize, uint32_t seed) {
    std::memset(gEntryStore, 0xa5, sizeof gEntryStore);
    std::memset(gWaveStore, 0xa5, sizeof gWaveStore);
    gWaveBuilt = false;
    gFileSize = fileSize < sizeof gFile ? fileSize : sizeof gFile;
    std::memcpy(gFile, file, gFileSize);
    for (uint32_t i = 0; i < kPixels; ++i) {
        gPixels[i] = static_cast<uint8_t>(seed + i * 7);
        gScratch[i] = static_cast<uint8_t>(seed * 3 + i * 5);
    }
    gProcTextureBuffer = gScratch;
    gKind[0] = gKind[1] = 0;
    gei_System.mFlags = 0;
    gei_System.mRenderer = 3;
    gei_TextureSettings.mFormat = 4;
}

// One operation; `a` are its integer arguments. Entry operations take the entry's index
// first.
int64_t tpw_te_op(int32_t op, const int32_t* a) {
    const int32_t index = a[0] & 1;
    CTextureEntry* entry = &gEntries[index];
    switch (op) {
    case 1:
        new (entry) CTextureEntry;
        gKind[index] = 0;
        return 0;
    case 2: {
        const int32_t result = entry->Create(a[1], a[2]);
        gKind[index] = result ? 2 : 0;
        Reconcile(index);   // Create keeps the flags of whatever the entry was before
        return result;
    }
    case 3: {
        const bool both = index == 0 && (entry->mFlags & CTextureEntry::kNextIsPlane) != 0;
        const int32_t result = entry->Close();
        gKind[index] = 0;
        if (both) gKind[1] = 0;
        return result;
    }
    case 4: return entry->GetBlendingMode();
    case 5: return entry->BestSize(a[1]);
    case 6: return entry->GetBestSize();
    case 7:
        if (gKind[index] == 3 && !SizeFits(*entry)) return -8;
        return lab(entry->GetData());
    case 8:   // only over the test's image or the unpack buffer, and only a size that fits them
        if ((gKind[index] != 1 && gKind[index] != 3) || !SizeFits(*entry)) return -8;
        return lab(entry->GetCorrectedData());
    case 9: entry->SetSize(a[1], a[2]); return 0;
    case 10: entry->FlushEntryFlags(); return 0;
    case 11: entry->Lock(); return 0;
    case 12: entry->Unlock(); return 0;
    case 13: return entry->StoreAsWrapping();
    case 14:
        switch (a[1]) {
        case 0: return fbits(entry->GetVSize());
        case 1: return fbits(entry->GetUSize());
        case 2: return fbits(entry->GetVOffset());
        case 3: return fbits(entry->GetUOffset());
        default: return lab(entry->GetTexture());
        }
    case 15:   // the fields a test sets: flags, size, priority, the four floats
        entry->mFlags = static_cast<uint32_t>(a[1]);
        entry->mWidth = static_cast<int16_t>(a[2]);
        entry->mHeight = static_cast<int16_t>(a[3]);
        entry->mCacheWidth = static_cast<int16_t>(a[4]);
        std::memcpy(&entry->mPriority, &a[5], 4);
        std::memcpy(&entry->mUSize, &a[6], 4);
        std::memcpy(&entry->mVSize, &a[7], 4);
        std::memcpy(&entry->mUOffset, &a[8], 4);
        std::memcpy(&entry->mVOffset, &a[9], 4);
        Reconcile(index);
        return 0;
    case 16:   // what the entry holds: a[1] 0 nothing, 1 the test's image, 2 an engine block, 3 a source object
        entry->mData = a[1] == 1 ? static_cast<void*>(gPixels) : a[1] == 2 ? gMalloc(64) : nullptr;
        if (a[1] == 3) {
            auto* source = new FakeSource;
            newlab(source);
            entry->mData = static_cast<CCompBase*>(source);
        }
        entry->mF38 = a[2] ? gMalloc(32) : nullptr;
        entry->mPage = a[3] ? reinterpret_cast<CTextureLink*>(&gPage) : nullptr;
        entry->mTexture = a[3] ? reinterpret_cast<CTextureBase*>(&gTexture) : nullptr;
        entry->mNext = a[4] ? reinterpret_cast<CTextureEntry*>(&gNext) : nullptr;
        gKind[index] = a[1];
        Reconcile(index);
        return 0;
    case 17:
        gei_System.mFlags = static_cast<uint32_t>(a[1]);
        gei_System.mRenderer = a[2];
        gei_TextureSettings.mFormat = a[3];
        return 0;
    // ---- the wave loader
    case 20: new (gWave) CWaveLoad; gWaveBuilt = true; return 0;
    case 21: {
        char name[32] = "data:image.WCT";
        return gWave->LoadTexture(name);
    }
    case 22: return gWave->Close();
    case 23:
    case 24: {
        SUnpackDestination dest = {a[1] ? gScratch : nullptr, a[2], a[3], a[4]};
        const int32_t result = op == 23 ? gWave->Decompress(&dest) : gWave->Blit(&dest, a[5]);
        return result * 0x1000000 + (dest.mPitch & 0xffff) * 0x100 +
               (dest.mData == nullptr ? 0xff : ((static_cast<uint8_t*>(dest.mData) - gScratch) & 0xff));
    }
    case 25: return gWave->AverageColor();
    case 26: return gWave->IsNaturallyFlipped();
    case 27: gWave->~CWaveLoad(); return 0;
    case 28:   // the header fields a test sets without a file
        gWave->mVersion = static_cast<uint8_t>(a[1]);
        gWave->mWidth = static_cast<uint16_t>(a[2]);
        gWave->mHeight = static_cast<uint16_t>(a[3]);
        for (int i = 0; i < 4; ++i) gWave->mQuality[i] = static_cast<uint16_t>(a[4 + i]);
        gWave->mAverageColour = static_cast<uint32_t>(a[8]);
        return 0;
    }
    return -99;
}

int32_t tpw_te_state(int64_t* out) {
    gOut = out;
    gCount = 0;
    for (int32_t i = 0; i < kEntries; ++i) {
        const CTextureEntry& e = gEntries[i];
        put(e.mPlace == &e.mOwnPlace);
        put(lab(e.mNext));
        put(e.mFlags);
        put(e.mWidth);
        put(e.mHeight);
        put(e.mCacheWidth);
        put(e.mCacheHeight);
        put(lab(e.mPage));
        put(lab(e.mTexture));
        put(fbits(e.mUSize));
        put(fbits(e.mVSize));
        put(fbits(e.mUOffset));
        put(fbits(e.mVOffset));
        put(fbits(e.mPriority));
        put(e.mPitch);
        put(e.mSortKey);
        put(lab(e.mF38));
        put(lab(e.mData));
        for (int32_t w = 0; w < 6; ++w) {
            uint32_t word;
            std::memcpy(&word, reinterpret_cast<const uint8_t*>(&e.mOwnPlace) + 4 * w, 4);
            put(word);
        }
    }
    const CWaveLoad& w = *gWave;
    put(w.mVersion);
    put(w.mAlphaThreshold);
    put(w.mF06);
    put(w.mLevel);
    put(w.mWidth);
    put(w.mHeight);
    for (uint16_t q : w.mQuality) put(q);
    put(w.mDataSize);
    put(w.mAlphaSize);
    put(w.mAverageColour);
    put(gWaveBuilt ? lab(w.mAlphaData) : -7);
    put(gWaveBuilt ? lab(w.mData) : -7);
    // What the loader read: the first bytes of each block.
    for (const uint8_t* block : {gWaveBuilt ? w.mData : nullptr, gWaveBuilt ? w.mAlphaData : nullptr}) {
        uint32_t word = 0;
        if (block != nullptr && lab(block) > 0) std::memcpy(&word, block, 4);
        put(word);
    }
    // The two image buffers, as sums that depend on every byte and its place.
    for (const uint8_t* image : {gPixels, gScratch}) {
        uint64_t sum = 0;
        for (uint32_t i = 0; i < kPixels; ++i) sum = (sum * 31 + image[i]) & 0xffffffffffffull;
        put(static_cast<int64_t>(sum));
    }
    return gCount;
}

}  // extern "C"
