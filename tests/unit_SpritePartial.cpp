// Test entry points for the partial sprite draw and the sprite shadows (library unit_SpritePartial:
// src/tb_sprites_partial.cpp and src/tb_sprites_shadow.cpp, with the real sprite classes' members that make a partial sprite
// (src/tb_spritefileio.cpp, src/tb_spritelist_rest.cpp: Init(int, int), SetUsedPart, GeneratePartialTexture and the
// rest), the texture entry (src/tb_textureentry.cpp), the shadow object (src/tb_shadow.cpp), the sprite manager's frame
// members (src/tb_sprites.cpp: the scripts draw through the real CSpriteManager::Render) and the real math unit
// (src/MatrixPowerPC.cpp: the shadows' two transforms).
//
// tests/test_SpritePartial.py builds one world twice from one description, in emulator memory for the original and
// through tpw_sp_reset for the translation, runs the same script of operations on both (tpw_sp_op) and compares, after every
// operation, the answer and a flat list of numbers that names the state of everything the operation could have changed
// (tpw_sp_state), and at the end the events the fakes recorded.
//
// The world: four sprites with a pixel block (labels 10..13), three plain sprites that point at a standalone texture entry
// (14..16), six sprite instances (20..25), six standalone texture entries (40..45), a sprite file of four sprites
// (label 8, its sprites 50..53), eight texture pages that are markers (61..68), the shadow object gShadow (5), a height
// field (6) with its heights, the final matrix (7), the cache (3), the sprite manager (4) and the draw primitive (1).
//
// Fakes that record: the draw primitive's DrawSprite, AddSprite and ExecuteCurrentPrimitive, the sprite instance members
// that are not in the unit (Prepare, DrawRegular and AddRegular), RotateVertex_Y (turns the corners exactly), the texture
// cache's Prepare and Remove, the engine allocator, the error handler and the assertion. The partial textures
// (CTextureEntry arrays) are named by the block that made them: 1000 + 100 * block + index.
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "Wavelet.h"
#include "MatrixPowerPC.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "sprite_engine.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_shadow.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "m3d2.h"

using namespace calllog;

// ---- The world's sizes (tests/test_SpritePartial.py has the same numbers) -------------------------------------
namespace {
constexpr int32_t kBlocks = 4;        // sprites with a pixel block
constexpr int32_t kPool = 7;          // blocks, then three plain sprites
constexpr int32_t kInstances = 6;
constexpr int32_t kEntries = 6;       // standalone texture entries
constexpr int32_t kPages = 8;         // texture page markers
constexpr int32_t kFileSprites = 4;   // sprites of the frame file

constexpr int32_t kLabelDraw = 1, kLabelMath = 2, kLabelCache = 3, kLabelManager = 4, kLabelShadow = 5, kLabelField = 6,
                  kLabelFinal = 7, kLabelFile = 8, kLabelPool = 10, kLabelInstance = 20, kLabelEntry = 40,
                  kLabelFileSprite = 50, kLabelPage = 60;

// The most the test reads of a partial array.
constexpr int32_t kMostParts = 36;
}  // namespace

// ---- Storage and bookkeeping -------------------------------------------------------------------------------
namespace {
alignas(16) unsigned char gPoolStore[kPool][sizeof(SSpriteEngineData)];
alignas(16) unsigned char gInstanceStore[kInstances][sizeof(SSpriteGameData)];
alignas(16) unsigned char gEntryStore[kEntries][sizeof(CTextureEntry)];
alignas(16) unsigned char gFileSpriteStore[kFileSprites][sizeof(SSpriteEngineData)];
CSpriteFileIO gFile;
char gMarkers[kPages + 1];
alignas(16) unsigned char gHFieldStore[sizeof(CHField)];
sHField gRecord;
CMatrix gFinalMatrix;
alignas(16) char gCacheStore[sizeof(CTextureCache)];
alignas(16) unsigned char gManagerStore[sizeof(CSpriteManager)];
CMatrixPowerPC gMath;

bool gBuilt[kBlocks];        // the block sprite has been constructed
const char* gGenBlock[kBlocks];   // the sprite's pixel block when its partial textures were last made (they look into that one)
bool gRotation[kInstances];  // the instance's word at Mac 0x30 was last set as a float (else as two halfwords)
void* gHeights;              // the arena block of the heights
uint32_t gLightMask;         // what DrawSprite's colour is ANDed with (the light manager's)
int32_t gHeapBase[4];

SSpriteEngineData* Pool(int32_t s) { return reinterpret_cast<SSpriteEngineData*>(gPoolStore[s]); }
SSpriteGameData* Instance(int32_t g) { return reinterpret_cast<SSpriteGameData*>(gInstanceStore[g]); }
CTextureEntry* Entry(int32_t e) { return e >= 0 && e < kEntries ? reinterpret_cast<CTextureEntry*>(gEntryStore[e]) : nullptr; }
SSpriteEngineData* FileSprite(int32_t i) { return reinterpret_cast<SSpriteEngineData*>(gFileSpriteStore[i]); }
CHField* Field() { return reinterpret_cast<CHField*>(gHFieldStore); }
CTextureCache* Cache() { return reinterpret_cast<CTextureCache*>(gCacheStore); }
CSpriteManager* Manager() { return reinterpret_cast<CSpriteManager*>(gManagerStore); }
char* Marker(int32_t page) { return page >= 1 && page <= kPages ? &gMarkers[page] : nullptr; }

// A NaN (as float bits) as one value: its sign and payload depend on the processor.
uint32_t Canon(uint32_t word) { return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word; }
uint32_t CanonBits(float value) { return Canon(fbits(value)); }
float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

int32_t PageLabel(const void* page) {
    if (page == nullptr) return 0;
    for (int32_t p = 1; p <= kPages; ++p)
        if (page == &gMarkers[p]) return kLabelPage + p;
    return -1;
}

// A texture entry is named by where it is: a standalone one by its number, a partial texture by its block and index.
int32_t EntryLabel(const CTextureEntry* entry) {
    if (entry == nullptr) return 0;
    for (int32_t e = 0; e < kEntries; ++e)
        if (entry == Entry(e)) return kLabelEntry + e;
    for (int32_t k = 0; k < kBlocks; ++k) {
        if (!gBuilt[k]) continue;
        const SSpriteEngineData* s = Pool(k);
        if (s->mTexture == nullptr) continue;
        for (int32_t i = 0; i < s->mPartCount && i < 64; ++i)
            if (entry == &s->mTexture[i]) return 1000 + 100 * k + i;
    }
    return -1;
}

int32_t PoolIndex(const SSpriteEngineData* s) {
    if (s == nullptr) return -1;
    for (int32_t p = 0; p < kPool; ++p)
        if (s == Pool(p)) return p;
    return -2;
}
}  // namespace

// ---- The engine around the unit -------------------------------------------------------------------------------
namespace {
void* FakeMalloc(uint32_t size) {
    ev("Malloc", {static_cast<int64_t>(size)});
    // A size no sprite of the test needs fails as the real allocator would.
    if (!ret("Malloc", 1) || size > 0x10000) return nullptr;
    void* block = std::calloc(1, size + 16);
    newlab(block);
    return block;
}
void FakeFree(void*& block) {
    const int32_t label = lab(block);
    ev("Free", {label});
    forget(block);
    // Nothing is given back: the PowerPC side never uses an address twice either.
    block = nullptr;
}
// The sprite files are not read here, but the library needs the callbacks.
void* FakeOpen(const char*, const char*) { return nullptr; }
uint32_t FakeRead(void*, uint32_t, uint32_t, void*) { return 0; }
uint32_t FakeClose(void*) { return 0; }
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
int32_t CSplash::InterpolateSplash() { return 0; }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
namespace SamsUtilities {
// A little-endian host has the file's words the right way round already.
void _USwapBlock16(uint16_t*, int32_t) {}
void _USwapBlock32(uint32_t*, int32_t) {}
UPixels::UPixels(intptr_t, void*, int32_t, int32_t, int32_t, UPixel*) {}
UPixels::~UPixels() {}
void UPixels::Convert(UPixels&, bool) {}
}  // namespace SamsUtilities
CWavelet::~CWavelet() {}
CWavelet* gWaveLoadWavelet = nullptr;
CMapLinkBase::~CMapLinkBase() {}
int32_t RegisterLinkBase(CMapLinkBase*) { return 1; }
int32_t FreeLinkBase(CMapLinkBase*) { return 1; }
void* TGALoadAndConvert(char*, int32_t*, int32_t*, int32_t, lARGBType*, int32_t) { return nullptr; }
void DebugStr(const unsigned char*) {}

CDisplay::CDisplay() {}
CDisplay gei_Display;
CTransformBase* gei_MathUnit = &gMath;
CTransformBase::~CTransformBase() {}
CMatrix* gei_FinalMatrix = &gFinalMatrix;
D3DVECTOR gUpVector = {0.0f, 1.0f, 0.0f};   // engine data 0x10051c48
CHField* gei_HField = nullptr;
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
CSpriteManager* gei_SpriteManager = reinterpret_cast<CSpriteManager*>(gManagerStore);
CSpriteManager::CSpriteManager() {}
CSpriteManager::~CSpriteManager() {}
CTextureLoadManager* gei_TLoadManager = nullptr;
CTextureEntry* CTextureLoadManager::LoadFile(char* name) {
    ev("LoadFile", {}, {name});
    return nullptr;
}

// The texture cache: the manager's Reset prepares its blob shadow texture, a sprite with partial textures takes them off.
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("TCache.Prepare", {lab(this), EntryLabel(entry), fbits(distance)});
}
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("Cache.Remove", {lab(this), EntryLabel(entry)});
    return 1;
}

// ---- The draw primitive -----------------------------------------------------------------------------------------
namespace {
class TestDraw : public CDrawPrimitive {
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
TestDraw gDraw;

// Records what a sprite draw record holds and does what the
// real DrawSprite does to the caller's record: the fog bit joins the blending mode, the colour is ANDed with the
// light manager's. The four corners come in two events of eight words.
void RecordDraw(const char* name, const CDrawPrimitive* self, SSpriteDrawParameters* sprite, int32_t mode) {
    ev(name, {lab(self), mode, sprite->mFlags, static_cast<uint32_t>(sprite->mF08), sprite->mBlendingMode,
              PageLabel(sprite->mTexture), CanonBits(sprite->mU0), CanonBits(sprite->mV0), CanonBits(sprite->mU1),
              CanonBits(sprite->mV1), sprite->mF28});
    const uint32_t* w = static_cast<const uint32_t*>(sprite->mVertices);
    ev("Corners", {Canon(w[0]), Canon(w[1]), Canon(w[2]), Canon(w[3]), Canon(w[4]), Canon(w[5]), Canon(w[6]), Canon(w[7])});
    ev("Corners", {Canon(w[8]), Canon(w[9]), Canon(w[10]), Canon(w[11]), Canon(w[12]), Canon(w[13]), Canon(w[14]), Canon(w[15])});
    if ((gei_Display.mFlags & 0x2000) != 0) sprite->mBlendingMode |= 0x100000;
    sprite->mF08 = static_cast<int32_t>(static_cast<uint32_t>(sprite->mF08) & gLightMask);
}
}  // namespace

CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
// DrawRegularShadow leaves the record's last field as stack garbage in the original and zero here; the PowerPC side records
// it as 0 for that caller, so the translation's zero is what is compared.
int32_t CDrawPrimitive::DrawSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    RecordDraw("DrawSprite", this, sprite, mode);
    return static_cast<int32_t>(ret("DrawSprite", 1));
}
int32_t CDrawPrimitive::AddSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    RecordDraw("AddSprite", this, sprite, mode);
    return static_cast<int32_t>(ret("AddSprite", 1));
}
int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    ev("Execute", {lab(this), fbits(depth)});
    return static_cast<int32_t>(ret("Execute", 1));
}

// Turns the x and z of each of the four corners a quarter turn, exactly: (x, z) -> (z, -x).
void RotateVertex_Y(SVertex* vertices, float angle, SVertex* centre) {
    uint32_t* w = reinterpret_cast<uint32_t*>(vertices);
    ev("RotateVertex_Y", {CanonBits(angle), lab(centre), Canon(w[0]), Canon(w[1]), Canon(w[2]), Canon(w[4]), Canon(w[5]),
                          Canon(w[6]), Canon(w[8]), Canon(w[9]), Canon(w[10])});
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t x = w[4 * i], z = w[4 * i + 2];
        w[4 * i] = z;
        w[4 * i + 2] = x ^ 0x80000000u;
    }
}

// The instance members the unit does not contain: the frame's other halves.
int32_t SSpriteGameData::Prepare() {
    ev("SpritePrepare", {lab(this)});
    return 1;
}
int32_t SSpriteGameData::DrawRegular() {
    ev("DrawRegular", {lab(this)});
    return static_cast<int32_t>(ret("DrawRegular", 1));
}
int32_t SSpriteGameData::AddRegular() {
    ev("AddRegular", {lab(this)});
    return static_cast<int32_t>(ret("AddRegular", 1));
}

// ---- Reading state ------------------------------------------------------------------------------------------------
namespace {
int64_t* gOut;
int32_t gCount;
void put(int64_t value) { gOut[gCount++] = value; }

// A partial texture's pixels are named by their distance from the block the sprite had when they were made (a later Init may
// have given the sprite another block, and the heap's addresses are not the same on the two sides); a standalone entry has none.
void put_entry(const CTextureEntry* e, bool partial, const char* reference) {
    put(EntryLabel(e));
    put(e->mFlags);
    put(e->mWidth);
    put(e->mHeight);
    put(e->mCacheWidth);
    put(e->mCacheHeight);
    put(PageLabel(e->mTexture));
    put(CanonBits(e->mUSize));
    put(CanonBits(e->mVSize));
    put(CanonBits(e->mUOffset));
    put(CanonBits(e->mVOffset));
    put(e->mPitch);
    if (e->mData == nullptr) {
        put(-1);
    } else if (partial) {
        put(static_cast<const char*>(e->mData) - reference);
    } else {
        put(-2);
    }
}

void put_sprite(int32_t p) {
    const SSpriteEngineData* s = Pool(p);
    put(EntryLabel(s->mTexture));
    put(lab(s->mData));
    put(CanonBits(s->mAspect));
    put(s->mWidth);
    put(s->mHeight);
    put(s->mUsedWidth);
    put(s->mUsedHeight);
    put(s->mOffsetX);
    put(s->mPartCount);   // the same halfword as the y offset
}
}  // namespace

// ---- The entry points ---------------------------------------------------------------------------------------------
extern "C" {

void* tpw_sp_address(int32_t which) {
    switch (which) {
    case kLabelDraw: return &gDraw;
    case kLabelMath: return &gMath;
    case kLabelCache: return gCacheStore;
    case kLabelManager: return gManagerStore;
    case kLabelShadow: return &gShadow;
    case kLabelField: return gHFieldStore;
    case kLabelFinal: return &gFinalMatrix;
    case kLabelFile: return &gFile;
    default: break;
    }
    if (which >= kLabelPool && which < kLabelPool + kPool) return gPoolStore[which - kLabelPool];
    if (which >= kLabelInstance && which < kLabelInstance + kInstances) return gInstanceStore[which - kLabelInstance];
    if (which >= kLabelEntry && which < kLabelEntry + kEntries) return gEntryStore[which - kLabelEntry];
    if (which >= kLabelFileSprite && which < kLabelFileSprite + kFileSprites) return gFileSpriteStore[which - kLabelFileSprite];
    if (which > kLabelPage && which <= kLabelPage + kPages) return &gMarkers[which - kLabelPage];
    return nullptr;
}

// Builds the world. s: the scalars (display flags, aspect bits, width, height, constant bits, the cache's UI size, the renderer,
// the light mask, the light count, the current light, then the height field's columns, rows, inverse cell sizes, origin and
// cell sizes); shadow: the four matrices (64 words), the four cross products (12), the four shades; heights: the height
// field's table of floats (`count`, the grid starts `margin` floats into it); finalMatrix: 16 words. Recording must be off.
void tpw_sp_reset(const int64_t* s, const uint32_t* shadow, const uint32_t* heights, int32_t count, int32_t margin,
                  const uint32_t* finalMatrix) {
    std::memset(gPoolStore, 0xa5, sizeof gPoolStore);
    std::memset(gInstanceStore, 0xa5, sizeof gInstanceStore);
    std::memset(gEntryStore, 0, sizeof gEntryStore);
    std::memset(gFileSpriteStore, 0, sizeof gFileSpriteStore);
    std::memset(&gFile, 0, sizeof gFile);
    std::memset(gCacheStore, 0, sizeof gCacheStore);
    std::memset(gManagerStore, 0, sizeof gManagerStore);
    std::memset(gHFieldStore, 0, sizeof gHFieldStore);
    for (int32_t k = 0; k < kBlocks; ++k) {
        gBuilt[k] = false;
        gGenBlock[k] = nullptr;
    }
    for (int32_t g = 0; g < kInstances; ++g) {
        gRotation[g] = false;
        new (gInstanceStore[g]) SSpriteGameData;
    }

    gei_Display.mFlags = static_cast<uint32_t>(s[0]);
    gei_Display.mF04 = FromBits(static_cast<uint32_t>(s[1]));
    gei_Display.mWidth = static_cast<int32_t>(s[2]);
    gei_Display.mHeight = static_cast<int32_t>(s[3]);
    gei_Display.mF938 = FromBits(static_cast<uint32_t>(s[4]));
    gei_Display.mFrameCount = 0;
    Cache()->mUITextureSize = static_cast<int32_t>(s[5]);
    gei_System.mRenderer = static_cast<int32_t>(s[6]);
    gei_System.mFlags = 0;
    gei_TextureSettings.mFormat = 4;
    gLightMask = static_cast<uint32_t>(s[7]);
    gHotSprite = nullptr;

    // The shadow object.
    gShadow.mLightCount = static_cast<int32_t>(s[8]);
    gShadow.mCurrent = static_cast<int32_t>(s[9]);
    std::memcpy(gShadow.mShadowMatrices, shadow, 4 * sizeof(CMatrix));
    std::memcpy(gShadow.mLightCross, shadow + 64, 4 * sizeof(D3DVECTOR));
    std::memcpy(gShadow.mLightShades, shadow + 76, 4 * sizeof(uint32_t));

    // The height field.
    if (gHeights != nullptr) tpw::ArenaFree(gHeights);
    gHeights = tpw::ArenaAllocZeroed(static_cast<size_t>(count) * sizeof(float) + 16);
    std::memcpy(gHeights, heights, static_cast<size_t>(count) * sizeof(float));
    std::memset(&gRecord, 0, sizeof gRecord);
    gRecord.mF08[0] = FromBits(static_cast<uint32_t>(s[14]));
    gRecord.mF08[1] = FromBits(static_cast<uint32_t>(s[15]));
    gRecord.mF08[2] = FromBits(static_cast<uint32_t>(s[16]));
    gRecord.mF08[3] = FromBits(static_cast<uint32_t>(s[17]));
    gRecord.mHeights = static_cast<float*>(gHeights) + margin;
    gei_HField = Field();
    Field()->mF00 = &gRecord;
    Field()->mGridWidth = static_cast<int32_t>(s[10]);
    Field()->mGridHeight = static_cast<int32_t>(s[11]);
    Field()->mInvCellX = FromBits(static_cast<uint32_t>(s[12]));
    Field()->mInvCellZ = FromBits(static_cast<uint32_t>(s[13]));

    std::memcpy(&gFinalMatrix, finalMatrix, sizeof gFinalMatrix);
    gei_FinalMatrix = &gFinalMatrix;
    gei_MathUnit = &gMath;
    gei_DrawPrimitive = &gDraw;
    gei_SpriteManager = Manager();
    gei_TCache = Cache();
    std::memset(gSpriteGroups, 0xff, sizeof gSpriteGroups);
    // The harness's own allocations (the arena's bookkeeping) are not the unit's heap traffic.
    for (int32_t i = 0; i < 4; ++i) gHeapBase[i] = tpw_cl_heap[i];
}

// One operation. a[0] is the code; see the list in tests/test_SpritePartial.py.
int64_t tpw_sp_op(const int64_t* a) {
    switch (a[0]) {
    case 100:   // the environment
        switch (a[1]) {
        case 0: gei_Display.mFlags = static_cast<uint32_t>(a[2]); return 0;
        case 1: gei_Display.mF04 = FromBits(static_cast<uint32_t>(a[2])); return 0;
        case 2: gei_Display.mWidth = static_cast<int32_t>(a[2]); return 0;
        case 3: gei_Display.mHeight = static_cast<int32_t>(a[2]); return 0;
        case 4: gei_Display.mF938 = FromBits(static_cast<uint32_t>(a[2])); return 0;
        case 5: Cache()->mUITextureSize = static_cast<int32_t>(a[2]); return 0;
        case 6: gei_System.mRenderer = static_cast<int32_t>(a[2]); return 0;
        case 7: gLightMask = static_cast<uint32_t>(a[2]); return 0;
        case 8: gShadow.mCurrent = static_cast<int32_t>(a[2]); return 0;
        case 9: gShadow.mLightCount = static_cast<int32_t>(a[2]); return 0;
        default: return -8;
        }
    // ---- sprites with a pixel block
    case 10: {   // Init(width, height), constructing the sprite first if it was not
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        if (!gBuilt[k]) {
            new (gPoolStore[k]) SSpriteEngineData;
            gBuilt[k] = true;
        }
        return Pool(k)->Init(static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
    }
    case 11: {   // Close
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        if (!gBuilt[k]) return -8;
        return Pool(k)->Close();
    }
    case 12: {   // SetUsedPart
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        if (!gBuilt[k]) return -8;
        Pool(k)->SetUsedPart(static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]));
        return 0;
    }
    case 13: {   // GeneratePartialTexture
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        if (!gBuilt[k]) return -8;
        const int32_t made = Pool(k)->GeneratePartialTexture(static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
        gGenBlock[k] = static_cast<const char*>(Pool(k)->mData);
        return made;
    }
    case 14: {   // DestroyPartialTexture
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        if (!gBuilt[k]) return -8;
        return Pool(k)->DestroyPartialTexture();
    }
    case 15: {   // RefreshPartialTexture
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        if (!gBuilt[k]) return -8;
        return Pool(k)->RefreshPartialTexture();
    }
    case 16: {   // an entry of a block's partial array: flags, page, u0, v0, usize, vsize
        const int32_t k = static_cast<int32_t>(a[1]) & 3;
        const int32_t i = static_cast<int32_t>(a[2]);
        if (!gBuilt[k] || Pool(k)->mTexture == nullptr || i < 0 || i >= Pool(k)->mPartCount) return -8;
        CTextureEntry& e = Pool(k)->mTexture[i];
        e.mFlags = static_cast<uint32_t>(a[3]);
        e.mTexture = reinterpret_cast<CTextureBase*>(Marker(static_cast<int32_t>(a[4])));
        e.mUOffset = FromBits(static_cast<uint32_t>(a[5]));
        e.mVOffset = FromBits(static_cast<uint32_t>(a[6]));
        e.mUSize = FromBits(static_cast<uint32_t>(a[7]));
        e.mVSize = FromBits(static_cast<uint32_t>(a[8]));
        return 0;
    }
    case 17: {   // a standalone entry: flags, page, u0, v0, usize, vsize
        CTextureEntry* e = Entry(static_cast<int32_t>(a[1]));
        if (e == nullptr) return -8;
        e->mFlags = static_cast<uint32_t>(a[2]);
        e->mTexture = reinterpret_cast<CTextureBase*>(Marker(static_cast<int32_t>(a[3])));
        e->mUOffset = FromBits(static_cast<uint32_t>(a[4]));
        e->mVOffset = FromBits(static_cast<uint32_t>(a[5]));
        e->mUSize = FromBits(static_cast<uint32_t>(a[6]));
        e->mVSize = FromBits(static_cast<uint32_t>(a[7]));
        return 0;
    }
    case 18: {   // a plain sprite (pool 4..6): entry, width, height, used width, used height, offset x, offset y
        const int32_t p = 4 + (static_cast<int32_t>(a[1]) % 3);
        SSpriteEngineData* s = Pool(p);
        std::memset(static_cast<void*>(s), 0, sizeof *s);
        s->mTexture = Entry(static_cast<int32_t>(a[2]));
        s->mWidth = static_cast<uint16_t>(a[3]);
        s->mHeight = static_cast<uint16_t>(a[4]);
        s->mUsedWidth = static_cast<uint16_t>(a[5]);
        s->mUsedHeight = static_cast<uint16_t>(a[6]);
        s->mOffsetX = static_cast<int16_t>(a[7]);
        s->mOffsetY = static_cast<int16_t>(a[8]);
        return 0;
    }
    case 19: {   // the frame file: the count, then the textures of its sprites (-1: none)
        const int32_t n = static_cast<int32_t>(a[1]);
        std::memset(gFileSpriteStore, 0, sizeof gFileSpriteStore);
        for (int32_t i = 0; i < kFileSprites; ++i) FileSprite(i)->mTexture = Entry(static_cast<int32_t>(a[2 + i]));
        gFile.mCount = n;
        gFile.mSprites = FileSprite(0);
        return 0;
    }
    case 20: {   // an instance's field
        const int32_t g = static_cast<int32_t>(a[1]) % kInstances;
        SSpriteGameData* d = Instance(g);
        switch (a[2]) {
        case 0: d->mF10 = static_cast<int32_t>(a[3]); return 0;
        case 1:
            d->mPosition.x = FromBits(static_cast<uint32_t>(a[3]));
            d->mPosition.y = FromBits(static_cast<uint32_t>(a[4]));
            d->mPosition.z = FromBits(static_cast<uint32_t>(a[5]));
            return 0;
        case 2: d->mF24 = static_cast<int32_t>(a[3]); return 0;
        case 3: d->mF2C = FromBits(static_cast<uint32_t>(a[3])); return 0;
        case 4:
            d->mUsedWidth = static_cast<int16_t>(a[3]);
            d->mUsedHeight = static_cast<int16_t>(a[4]);
            gRotation[g] = false;
            return 0;
        case 5:
            d->mRotation = FromBits(static_cast<uint32_t>(a[3]));
            gRotation[g] = true;
            return 0;
        case 6: d->mShadowLift = FromBits(static_cast<uint32_t>(a[3])); return 0;
        case 7: d->mSprite = a[3] >= 0 ? Pool(static_cast<int32_t>(a[3]) % kPool) : nullptr; return 0;
        case 8:
            d->mFrameFile = a[3] != 0 ? &gFile : nullptr;
            d->mFrameBase = static_cast<uint16_t>(a[4]);
            for (int32_t i = 0; i < 4; ++i) d->mLightFrames[i] = static_cast<uint8_t>(a[5] >> (8 * i));
            return 0;
        case 9: d->mDepth = FromBits(static_cast<uint32_t>(a[3])); return 0;
        default: return -8;
        }
    }
    // ---- the draws
    case 21: return Instance(static_cast<int32_t>(a[1]) % kInstances)->DrawPartial();
    case 22: return Instance(static_cast<int32_t>(a[1]) % kInstances)->DrawRegularShadow();
    case 23: return Instance(static_cast<int32_t>(a[1]) % kInstances)->AddRegularShadow(Entry(static_cast<int32_t>(a[2])));
    case 24: {   // CreateShadow: the instance, the matrix, the two scales; the corners follow as an event
        SVertex corners[4];
        std::memset(corners, 0xa5, sizeof corners);
        const int32_t m = static_cast<int32_t>(a[2]) & 3;
        const int32_t result = Instance(static_cast<int32_t>(a[1]) % kInstances)
                                   ->CreateShadow(corners, &gShadow.mShadowMatrices[m], FromBits(static_cast<uint32_t>(a[3])),
                                                  FromBits(static_cast<uint32_t>(a[4])));
        const uint32_t* w = reinterpret_cast<const uint32_t*>(corners);
        ev("Shadow", {Canon(w[0]), Canon(w[1]), Canon(w[2]), Canon(w[3]), Canon(w[4]), Canon(w[5]), Canon(w[6]), Canon(w[7])});
        ev("Shadow", {Canon(w[8]), Canon(w[9]), Canon(w[10]), Canon(w[11]), Canon(w[12]), Canon(w[13]), Canon(w[14]), Canon(w[15])});
        return result;
    }
    // ---- the sprite manager
    case 25: Manager()->Reset(); return 0;
    case 26: return Manager()->AddSprite(Instance(static_cast<int32_t>(a[1]) % kInstances));
    case 27: return Manager()->AddPartialSprite(Instance(static_cast<int32_t>(a[1]) % kInstances));
    case 28: return Manager()->Render();
    case 29: return Manager()->RenderSprites();
    case 30: return Manager()->RenderPartialSprites();
    case 31: Manager()->mTextures = Entry(static_cast<int32_t>(a[1])); return 0;
    case 32: return gShadow.GetFirstShadowMatrix() != nullptr ? 1 : 0;
    default: return -9;
    }
}

// The state, as a list of numbers in the order tests/test_SpritePartial.py reads the original's. Returns how many.
int32_t tpw_sp_state(int64_t* out) {
    gOut = out;
    gCount = 0;
    for (int32_t i = 0; i < 4; ++i) put(tpw_cl_heap[i] - gHeapBase[i]);
    for (int32_t k = 0; k < kBlocks; ++k) {
        put(gBuilt[k] ? 1 : 0);
        if (!gBuilt[k]) continue;
        put_sprite(k);
        const SSpriteEngineData* s = Pool(k);
        if (s->mTexture != nullptr && s->mPartCount != 0) {
            for (int32_t i = 0; i < s->mPartCount && i < kMostParts; ++i) put_entry(&s->mTexture[i], true, gGenBlock[k]);
        }
    }
    for (int32_t g = 0; g < kInstances; ++g) {
        const SSpriteGameData* d = Instance(g);
        put(d->mF10);
        put(CanonBits(d->mPosition.x));
        put(CanonBits(d->mPosition.y));
        put(CanonBits(d->mPosition.z));
        put(CanonBits(d->mShadowLift));
        put(d->mF24);
        put(PoolIndex(d->mSprite));
        put(CanonBits(d->mF2C));
        if (gRotation[g]) {
            put(CanonBits(d->mRotation));
        } else {
            put(d->mUsedWidth);
            put(d->mUsedHeight);
        }
        put(d->mFrameFile == nullptr ? 0 : d->mFrameFile == &gFile ? kLabelFile : -1);
        put(d->mFrameBase);
        put(d->mLightFrames[0] | (d->mLightFrames[1] << 8) | (d->mLightFrames[2] << 16) | (static_cast<int64_t>(d->mLightFrames[3]) << 24));
        put(CanonBits(d->mDepth));
        put(d->mNext == nullptr ? 0 : 1);
    }
    for (int32_t e = 0; e < kEntries; ++e) put_entry(Entry(e), false, nullptr);
    put(gShadow.mCurrent);
    put(gShadow.mLightCount);
    CSpriteManager* m = Manager();
    put(m->mF2404);
    put(m->mF2408);
    put(m->mF240C);
    for (int32_t i = 0; i < m->mF2404 && i < 40; ++i) {
        const SSpriteGameData* d = m->mSprites[i];
        int32_t g = -1;
        for (int32_t k = 0; k < kInstances; ++k)
            if (d == Instance(k)) g = k;
        put(g);
    }
    for (int32_t i = 0; i < m->mF2408 && i < 40; ++i) {
        const SSpriteGameData* d = m->mPartialSprites[i];
        int32_t g = -1;
        for (int32_t k = 0; k < kInstances; ++k)
            if (d == Instance(k)) g = k;
        put(g);
    }
    put(EntryLabel(m->mTextures));
    put(gHotSprite == nullptr ? 0 : 1);
    return gCount;
}

}  // extern "C"
