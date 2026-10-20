// Test entry points for the engine's sprite files (library unit_SpriteFileIO:
// tb_spritefileio.cpp with the real texture entry and wavelet loader, tb_textureentry.cpp and
// tb_waveload.cpp). A test runs a script of operations on two sprite file objects, four sprites
// (three in static storage, the fourth made with new), two sprite instances, three sprite
// lists (two static, one made with new), a standalone RLE loader and wavelet loader and a
// standalone array of texture entries; after each operation it reads their state as a flat
// list of numbers, in the order tests/test_SpriteFileIO.py reads the original's.
//
// Fakes that record: the engine allocator, the file callbacks (a file of the test's bytes), the
// texture cache's Remove, the error handler, the assertion, the splash, the link base table,
// the palette converter (UPixels: it also changes the palette bytes, in a way that shows the
// order of the conversions), CSpriteList::Close and the unpacking members of CRLELoad. Every
// operator new gets a label (and the 0xa5 fill the PowerPC side's has), and an array of
// texture entries is also known by the label of its block: an entry is named by that label and
// its index, since the host and the Mac space the entries differently.
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
// A game file has a loader and a block for each of its few hundred pictures: all of them need a label.
#define TPW_CALLLOG_LABELS 4096
#include "calllog.cpp"

#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "sprite_engine.h"
#include "tb_decode_wavelet.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_proctexture.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

// ---- Arrays of texture entries, known by their blocks ---------------------------------------------
namespace {
struct ArrayBlock {
    char* block;
    std::size_t size;
};
ArrayBlock gArrays[512];
int32_t gArrayCount;
bool gQuiet;                 // allocations the unit makes about itself
char* gLastBlock;            // the block the last operator new gave
std::size_t gCookie = 16;    // what new[] puts before the elements (found out by reset: 8 or 16 bytes)

void ObserveAllocation(std::size_t size, bool array, void* block) {
    std::memset(block, 0xa5, size);
    gLastBlock = static_cast<char*>(block);
    if (gQuiet) return;
    newlab(block);
    if (!array) return;
    for (int32_t i = 0; i < gArrayCount; ++i) {
        if (gArrays[i].block == block) {   // an address freed and used again
            gArrays[i].size = size;
            return;
        }
    }
    if (gArrayCount < 512) gArrays[gArrayCount++] = {static_cast<char*>(block), size};
}

// The label of the array whose first element is `first` (its block starts a cookie before it).
int32_t ArrayLabel(const void* first) {
    return first == nullptr ? 0 : lab(static_cast<const char*>(first) - gCookie);
}

// A texture entry is named by the label of the live array that holds it times 100 plus its index.
int32_t EntryLabel(const CTextureEntry* entry) {
    if (entry == nullptr) return 0;
    const char* p = reinterpret_cast<const char*>(entry);
    for (int32_t i = gArrayCount - 1; i >= 0; --i) {
        const ArrayBlock& a = gArrays[i];
        if (lab(a.block) <= 0 || p < a.block + gCookie || p >= a.block + a.size) continue;
        const std::size_t offset = static_cast<std::size_t>(p - (a.block + gCookie));
        if (offset % sizeof(CTextureEntry) != 0) continue;
        return lab(a.block) * 100 + static_cast<int32_t>(offset / sizeof(CTextureEntry));
    }
    return -1;
}
}  // namespace

// ---- The engine around the unit ---------------------------------------------------------------------
namespace {
int64_t gMallocUnit = 1;   // CSpriteList::Init asks for pointers: the event counts them, not bytes
void* FakeMalloc(uint32_t size) {
    ev("Malloc", {static_cast<int64_t>(size) / gMallocUnit});
    // A size no sprite file of the test needs (a header of random bytes asks for up to 4 GB)
    // fails as the real allocator would.
    if (!ret("Malloc", 1) || size > 0x10000) return nullptr;
    void* block = std::calloc(1, size + 16);
    newlab(block);
    return block;
}
void FakeFree(void*& block) {
    const int32_t label = lab(block);
    ev("Free", {label});
    forget(block);
    // Nothing is given back: the PowerPC side never uses an address twice either, and a pointer that outlives its
    // block (a loader's palette) must not name a new block.
    block = nullptr;
}

// The file the loaders read: the test's bytes.
uint8_t gFile[0x200000];
uint32_t gFileSize, gFilePosition;
char gFileHandle;
void* FakeOpen(const char* name, const char* mode) {
    ev("Open", {}, {name, mode});
    return &gFileHandle;
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
CSplash gei_Splash;
CSplash::~CSplash() {}
int32_t CSplash::InterpolateSplash() {
    ev("Splash.Interpolate");
    return 0;
}
alignas(8) char gCacheStore[16];
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
// An entry is named as the state names it.
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("Cache.Remove", {lab(this), EntryLabel(entry)});
    return 1;
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
namespace SamsUtilities {
// A little-endian host has the file's words the right way round already.
void _USwapBlock16(uint16_t*, int32_t) {}
void _USwapBlock32(uint32_t*, int32_t) {}
}  // namespace SamsUtilities

// The decompressor is never asked to unpack here.
CWavelet::~CWavelet() {}
CWavelet* gWaveLoadWavelet = nullptr;

// The link base table.
CMapLinkBase::~CMapLinkBase() {}
int32_t RegisterLinkBase(CMapLinkBase* link) {
    ev("RegisterLinkBase", {lab(dynamic_cast<CSpriteList*>(link))});
    return 1;
}
int32_t FreeLinkBase(CMapLinkBase* link) {
    ev("FreeLinkBase", {lab(dynamic_cast<CSpriteList*>(link))});
    return 1;
}

// The sprite list's members the unit does not translate.
int32_t CSpriteList::Close() {
    ev("SpriteList.Close", {lab(this)});
    return 1;
}
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }

// The RLE loader's: the destructor records itself (the delete that follows is the heap's event).
CRLELoad::~CRLELoad() { ev("RLE.dtor", {lab(this)}); }
int32_t CRLELoad::Close() { return 1; }
int32_t CRLELoad::Blit(SUnpackDestination*, int32_t) { return 0; }
int32_t CRLELoad::Decompress(SUnpackDestination*) { return 0; }
uint32_t CRLELoad::AverageColor() { return 0; }

// The palette converter: constructing records the format and the data, converting records the
// two objects and rewrites the destination's bytes (each plus the format and its position),
// as many as the destination format has (16 bits: 2 a pixel, 32: 4, none for a format the
// converter does not have).
namespace {
struct PixelsInfo {
    const void* object;
    uint8_t* data;
    intptr_t format;
};
PixelsInfo gPixels[8];
PixelsInfo* PixelsOf(const void* object) {
    for (PixelsInfo& info : gPixels)
        if (info.object == object) return &info;
    return nullptr;
}
}  // namespace
namespace SamsUtilities {
UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    ev("Pixels.ctor", {newlab(this), format, lab(data), width, height, stride, lab(palette)});
    for (PixelsInfo& info : gPixels) {
        if (info.object == nullptr) {
            info = {this, static_cast<uint8_t*>(data), format};
            break;
        }
    }
}
UPixels::~UPixels() {
    ev("Pixels.dtor", {lab(this)});
    PixelsInfo* info = PixelsOf(this);
    if (info != nullptr) info->object = nullptr;
    forget(this);
}
void UPixels::Convert(UPixels& source, bool dither) {
    ev("Pixels.Convert", {lab(this), lab(&source), dither ? 1 : 0});
    PixelsInfo* destination = PixelsOf(this);
    if (destination == nullptr || PixelsOf(&source) == nullptr) return;
    const int32_t bytes = destination->format == 4 ? 2 : destination->format == 10 ? 4 : 0;
    for (int32_t i = 0; i < 0x100 * bytes; ++i)
        destination->data[i] = static_cast<uint8_t>(destination->data[i] + destination->format + i);
}
}  // namespace SamsUtilities

// ---- The objects of the test ---------------------------------------------------------------------------
namespace {
enum { kSlots = 4, kLists = 3, kPixelBytes = 0x40 };
alignas(16) unsigned char gSpriteStore[3][sizeof(SSpriteEngineData)];
SSpriteEngineData* gSprites[kSlots];
bool gSpriteBuilt[kSlots];
int32_t gOwn[kSlots];   // what a sprite's texture pointer is: 0 none, 1 an array of its own, 2 somebody else's
int32_t gLen[kSlots];   // how many entries the array of its own holds
alignas(16) unsigned char gGameStore[2][sizeof(SSpriteGameData)];
bool gGameBuilt[2];
alignas(16) unsigned char gListStore[2][sizeof(CSpriteList)];
CSpriteList* gLists[kLists];
bool gListBuilt[kLists];
alignas(16) unsigned char gRleStore[sizeof(CRLELoad)];
CRLELoad* gRle = reinterpret_cast<CRLELoad*>(gRleStore);
bool gRleBuilt;
alignas(16) unsigned char gWaveStore[sizeof(CWaveLoad)];
CWaveLoad* gWave = reinterpret_cast<CWaveLoad*>(gWaveStore);
bool gWaveBuilt;
CSpriteFileIO gIo[2];
CTextureEntry* gE;   // the standalone array
int32_t gECount;
uint8_t gPixelsA[kPixelBytes], gPixelsB[kPixelBytes];   // labels 5 and 6: images for CreateTextureEntry
uint8_t gPalette[0x400];                                // label 13: a palette of the test's
char gPage;                                             // label 7: a page a test entry may sit on

int32_t gHeapBase[4];   // the heap counters when the case began
// The file holds wavelet pictures (kind 2): the Mac reads their headers raw, so the host is given the same
// numbers in the other byte order, and only the operations that read them whole, from a header, mean the same.
bool gWavelet;
int64_t* gOut;
int32_t gCount;
void put(int64_t value) { gOut[gCount++] = value; }

// A running hash of every word, for what is too long to list.
uint64_t gHash;
void hash(int64_t value) { gHash = (gHash ^ static_cast<uint64_t>(value)) * 0x100000001b3ull; }
}  // namespace

// ---- Reading state -----------------------------------------------------------------------------------------
namespace {
// Where a texture entry's data comes from: a loader (named by its label, with the kind), the
// pixel block of a sprite (the offset into it) or a block of the test's by label.
void put_loader(const CCompBase* source, bool hashing);

void put_entry(const CTextureEntry* e, const SSpriteEngineData* owner, bool hashing) {
    auto emit = [&](int64_t value) {
        if (hashing) {
            hash(value);
        } else {
            put(value);
        }
    };
    emit(EntryLabel(e));
    emit(e->mPlace == &e->mOwnPlace);
    emit(lab(e->mNext));
    emit(e->mFlags);
    emit(e->mWidth);
    emit(e->mHeight);
    emit(e->mCacheWidth);
    emit(e->mCacheHeight);
    emit(lab(e->mPage));
    emit(lab(e->mTexture));
    emit(fbits(e->mUSize));
    emit(fbits(e->mVSize));
    emit(fbits(e->mUOffset));
    emit(fbits(e->mVOffset));
    emit(fbits(e->mPriority));
    emit(e->mPitch);
    emit(e->mSortKey);
    emit(lab(e->mF38));
    emit(static_cast<uint32_t>(e->mF40));
    for (int32_t w = 0; w < 6; ++w) {
        uint32_t word;
        std::memcpy(&word, reinterpret_cast<const uint8_t*>(&e->mOwnPlace) + 4 * w, 4);
        emit(word);
    }
    if ((e->mFlags & CTextureEntry::kCompressed) != 0) {
        emit(-3);
        if (hashing) {
            put_loader(static_cast<const CCompBase*>(e->mData), true);
        } else {
            put_loader(static_cast<const CCompBase*>(e->mData), false);
        }
    } else if (e->mData == nullptr) {
        emit(0);
    } else if (lab(e->mData) > 0) {
        emit(lab(e->mData));
    } else if (owner != nullptr && owner->mData != nullptr && e->mData >= owner->mData &&
               static_cast<const char*>(e->mData) < static_cast<const char*>(owner->mData) + 0x100) {
        emit(1000 + (static_cast<const char*>(e->mData) - static_cast<const char*>(owner->mData)));
    } else {
        emit(-1);
    }
}

void put_loader(const CCompBase* source, bool hashing) {
    auto emit = [&](int64_t value) {
        if (hashing) {
            hash(value);
        } else {
            put(value);
        }
    };
    if (source == nullptr) {
        emit(-4);
        return;
    }
    if (const CRLELoad* rle = dynamic_cast<const CRLELoad*>(source)) {
        emit(1);
        emit(lab(rle));
        emit(lab(rle->mData));
        emit(lab(rle->mPalette));
        emit(rle->mWidth);
        emit(rle->mHeight);
        // The first bytes of what it read.
        uint32_t word = 0;
        if (rle->mData != nullptr && lab(rle->mData) > 0) std::memcpy(&word, rle->mData, 4);
        emit(word);
    } else if (const CWaveLoad* w = dynamic_cast<const CWaveLoad*>(source)) {
        emit(2);
        emit(lab(w));
        emit(w->mVersion);
        emit(w->mAlphaThreshold);
        emit(w->mF06);
        emit(w->mLevel);
        emit(w->mWidth);
        emit(w->mHeight);
        for (uint16_t q : w->mQuality) emit(q);
        emit(w->mDataSize);
        emit(w->mAlphaSize);
        emit(w->mAverageColour);
        emit(lab(w->mAlphaData));
        emit(lab(w->mData));
        for (const uint8_t* block : {w->mData, w->mAlphaData}) {
            uint32_t word = 0;
            if (block != nullptr && lab(block) > 0) std::memcpy(&word, block, 4);
            emit(word);
        }
    } else {
        emit(-5);
    }
}

void put_sprite(const SSpriteEngineData* s, bool hashing) {
    auto emit = [&](int64_t value) {
        if (hashing) {
            hash(value);
        } else {
            put(value);
        }
    };
    emit(EntryLabel(s->mTexture));
    emit(lab(s->mData));
    emit(fbits(s->mAspect));
    emit(s->mWidth);
    emit(s->mHeight);
    emit(s->mUsedWidth);
    emit(s->mUsedHeight);
    emit(s->mOffsetX);
    emit(s->mPartCount);   // the same halfword as the y offset, unsigned
}

int32_t SlotOf(const SSpriteEngineData* s) {
    if (s == nullptr) return -1;
    for (int32_t i = 0; i < kSlots; ++i)
        if (gSprites[i] == s) return i;
    return -2;
}
}  // namespace

extern "C" {

void* tpw_sf_address(int32_t which) {
    switch (which) {
    case 3: return gCacheStore;
    case 5: return gPixelsA;
    case 6: return gPixelsB;
    case 7: return &gPage;
    case 9: return &gFileHandle;
    case 11: return gRleStore;
    case 12: return gWaveStore;
    case 13: return gPalette;
    case 31: return gListStore[0];
    case 32: return gListStore[1];
    default: return nullptr;
    }
}

// Recording must be off. Everything becomes blocks of 0xa5 or zeros, the file the test's bytes.
void tpw_sf_reset(const uint8_t* file, uint32_t fileSize, int32_t wavelet) {
    gWavelet = wavelet != 0;
    {
        // How far from its block the first element of a new[] array is, for both kinds of array.
        gQuiet = true;
        CTextureEntry* probe = new CTextureEntry[1];
        gCookie = static_cast<std::size_t>(reinterpret_cast<char*>(probe) - gLastBlock);
        delete[] probe;
        SSpriteEngineData* sprites = new SSpriteEngineData[1];
        if (static_cast<std::size_t>(reinterpret_cast<char*>(sprites) - gLastBlock) != gCookie) std::abort();
        delete[] sprites;
        gQuiet = false;
    }
    std::memset(gSpriteStore, 0xa5, sizeof gSpriteStore);
    std::memset(gGameStore, 0xa5, sizeof gGameStore);
    std::memset(gListStore, 0xa5, sizeof gListStore);
    std::memset(gRleStore, 0xa5, sizeof gRleStore);
    std::memset(gWaveStore, 0xa5, sizeof gWaveStore);
    std::memset(gIo, 0, sizeof gIo);
    std::memset(&gSpriteWaveHeader, 0, sizeof gSpriteWaveHeader);
    for (int32_t i = 0; i < 3; ++i) gSprites[i] = reinterpret_cast<SSpriteEngineData*>(gSpriteStore[i]);
    gSprites[3] = nullptr;
    for (int32_t i = 0; i < kSlots; ++i) {
        gSpriteBuilt[i] = false;
        gOwn[i] = 0;
        gLen[i] = 0;
    }
    gGameBuilt[0] = gGameBuilt[1] = false;
    gLists[0] = reinterpret_cast<CSpriteList*>(gListStore[0]);
    gLists[1] = reinterpret_cast<CSpriteList*>(gListStore[1]);
    gLists[2] = nullptr;
    gListBuilt[0] = gListBuilt[1] = gListBuilt[2] = false;
    gRleBuilt = gWaveBuilt = false;
    gE = nullptr;
    gECount = 0;
    gArrayCount = 0;
    gMallocUnit = 1;
    for (int32_t i = 0; i < 4; ++i) gHeapBase[i] = tpw_cl_heap[i];
    gFileSize = fileSize < sizeof gFile ? fileSize : sizeof gFile;
    std::memcpy(gFile, file, gFileSize);
    gFilePosition = 0;
    for (uint32_t i = 0; i < kPixelBytes; ++i) {
        gPixelsA[i] = static_cast<uint8_t>(i * 7 + 1);
        gPixelsB[i] = static_cast<uint8_t>(i * 5 + 3);
    }
    for (uint32_t i = 0; i < sizeof gPalette; ++i) gPalette[i] = static_cast<uint8_t>(i * 3 + 9);
    gei_System.mFlags = 0;
    gei_TextureSettings.mFormat = 4;
    std::memset(gPixels, 0, sizeof gPixels);
}

namespace {
// What an operation's arguments name.
CTextureEntry* EntryArgument(int32_t kind) {
    if (kind == 1 && gE != nullptr && gECount >= 2) return &gE[0];
    if (kind == 2 && gE != nullptr && gECount >= 2) return &gE[1];
    return nullptr;
}
bool EntryArgumentOk(int32_t kind) { return kind == 0 || EntryArgument(kind) != nullptr; }

void* ImageArgument(int32_t kind) { return kind == 1 ? gPixelsA : kind == 2 ? gPixelsB : nullptr; }

// Whether a sprite's partial textures can be freed: the loop (when it will run) must walk an
// array of its own, as many as the count says.
bool SafeToFree(int32_t slot, bool needsTexture) {
    SSpriteEngineData* s = gSprites[slot];
    if (s->mPartCount == 0 || (needsTexture && s->mTexture == nullptr)) return true;
    return gOwn[slot] == 1 && s->mPartCount <= gLen[slot] && s->mTexture != nullptr;
}
}  // namespace

// One operation; `a` are its integer arguments.
int64_t tpw_sf_op(int32_t op, const int32_t* a) {
    const int32_t k = a[0] & 1;
    CSpriteFileIO* io = &gIo[k];
    const int32_t slot = a[0] & 3;
    switch (op) {
    case 1: std::memset(io, 0, sizeof *io); return 0;
    case 2:
        // A file is read from its start into an object that holds nothing: a word that comes up short is swapped
        // again by the Mac and not by the host, which is only the same for zero.
        if (io->mCount != 0 || gFilePosition != 0) return -8;
        return CSpriteFileIO::Load(io, &gFileHandle);
    case 3: return io->Free();
    case 4:
        if (io->mCount == 0 || io->mSprites == nullptr) return -8;
        for (int32_t i = 0; i < io->mCount; ++i)
            if (io->mSprites[i].mTexture == nullptr) return -8;
        io->FlagAsSemitransparent();
        return 0;
    case 5:
        if (gWavelet && a[1] > 0 && a[1] != 8) return -8;
        gFilePosition = a[1] >= 0 && static_cast<uint32_t>(a[1]) < gFileSize ? static_cast<uint32_t>(a[1]) : gFileSize;
        return 0;
    case 7:
        gei_System.mFlags = static_cast<uint32_t>(a[1]);
        gei_TextureSettings.mFormat = a[2];
        return 0;
    // ---- sprites
    case 10:
        if (slot == 3) {
            gSprites[3] = new SSpriteEngineData;
        } else {
            new (gSprites[slot]) SSpriteEngineData;
        }
        gSpriteBuilt[slot] = true;
        gOwn[slot] = 0;
        gLen[slot] = 0;
        return 0;
    case 11:
        if (!gSpriteBuilt[slot] || gOwn[slot] == 2 || !SafeToFree(slot, true)) return -8;
        if (slot == 3) {
            delete gSprites[3];
            gSprites[3] = nullptr;
            gSpriteBuilt[3] = false;
        } else {
            gSprites[slot]->~SSpriteEngineData();
        }
        gOwn[slot] = 0;
        gLen[slot] = 0;
        return 0;
    case 12: {   // CreateTextureEntry: image, alpha, source, width, height, entry, flags
        if (!gSpriteBuilt[slot] || !EntryArgumentOk(a[6])) return -8;
        CCompBase* source = a[3] == 1 && gRleBuilt ? static_cast<CCompBase*>(gRle)
                            : a[3] == 2 && gWaveBuilt ? static_cast<CCompBase*>(gWave) : nullptr;
        if (a[3] != 0 && (source == nullptr || a[6] == 0)) return -8;   // a standalone loader is never closed
        CTextureEntry* entry = EntryArgument(a[6]);
        // An entry that unpacks from a loader says so.
        const int32_t flags = source != nullptr ? (a[7] | 0x10000000) : a[7];
        const int32_t result = gSprites[slot]->CreateTextureEntry(ImageArgument(a[1]), ImageArgument(a[2]), source, a[4],
                                                                  a[5], entry, flags);
        gOwn[slot] = entry != nullptr ? 2 : 1;
        gLen[slot] = 1;
        return result;
    }
    case 13:
    case 14: {   // Load_WCT (entry kind a[1]); Load_RLE (palette a[1], entry kind a[2])
        const int32_t entryKind = op == 13 ? a[1] : a[2];
        // The header is in the file whole: a short read leaves earlier bytes (of the other byte order) or stack.
        if (!gSpriteBuilt[slot] || !EntryArgumentOk(entryKind) || gFilePosition + (op == 13 ? 0x2c : 20) > gFileSize) return -8;
        if (op == 13 ? (!gWavelet || gFilePosition < 8) : gWavelet) return -8;
        CTextureEntry* entry = EntryArgument(entryKind);
        int32_t result;
        if (op == 13) {
            result = gSprites[slot]->Load_WCT(&gFileHandle, entry);
        } else {
            void* palette = a[1] == 1 ? static_cast<void*>(gIo[0].mPalette) : a[1] == 2 ? static_cast<void*>(gPalette) : nullptr;
            result = gSprites[slot]->Load_RLE(&gFileHandle, palette, entry);
        }
        gOwn[slot] = entry != nullptr ? 2 : 1;
        gLen[slot] = 1;
        return result;
    }
    case 15:
        if (!gSpriteBuilt[slot]) return -8;
        return gSprites[slot]->Init(a[1], a[2]);
    case 16:
        if (!gSpriteBuilt[slot] || !SafeToFree(slot, true)) return -8;
        if (gSprites[slot]->mTexture != nullptr && gSprites[slot]->mPartCount != 0) {
            gOwn[slot] = 0;
            gLen[slot] = 0;
        }
        return gSprites[slot]->RefreshPartialTexture();
    case 17:
        if (!gSpriteBuilt[slot] || gOwn[slot] == 2 || !SafeToFree(slot, true)) return -8;
        {
            const int32_t result = gSprites[slot]->Close();
            gOwn[slot] = gSprites[slot]->mTexture != nullptr ? gOwn[slot] : 0;
            return result;
        }
    case 18: {   // a pixel block and a/1] entries of partial textures: a[1] block, a[2] entries, a[3] on-page bits, a[4] count
        if (!gSpriteBuilt[slot]) return -8;
        SSpriteEngineData* s = gSprites[slot];
        s->mData = a[1] != 0 ? gMalloc(0x100) : nullptr;
        const int32_t n = a[2];
        s->mTexture = n > 0 ? new CTextureEntry[static_cast<std::size_t>(n)] : nullptr;
        for (int32_t i = 0; i < n; ++i) {
            CTextureEntry& e = s->mTexture[i];
            e.mFlags = 0x5180200u | (((a[3] >> i) & 1) != 0 ? CTextureEntry::kOnPage : 0u);
            e.SetSize(8 + i, 4 + i);
            e.mPitch = 64;
            e.mData = s->mData != nullptr ? static_cast<char*>(s->mData) + 4 * i : nullptr;
        }
        s->mPartCount = static_cast<uint16_t>(a[4] < 0 ? n : a[4]);
        gOwn[slot] = n > 0 ? 1 : 0;
        gLen[slot] = n > 0 ? n : 0;
        return 0;
    }
    case 19:   // forget the texture pointer of a sprite that does not own it
        if (!gSpriteBuilt[slot]) return -8;
        gSprites[slot]->mTexture = nullptr;
        gOwn[slot] = 0;
        gLen[slot] = 0;
        return 0;
    // ---- sprite instances
    case 20:
        new (gGameStore[k]) SSpriteGameData;
        gGameBuilt[k] = true;
        return 0;
    case 21:
    case 22: {
        if (!gGameBuilt[k]) return -8;
        SSpriteGameData* g = reinterpret_cast<SSpriteGameData*>(gGameStore[k]);
        SSpriteEngineData* s = g->mSprite;
        const int32_t sl = SlotOf(s);
        // The loop that frees the partial textures, when the change will run it, needs a sprite of the test's.
        if (s != nullptr && (sl < 0 || !gSpriteBuilt[sl] || !SafeToFree(sl, op == 21))) return -8;
        const bool frees = s != nullptr && (op == 21 ? (g->mUsedX != a[1] || g->mUsedY != a[2])
                                                     : (g->mUsedWidth != a[1] || g->mUsedWidth != a[2])) &&
                           s->mPartCount != 0 && (op == 22 || s->mTexture != nullptr);
        if (op == 21) {
            g->SetUsedPosition(a[1], a[2]);
        } else {
            g->SetUsedSize(a[1], a[2]);
        }
        if (frees) {
            gOwn[sl] = 0;
            gLen[sl] = 0;
        }
        return 0;
    }
    case 23: {   // the instance's sprite: a[1] a slot, or none
        if (!gGameBuilt[k]) return -8;
        SSpriteGameData* g = reinterpret_cast<SSpriteGameData*>(gGameStore[k]);
        g->mSprite = a[1] >= 0 && gSpriteBuilt[a[1] & 3] ? gSprites[a[1] & 3] : nullptr;
        return 0;
    }
    // ---- sprite lists
    case 30: {
        const int32_t l = a[0] % 3;
        if (l == 2) {
            gLists[2] = new CSpriteList;
        } else {
            new (gLists[l]) CSpriteList;
        }
        gListBuilt[l] = true;
        return 0;
    }
    case 31: {
        const int32_t l = a[0] % 3;
        if (!gListBuilt[l]) return -8;
        if (l == 2) {
            delete gLists[2];
            gLists[2] = nullptr;
        } else {
            gLists[l]->~CSpriteList();
        }
        gListBuilt[l] = false;
        return 0;
    }
    case 32: {
        const int32_t l = a[0] % 3;
        if (!gListBuilt[l]) return -8;
        gMallocUnit = sizeof(void*);
        SSpriteGameData** result = gLists[l]->Init(a[1]);
        gMallocUnit = 1;
        return lab(result);
    }
    // ---- the RLE loader
    case 40: new (gRleStore) CRLELoad; gRleBuilt = true; return 0;
    case 41:
        if (!gRleBuilt) return -8;
        gRle->SetSize(a[1], a[2]);
        return 0;
    case 42:
        if (!gRleBuilt) return -8;
        gRle->SetPalette(a[1] == 1 ? static_cast<void*>(gPalette) : a[1] == 2 ? static_cast<void*>(gPixelsA) : nullptr);
        return 0;
    case 43:
        if (!gRleBuilt || gWavelet) return -8;
        return gRle->LoadSprite(&gFileHandle, a[1]);
    // ---- the wavelet loader
    case 50: new (gWaveStore) CWaveLoad; gWaveBuilt = true; return 0;
    case 51:
        if (!gWaveBuilt || !gWavelet || gFilePosition < 8 || gFilePosition + 0x2c > gFileSize) return -8;
        return gWave->LoadSprite(&gFileHandle);
    case 52:
        if (!gWaveBuilt) return -8;
        gWave->~CWaveLoad();
        return 0;
    // ---- the standalone entries
    case 60:
        gE = a[1] > 0 ? new CTextureEntry[static_cast<std::size_t>(a[1])] : nullptr;
        gECount = a[1] > 0 ? a[1] : 0;
        return 0;
    }
    return -99;
}

int32_t tpw_sf_state(int64_t* out) {
    gOut = out;
    gCount = 0;
    for (int32_t i = 0; i < 4; ++i) put(tpw_cl_heap[i] - gHeapBase[i]);
    // ---- the sprite files
    for (int32_t k = 0; k < 2; ++k) {
        const CSpriteFileIO& io = gIo[k];
        put(io.mCount);
        put(ArrayLabel(io.mSprites));
        put(ArrayLabel(io.mTextures));
        put(lab(io.mPalette));
        uint64_t palette = 0;
        if (io.mPalette != nullptr && lab(io.mPalette) > 0) {
            for (int32_t i = 0; i < 0x400; ++i) palette = (palette * 31 + static_cast<const uint8_t*>(io.mPalette)[i]) & 0xffffffffffffull;
        }
        put(static_cast<int64_t>(palette));
        gHash = 0xcbf29ce484222325ull;
        const int32_t shown = io.mSprites != nullptr && io.mTextures != nullptr && io.mCount > 0 ? io.mCount : 0;
        for (int32_t i = 0; i < shown; ++i) {
            put_sprite(&io.mSprites[i], i >= 6);
            put_entry(&io.mTextures[i], nullptr, i >= 6);
        }
        put(static_cast<int64_t>(gHash & 0xffffffffffffull));
    }
    // ---- the sprites
    for (int32_t s = 0; s < kSlots; ++s) {
        if (!gSpriteBuilt[s]) {
            put(-7);
            continue;
        }
        put(1);
        put_sprite(gSprites[s], false);
        const CTextureEntry* texture = gSprites[s]->mTexture;
        const int32_t shown = texture == nullptr ? 0 : gOwn[s] == 1 ? (gLen[s] < 4 ? gLen[s] : 4) : 1;
        put(shown);
        for (int32_t i = 0; i < shown; ++i) put_entry(&texture[i], gSprites[s], false);
    }
    // ---- the instances
    for (int32_t g = 0; g < 2; ++g) {
        if (!gGameBuilt[g]) {
            put(-7);
            continue;
        }
        const SSpriteGameData* d = reinterpret_cast<const SSpriteGameData*>(gGameStore[g]);
        put(static_cast<uint32_t>(d->mF10));
        put(d->mF24);
        put(SlotOf(d->mSprite));
        put(fbits(d->mF2C));
        put(d->mUsedWidth);
        put(d->mUsedHeight);
        put(d->mUsedX);
        put(d->mUsedY);
    }
    // ---- the lists
    for (int32_t l = 0; l < kLists; ++l) {
        if (!gListBuilt[l]) {
            put(-7);
            continue;
        }
        const CSpriteList& list = *gLists[l];
        put(list.mCount);
        put(list.mCapacity);
        put(list.mFrameA);
        put(list.mFrameB);
        put(lab(list.mInstances));
        put(list.mF18);
    }
    // ---- the loaders
    if (gRleBuilt) {
        put(lab(gRle->mData));
        put(lab(gRle->mPalette));
        put(gRle->mWidth);
        put(gRle->mHeight);
    } else {
        put(-7);
    }
    if (gWaveBuilt) {
        put_loader(gWave, false);
    } else {
        put(-7);
    }
    // ---- the header the wavelet reader leaves behind
    const SSpriteWaveHeader& h = gSpriteWaveHeader;
    put(h.mF00);
    put(h.mWidth);
    put(h.mHeight);
    put(h.mUsedWidth);
    put(h.mUsedHeight);
    put(h.mOffsetX);
    put(h.mOffsetY);
    for (float q : h.mQuality) put(fbits(q));
    put(h.mDataSize);
    put(h.mAlphaSize);
    // ---- the standalone entries and the test's blocks
    for (int32_t i = 0; i < gECount; ++i) put_entry(&gE[i], nullptr, false);
    for (const uint8_t* block : {gPixelsA, gPixelsB}) {
        uint64_t sum = 0;
        for (uint32_t i = 0; i < kPixelBytes; ++i) sum = (sum * 31 + block[i]) & 0xffffffffffffull;
        put(static_cast<int64_t>(sum));
    }
    uint64_t palette = 0;
    for (uint32_t i = 0; i < sizeof gPalette; ++i) palette = (palette * 31 + gPalette[i]) & 0xffffffffffffull;
    put(static_cast<int64_t>(palette));
    return gCount;
}

}  // extern "C"
