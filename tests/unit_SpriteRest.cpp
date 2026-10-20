// Test entry points for the sprite classes' remaining members (library unit_SpriteRest: tb_rleload.cpp and
// tb_spritelist_rest.cpp with the file reader of tb_spritefileio.cpp and the real texture entry and wavelet
// loader, tb_textureentry.cpp and tb_waveload.cpp). Two kinds of test run on it:
//
//  * a script of operations on two sprite file objects, four sprites (three in static storage, the fourth made with
//    new), three sprite lists (two static, one made with new), a standalone RLE loader (static or made with new)
//    and a standalone array of texture entries; after each operation the test reads their state as a flat list of
//    numbers, in the order tests/test_SpriteRest.py reads the original's (tpw_sr_reset, tpw_sr_op, tpw_sr_state);
//  * the RLE unpacking on its own: a loader, a palette and a destination built from the case's numbers
//    (tpw_rd_setup), then Decompress, Blit or AverageColor (tpw_rd_call), and the destination's bytes and the
//    loader's words read back (tpw_rd_read).
//
// Fakes that record: the engine allocator, the file callbacks (a file of the test's bytes), the texture cache's
// Remove, the error handler, the assertion, the splash, the link base table, the palette converter, the sprite
// manager's AddList and the TGA loader. Every operator new gets a label (and the 0xa5 fill the PowerPC side's has),
// and an array of texture entries is also known by the label of its block: an entry is named by that label and its
// index, since the host and the Mac space the entries differently.
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
#include "tb_resource_display.h"
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
// A file the test can fail to open: the queue "Open" (1 when it is empty).
void* FakeOpen(const char* name, const char* mode) {
    ev("Open", {}, {name, mode});
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
CSplash gei_Splash;
CSplash::~CSplash() {}
int32_t CSplash::InterpolateSplash() {
    ev("Splash.Interpolate");
    return 0;
}
// The texture cache is memory of its size: only the UI texture size is read, and Remove records.
alignas(16) char gCacheStore[sizeof(CTextureCache)];
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

// The display's frame words (CSpriteList::Prepare reads them) and the sprite manager it adds the list to.
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSpriteManager::CSpriteManager() {}
CSpriteManager::~CSpriteManager() {}
alignas(16) unsigned char gManagerStore[sizeof(CSpriteManager)];
CSpriteManager* gei_SpriteManager = reinterpret_cast<CSpriteManager*>(gManagerStore);
int32_t CSpriteManager::AddList(CSpriteList* list) {
    ev("AddList", {lab(this), lab(list)});
    return static_cast<int32_t>(ret("AddList", 1));
}

// The TGA loader: it answers from the queues "TGA" (1 loads, 0 fails; 1 when empty), "TGAw" and "TGAh" (the size it
// writes, 29 x 28 when empty), and gives a block of the engine's allocator for the pixels.
void* TGALoadAndConvert(char* name, int32_t* width, int32_t* height, int32_t flags, lARGBType* palette, int32_t bytesPerPixel) {
    ev("TGA", {flags, lab(palette), bytesPerPixel}, {name});
    if (!ret("TGA", 1)) return nullptr;
    *width = static_cast<int32_t>(ret("TGAw", 29));
    *height = static_cast<int32_t>(ret("TGAh", 28));
    return gMalloc(16);
}

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
enum { kSlots = 4, kLists = 3, kPixelBytes = 0x40, kNames = 10 };
alignas(16) unsigned char gSpriteStore[3][sizeof(SSpriteEngineData)];
SSpriteEngineData* gSprites[kSlots];
bool gSpriteBuilt[kSlots];
int32_t gOwn[kSlots];   // what a sprite's texture pointer is: 0 none, 1 an array of its own, 2 somebody else's
int32_t gLen[kSlots];   // how many entries the array of its own holds
const void* gBase[kSlots];   // the pixel block the sprite's own entries point into (their data is named by the distance), or none
alignas(16) unsigned char gListStore[2][sizeof(CSpriteList)];
CSpriteList* gLists[kLists];
bool gListBuilt[kLists];
alignas(16) unsigned char gRleStore[sizeof(CRLELoad)];
CRLELoad* gRle;          // the standalone loader: gRleStore, or made with new
bool gRleBuilt;
bool gRleHeap;
bool gRleUsed;           // a standalone entry unpacks from it: it must not be destroyed
CSpriteFileIO gIo[2];
CTextureEntry* gE;   // the standalone array
int32_t gECount;
uint8_t gPixelsA[kPixelBytes], gPixelsB[kPixelBytes];   // labels 5 and 6: images for CreateTextureEntry
uint8_t gPalette[0x400];                                // label 13: a palette of the test's

// The names Init(char*) is given, one block with a zero before each (the name before the empty one is read: the
// last character of "" is the byte in front of it).
const char kNameBlock[] = "X\0SPR_A.TGA\0SPR_A.TGB\0spr_a.tgb\0SPR_AB\0spr_ab\0b\0B\0\0x.tgaB.\0AB.TGAb ";
char gNameBlock[sizeof kNameBlock];
char* gNames[kNames];

int32_t gHeapBase[4];   // the heap counters when the case began
int64_t* gOut;
int32_t gCount;
void put(int64_t value) { gOut[gCount++] = value; }

// A running hash of every word, for what is too long to list.
uint64_t gHash;
void hash(int64_t value) { gHash = (gHash ^ static_cast<uint64_t>(value)) * 0x100000001b3ull; }
}  // namespace

// ---- Reading state -----------------------------------------------------------------------------------------
namespace {
// Where a texture entry's data comes from: a loader (named by its label, with the kind), the pixel block of
// a sprite (the offset into it) or a block of the test's by label.
void put_loader(const CCompBase* source, bool hashing);

void put_entry(const CTextureEntry* e, const void* base, bool hashing) {
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
        put_loader(static_cast<const CCompBase*>(e->mData), hashing);
    } else if (e->mData == nullptr) {
        emit(0);
    } else if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(e->mData)) == 0xa5a5a5a5u) {
        emit(-2);   // never set: the constructor leaves the heap's fill (a negative cache size makes parts the generator does not fill)
    } else if (base != nullptr) {
        // Into the sprite's pixel block (made by the partial texture generator): the distance from the block's start. (A
        // pointer left behind by a freed block is not looked up: the host's allocator can give its address out again.)
        emit(-9);
        emit(static_cast<int32_t>(reinterpret_cast<intptr_t>(e->mData) - reinterpret_cast<intptr_t>(base)));
    } else if (lab(e->mData) > 0) {
        emit(lab(e->mData));
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
}  // namespace

extern "C" {

void* tpw_sr_address(int32_t which) {
    switch (which) {
    case 3: return gCacheStore;
    case 4: return gManagerStore;
    case 5: return gPixelsA;
    case 6: return gPixelsB;
    case 9: return &gFileHandle;
    case 11: return gRleStore;
    case 13: return gPalette;
    case 31: return gListStore[0];
    case 32: return gListStore[1];
    default: return nullptr;
    }
}

// Recording must be off. Everything becomes blocks of 0xa5 or zeros, the file the test's bytes.
void tpw_sr_reset(const uint8_t* file, uint32_t fileSize) {
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
    std::memset(gListStore, 0xa5, sizeof gListStore);
    std::memset(gRleStore, 0xa5, sizeof gRleStore);
    std::memset(gIo, 0, sizeof gIo);
    for (int32_t i = 0; i < 3; ++i) gSprites[i] = reinterpret_cast<SSpriteEngineData*>(gSpriteStore[i]);
    gSprites[3] = nullptr;
    for (int32_t i = 0; i < kSlots; ++i) {
        gSpriteBuilt[i] = false;
        gOwn[i] = 0;
        gLen[i] = 0;
        gBase[i] = nullptr;
    }
    gLists[0] = reinterpret_cast<CSpriteList*>(gListStore[0]);
    gLists[1] = reinterpret_cast<CSpriteList*>(gListStore[1]);
    gLists[2] = nullptr;
    gListBuilt[0] = gListBuilt[1] = gListBuilt[2] = false;
    gRle = reinterpret_cast<CRLELoad*>(gRleStore);
    gRleBuilt = gRleHeap = gRleUsed = false;
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
    std::memcpy(gNameBlock, kNameBlock, sizeof kNameBlock);
    {
        // The names are the pieces of the block between the zeros, after the leading "X".
        char* p = gNameBlock + 2;
        for (int32_t i = 0; i < kNames; ++i) {
            gNames[i] = p;
            p += std::strlen(p) + 1;
        }
    }
    gei_System.mFlags = 0;
    gei_TextureSettings.mFormat = 4;
    gei_Display.mFrameCount = 0;
    gei_Display.mF84 = 0;
    std::memset(gCacheStore, 0, sizeof gCacheStore);
    gei_TCache->mUITextureSize = 32;
    std::memset(gManagerStore, 0, sizeof gManagerStore);
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

// Whether Close can run: the partial textures are safe to free, and a sprite without a pixel block closes the first entry of
// its texture, which an array of none does not have.
bool SafeToClose(int32_t slot) {
    SSpriteEngineData* s = gSprites[slot];
    if (!SafeToFree(slot, true)) return false;
    return !(s->mData == nullptr && s->mTexture != nullptr && gLen[slot] < 1);
}

// How many partial textures GeneratePartialTexture would make for the sprite's used size at the cache's size, or
// -1 when it cannot be done safely (no size, a count the test will not allocate). A negative cache size is nonsense
// to the game but is the way to reach the original's size assertion.
int32_t PartsToMake(const SSpriteEngineData* s) {
    const int32_t size = gei_TCache->mUITextureSize;
    if (size == 0) return -1;
    const uint32_t mask = static_cast<uint32_t>(size) - 1;
    const int32_t columns = static_cast<int32_t>(s->mUsedWidth + mask) / size;
    const int32_t rows = static_cast<int32_t>(s->mUsedHeight + mask) / size;
    const int64_t count = static_cast<int64_t>(columns) * rows;
    if (columns > 64 || rows > 64 || columns < -4096 || rows < -4096 || count < 0 || count > 64) return -1;
    return static_cast<int32_t>(count);
}
}  // namespace

// One operation; `a` are its integer arguments.
int64_t tpw_sr_op(int32_t op, const int32_t* a) {
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
        gFilePosition = a[1] >= 0 && static_cast<uint32_t>(a[1]) < gFileSize ? static_cast<uint32_t>(a[1]) : gFileSize;
        return 0;
    case 7:
        gei_System.mFlags = static_cast<uint32_t>(a[1]);
        gei_TextureSettings.mFormat = a[2];
        return 0;
    case 8:   // Load(name, unused) of a file object that holds nothing, from the start of the file
        if (io->mCount != 0 || gFilePosition != 0) return -8;
        return CSpriteFileIO::Load(io, gNames[a[1] % kNames], a[2]);
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
        if (!gSpriteBuilt[slot] || gOwn[slot] == 2 || !SafeToClose(slot)) return -8;
        if (slot == 3) {
            delete gSprites[3];
            gSprites[3] = nullptr;
        } else {
            gSprites[slot]->~SSpriteEngineData();
        }
        gSpriteBuilt[slot] = false;   // what it pointed at may be gone
        gOwn[slot] = 0;
        gLen[slot] = 0;
        gBase[slot] = nullptr;
        return 0;
    case 12: {   // CreateTextureEntry: image, alpha, source, width, height, entry, flags
        if (!gSpriteBuilt[slot] || !EntryArgumentOk(a[6])) return -8;
        CCompBase* source = a[3] == 1 && gRleBuilt ? static_cast<CCompBase*>(gRle) : nullptr;
        if (a[3] != 0 && (source == nullptr || a[6] == 0)) return -8;   // a standalone loader is never closed
        CTextureEntry* entry = EntryArgument(a[6]);
        // An entry that unpacks from a loader says so.
        const int32_t flags = source != nullptr ? (a[7] | 0x10000000) : a[7];
        const int32_t result = gSprites[slot]->CreateTextureEntry(ImageArgument(a[1]), ImageArgument(a[2]), source, a[4],
                                                                  a[5], entry, flags);
        gOwn[slot] = entry != nullptr ? 2 : 1;
        gLen[slot] = 1;
        gBase[slot] = nullptr;
        if (source != nullptr) gRleUsed = true;
        return result;
    }
    case 14: {   // Load_RLE (palette a[1], entry kind a[2])
        // The header is in the file whole: a short read leaves earlier bytes or stack.
        if (!gSpriteBuilt[slot] || !EntryArgumentOk(a[2]) || gFilePosition + 20 > gFileSize) return -8;
        CTextureEntry* entry = EntryArgument(a[2]);
        void* palette = a[1] == 1 ? static_cast<void*>(gIo[0].mPalette) : a[1] == 2 ? static_cast<void*>(gPalette) : nullptr;
        const int32_t result = gSprites[slot]->Load_RLE(&gFileHandle, palette, entry);
        gOwn[slot] = entry != nullptr ? 2 : 1;
        gLen[slot] = 1;
        gBase[slot] = nullptr;
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
        if (!gSpriteBuilt[slot] || gOwn[slot] == 2 || !SafeToClose(slot)) return -8;
        {
            const int32_t result = gSprites[slot]->Close();
            gOwn[slot] = gSprites[slot]->mTexture != nullptr ? gOwn[slot] : 0;
            return result;
        }
    case 18: {   // a pixel block and n entries of partial textures: a[1] block, a[2] entries, a[3] on-page bits, a[4] count
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
        gBase[slot] = s->mData;
        return 0;
    }
    case 19:   // forget the texture pointer of a sprite that does not own it
        if (!gSpriteBuilt[slot]) return -8;
        gSprites[slot]->mTexture = nullptr;
        gOwn[slot] = 0;
        gLen[slot] = 0;
        return 0;
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
    case 33:
    case 34:
    case 36:
    case 37: {
        const int32_t l = a[0] % 3;
        if (!gListBuilt[l]) return -8;
        if (op == 33) return gLists[l]->Close();
        if (op == 34) return gLists[l]->Prepare();
        if (op == 36) return reinterpret_cast<intptr_t>(gLists[l]->GetData());
        return gLists[l]->GetLinkType();
    }
    case 35:   // the display's frame words
        gei_Display.mFrameCount = a[1];
        gei_Display.mF84 = a[2];
        return 0;
    // ---- the RLE loader
    case 40:
        if (a[1] != 0) {
            gRle = new CRLELoad;
            gRleHeap = true;
        } else {
            gRle = reinterpret_cast<CRLELoad*>(gRleStore);
            new (gRleStore) CRLELoad;
            gRleHeap = false;
        }
        gRleBuilt = true;
        return 0;
    case 41:
        if (!gRleBuilt) return -8;
        gRle->SetSize(a[1], a[2]);
        return 0;
    case 42:
        if (!gRleBuilt) return -8;
        gRle->SetPalette(a[1] == 1 ? static_cast<void*>(gPalette) : a[1] == 2 ? static_cast<void*>(gPixelsA) : nullptr);
        return 0;
    case 43:
        if (!gRleBuilt) return -8;
        return gRle->LoadSprite(&gFileHandle, a[1]);
    case 44:
        if (!gRleBuilt) return -8;
        return gRle->Close();
    case 45:
        if (!gRleBuilt) return -8;
        return gRle->AverageColor();
    case 49:   // the destructor: a[1] 0 flag 0, 1 flag 1 (delete), 2 flag -1; a delete needs a loader made with new
        if (!gRleBuilt || gRleUsed || (a[1] == 1 && !gRleHeap)) return -8;
        if (a[1] == 1) {
            delete gRle;
        } else {
            gRle->~CRLELoad();
        }
        gRleBuilt = false;
        gRle = reinterpret_cast<CRLELoad*>(gRleStore);
        return 0;
    // ---- the sprites' remaining members
    case 70: {   // Init(name)
        if (!gSpriteBuilt[slot]) return -8;
        const int32_t result = gSprites[slot]->Init(gNames[a[1] % kNames]);
        if (result != 0) {
            gOwn[slot] = 1;
            gLen[slot] = 1;
            gBase[slot] = nullptr;
        }
        return result;
    }
    case 71: {   // SetUsedPart(x, y)
        if (!gSpriteBuilt[slot]) return -8;
        SSpriteEngineData* s = gSprites[slot];
        const bool destroys = s->mTexture != nullptr && (s->mUsedWidth != static_cast<int16_t>(a[1]) ||
                                                         s->mUsedHeight != static_cast<int16_t>(a[2]));
        if (destroys && !SafeToFree(slot, true)) return -8;
        const bool frees = destroys && s->mPartCount != 0;
        s->SetUsedPart(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]));
        if (frees) {
            gOwn[slot] = 0;
            gLen[slot] = 0;
        }
        return 0;
    }
    case 72: {   // GeneratePartialTexture(x, y)
        if (!gSpriteBuilt[slot]) return -8;
        SSpriteEngineData* s = gSprites[slot];
        const int32_t parts = PartsToMake(s);
        if (parts < 0 || s->mData == nullptr) return -8;
        const int32_t result = s->GeneratePartialTexture(a[1], a[2]);
        gOwn[slot] = 1;
        gLen[slot] = parts;
        gBase[slot] = s->mData;
        return result;
    }
    case 73: {   // DestroyPartialTexture()
        if (!gSpriteBuilt[slot] || !SafeToFree(slot, false)) return -8;
        SSpriteEngineData* s = gSprites[slot];
        const bool frees = s->mPartCount != 0;
        const int32_t result = s->DestroyPartialTexture();
        if (frees) {
            gOwn[slot] = 0;
            gLen[slot] = 0;
        }
        return result;
    }
    case 76: gei_TCache->mUITextureSize = a[1]; return 0;
    case 78: {   // mark some of a sprite's own entries as placed on a cache page
        if (!gSpriteBuilt[slot] || gOwn[slot] != 1 || gSprites[slot]->mTexture == nullptr) return -8;
        for (int32_t i = 0; i < gLen[slot] && i < 6; ++i)
            if (((a[1] >> i) & 1) != 0) gSprites[slot]->mTexture[i].mFlags |= CTextureEntry::kOnPage;
        return 0;
    }
    // ---- the standalone entries
    case 60:
        gE = a[1] > 0 ? new CTextureEntry[static_cast<std::size_t>(a[1])] : nullptr;
        gECount = a[1] > 0 ? a[1] : 0;
        return 0;
    }
    return -99;
}

int32_t tpw_sr_state(int64_t* out) {
    gOut = out;
    gCount = 0;
    for (int32_t i = 0; i < 4; ++i) put(tpw_cl_heap[i] - gHeapBase[i]);
    // ---- the sprite files
    for (int32_t k = 0; k < 2; ++k) {
        const CSpriteFileIO& io = gIo[k];
        put(-1000 - k);
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
        put(-1010 - s);
        if (!gSpriteBuilt[s]) {
            put(-7);
            continue;
        }
        put(1);
        put_sprite(gSprites[s], false);
        const CTextureEntry* texture = gSprites[s]->mTexture;
        const int32_t shown = texture == nullptr ? 0 : gOwn[s] == 1 ? (gLen[s] < 4 ? gLen[s] : 4) : 1;
        put(shown);
        for (int32_t i = 0; i < shown; ++i) put_entry(&texture[i], gBase[s], false);
    }
    // ---- the lists
    for (int32_t l = 0; l < kLists; ++l) {
        put(-1020 - l);
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
    // ---- the loader
    put(-1030);
    if (gRleBuilt) {
        put(gRleHeap ? 1 : 0);
        put(lab(gRle->mData));
        put(lab(gRle->mPalette));
        put(gRle->mWidth);
        put(gRle->mHeight);
    } else {
        put(-7);
    }
    // ---- the standalone entries and the test's blocks
    put(-1031);
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

// ---- The RLE unpacking on its own ------------------------------------------------------------------------------
namespace {
constexpr int32_t kRdBuffer = 0x10000;   // the destination's room (the PowerPC side allocates the same)
constexpr int32_t kRdData = 0x8000;      // the room for the stream and its zero padding
alignas(16) uint8_t gRdBuffer[kRdBuffer];
alignas(16) uint8_t gRdData[kRdData];
alignas(16) uint8_t gRdPalette[0x400];
alignas(16) unsigned char gRdLoaderStore[sizeof(CRLELoad)];
CRLELoad* gRdLoader = reinterpret_cast<CRLELoad*>(gRdLoaderStore);
SUnpackDestination gRdDest;
}  // namespace

extern "C" {

// Builds the loader (made by its constructor, then given the stream, the palette and the size as the sprite
// loader would), the destination and the texture format. The buffer starts as a pattern (so that what is
// written, and what is not, shows), the stream is followed by zero bytes to the end of its room (what a
// malformed stream reads), and `start` is where the destination's pixels begin in the buffer.
void tpw_rd_setup(int32_t format, int32_t width, int32_t height, const uint8_t* stream, int32_t streamLength,
                  const uint8_t* palette, int32_t destWidth, int32_t destHeight, int32_t pitch, int32_t start, int32_t seed) {
    gei_TextureSettings.mFormat = format;
    new (gRdLoaderStore) CRLELoad;
    std::memset(gRdData, 0, sizeof gRdData);
    std::memcpy(gRdData, stream, static_cast<std::size_t>(streamLength));
    std::memcpy(gRdPalette, palette, sizeof gRdPalette);
    gRdLoader->mData = gRdData;
    gRdLoader->mPalette = gRdPalette;
    gRdLoader->mWidth = width;
    gRdLoader->mHeight = height;
    for (int32_t i = 0; i < kRdBuffer; ++i) gRdBuffer[i] = static_cast<uint8_t>(i * 13 + seed);
    gRdDest.mData = gRdBuffer + start;
    gRdDest.mWidth = destWidth;
    gRdDest.mHeight = destHeight;
    gRdDest.mPitch = pitch;
}

// One call: kind 0 Decompress, 1 Blit(flip), 2 AverageColor. The answer.
int64_t tpw_rd_call(int32_t kind, int32_t flip) {
    if (kind == 0) return gRdLoader->Decompress(&gRdDest);
    if (kind == 1) return gRdLoader->Blit(&gRdDest, flip);
    return gRdLoader->AverageColor();
}

// The destination's buffer, and the loader's words (the pointers as offsets into the stream and the palette).
void tpw_rd_read(uint8_t* out, int64_t* words) {
    std::memcpy(out, gRdBuffer, sizeof gRdBuffer);
    words[0] = gRdLoader->mData == static_cast<void*>(gRdData) ? 1 : gRdLoader->mData == nullptr ? 0 : -1;
    words[1] = gRdLoader->mPalette == static_cast<void*>(gRdPalette) ? 1 : gRdLoader->mPalette == nullptr ? 0 : -1;
    words[2] = gRdLoader->mWidth;
    words[3] = gRdLoader->mHeight;
    words[4] = static_cast<int64_t>(reinterpret_cast<uint8_t*>(gRdDest.mData) - gRdBuffer);
    words[5] = gRdDest.mWidth;
    words[6] = gRdDest.mHeight;
    words[7] = gRdDest.mPitch;
}

}  // extern "C"
