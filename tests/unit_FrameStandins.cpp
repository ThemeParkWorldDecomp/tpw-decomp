// Test entry points for the lobby frame's last engine stand-ins (library unit_FrameStandins:
// src/engine/DrawPrimitiveOpenGL.cpp, src/engine/OpenGLTexture.cpp, src/engine/TextureCache.cpp, src/engine/tb_profiler.cpp and
// src/engine/Rain.cpp): COpenGLTexture::Download, COpenGLDrawPrimitive's destructor,
// CTextureCache::Entry_Specific_Compress and BalanceCache_ClearTexture, CParticle::Prepare and
// PlotParticles, CRain::Prepare, Render and RenderLightning2.
//
// tests/test_FrameStandins.py builds the same world twice from one description, in emulator memory
// for the original and through the entry points below for the translation, runs one function on
// both and compares the answer, the recorded calls and the state of everything the call could change.
//
// Everything outside the unit is a fake that records its arguments (floats by their bits, a NaN as one
// value): the entry's GetData, StoreAsWrapping, FlushEntryFlags and GetBlendingMode, the compressed
// image's IsNaturallyFlipped, the texture's SetFlags, the cache's Prepare, the clipper's
// CheckBoundingBox, the matrix's TransformVertex, RotateVertex_Y, the draw primitive's GetNextPrimitive,
// ProjectCurrentPrimitive, ExecuteCurrentPrimitive and DrawSprite, the rain's drawing members, the
// game's clock and view hooks, the particle system's hooks and the assertion handler.
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "Wavelet.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/msl.h"
#include "tb_clipping.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_particle.h"
#include "tb_printf_opengl.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

// A NaN (as float bits) as one value: its sign and payload depend on the processor.
uint32_t canonical(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}

// A float as recorded.
uint32_t cbits(float value) { return canonical(fbits(value)); }

float from_bits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// A description is a flat list of numbers, read in order.
struct Reader {
    const int64_t* data;
    int32_t at;
    int64_t next() { return data[at++]; }
    int32_t i32() { return static_cast<int32_t>(next()); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    float f32() { return from_bits(u32()); }
};

// The state is a flat list too.
int64_t* gOut;
int32_t gCount;
int32_t gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOut[gCount] = value;
    ++gCount;
}
void begin_output(int64_t* out, int32_t capacity) {
    gOut = out;
    gCapacity = capacity;
    gCount = 0;
}

// ---- Download's world: one page, one entry, one image, the shared upload buffer ----------------------
constexpr int32_t kUploadBytes = 256 * 256 * 4;
constexpr int32_t kImageWords = 1 << 17;

alignas(16) uint8_t gUpload[kUploadBytes];
uint16_t gImage[kImageWords];
COpenGLTexture gPage;
CTextureEntry gEntry;

// The compressed image an entry can unpack from: only the question Download asks it.
class FakeComp : public CCompBase {
public:
    int32_t Blit(SUnpackDestination*, int32_t) override { return 0; }
    int32_t Decompress(SUnpackDestination*) override { return 0; }
    uint32_t AverageColor() override { return 0; }
    int32_t IsNaturallyFlipped() override {
        ev("IsNaturallyFlipped", {lab(this)});
        return static_cast<int32_t>(ret("IsNaturallyFlipped", 0));
    }
};
FakeComp gComp;

// ---- The cache helpers' world: a cache, six entries, two pages, a list of entries -------------------
constexpr int32_t kCacheEntries = 6;
constexpr int32_t kCachePages = 2;
constexpr int32_t kCacheArray = 8;
alignas(CTextureCache) unsigned char gCacheStorage[sizeof(CTextureCache)];
CTextureEntry gCacheEntries[kCacheEntries];
CTextureLink gCacheLinks[kCachePages];
COpenGLTexture gCachePages[kCachePages];
CTextureEntry* gCacheArrayPointers[kCacheArray];

CTextureEntry* CacheEntryAt(int32_t index) { return index > 0 ? &gCacheEntries[index - 1] : nullptr; }

// ---- The frame's globals shared by the particle and the rain worlds -----------------------------------
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
SPrimitive gPrimitive;   // what GetNextPrimitive hands out
alignas(CClipping) CClipping gClippingObject;
alignas(CMatrix) unsigned char gFinalMatrixStorage[sizeof(CMatrix)];
alignas(CMatrix) unsigned char gViewMatrixStorage[sizeof(CMatrix)];
alignas(CMatrix) unsigned char gInverseMatrixStorage[sizeof(CMatrix)];
alignas(CTextureCache) unsigned char gFrameCacheStorage[sizeof(CTextureCache)];
char gPageMarkers[8];   // CTextureBase* values that are only compared

// Labels of the objects the frame members are given (the test uses the same numbers).
enum : int32_t {
    kLabelCache = 1,
    kLabelDraw = 2,
    kLabelClipping = 3,
    kLabelFinal = 4,
    kLabelTCache = 5,
    kLabelRain = 6,
    kLabelFrom = 7,
    kLabelTo = 8,
    kLabelView = 9,
    kLabelInverse = 10,
    kLabelEntry = 11,    // + index
    kLabelPage = 21,     // + index
    kLabelList = 41,     // + emitter's position in the list of active emitters
    kLabelSlot = 61,     // + index
};

CTextureBase* PageMarker(int32_t index) {
    return index > 0 ? reinterpret_cast<CTextureBase*>(&gPageMarkers[index - 1]) : nullptr;
}

// ---- The particle world ----------------------------------------------------------------------------
constexpr int32_t kParticleEntries = 4;
constexpr int32_t kParticleSlots = 4;
alignas(CParticle) unsigned char gParticleStorage[sizeof(CParticle)];
CTextureEntry gParticleEntries[kParticleEntries];
PTCLTexture gParticleSlots[kParticleSlots];
int32_t gEmitterCount[CParticle::kEmitters];
PARPOINT3D gEmitterLow[CParticle::kEmitters];
PARPOINT3D gEmitterHigh[CParticle::kEmitters];
std::vector<PTCL> gEmitterList[CParticle::kEmitters];
bool gEmitterHasList[CParticle::kEmitters];

CParticle* Particle() { return reinterpret_cast<CParticle*>(gParticleStorage); }

// The particle system's hooks (the game's PAR_* functions).
int32_t HookParticleCount(uint32_t emitter) {
    ev("GetEmitterParticleCount", {emitter});
    return emitter < CParticle::kEmitters ? gEmitterCount[emitter] : 0;
}
void HookBoundingBox(uint32_t emitter, PARPOINT3D* low, PARPOINT3D* high) {
    ev("GetEmitterBoundingBox", {emitter});
    *low = gEmitterLow[emitter];
    *high = gEmitterHigh[emitter];
}
PTCL* HookBuildList(int32_t emitter) {
    ev("BuildDrawingList", {emitter});
    return gEmitterHasList[emitter] ? gEmitterList[emitter].data() : nullptr;
}
void HookFreeList(PTCL* list) { ev("FreeDrawingList", {lab(list)}); }

// ---- The rain world ----------------------------------------------------------------------------------
constexpr int32_t kRainEntries = 3;
constexpr int32_t kRainBytes = 0xa1cc;   // the Mac object; the host's is 12 bytes longer after 0x3354
alignas(CRain) unsigned char gRainStorage[sizeof(CRain)];
CTextureEntry gRainEntries[kRainEntries];
D3DVECTOR gFrom;
D3DVECTOR gTo;

CRain* Rain() { return reinterpret_cast<CRain*>(gRainStorage); }
CTextureEntry* RainEntryAt(int32_t index) { return index > 0 ? &gRainEntries[index - 1] : nullptr; }

// The host address of the word at Mac offset `offset` of the rain object (the three texture
// pointers, 4 bytes each on the Mac, are 8 here).
unsigned char* RainWord(int32_t offset) {
    return gRainStorage + (offset < 0x3354 ? offset : offset + 12);
}

// The game's hooks for the clock (milliseconds) and the first-person view.
uint32_t FakeReadClock() {
    ev("ReadClock");
    return static_cast<uint32_t>(ret("ReadClock", 0));
}
int32_t FakeIsFirstPerson() {
    ev("IsFirstPerson");
    return static_cast<int32_t>(ret("IsFirstPerson", 0));
}

}  // namespace

// ---- The engine around the unit ---------------------------------------------------------------------
CSystem::CSystem() {}
CSystem gei_System;
CDisplay::CDisplay() {}
CDisplay gei_Display;
TbReadClockFn gReadClock;
TbIsFirstPersonFn gIsFirstPerson;
void* gOpenGLTextureData;
CClipping* gei_Clipping = &gClippingObject;
CMatrix* gei_FinalMatrix = reinterpret_cast<CMatrix*>(gFinalMatrixStorage);
CMatrix* gei_ViewMatrix = reinterpret_cast<CMatrix*>(gViewMatrixStorage);
CMatrix* gei_InvViewMatrix = reinterpret_cast<CMatrix*>(gInverseMatrixStorage);
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gFrameCacheStorage);
CDrawPrimitive* gei_DrawPrimitive = &gDraw;

void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

// The entry: the constructor and destructor are another unit's; GetData hands out the case's image.
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
void* CTextureEntry::GetData() {
    ev("GetData", {lab(this)});
    return gImage;
}
int32_t CTextureEntry::StoreAsWrapping() {
    ev("StoreAsWrapping", {lab(this)});
    return static_cast<int32_t>(ret("StoreAsWrapping", 0));
}
// Takes the page bits off the flags, as the original does (without its assertion).
void CTextureEntry::FlushEntryFlags() {
    ev("FlushEntryFlags", {lab(this)});
    mFlags &= ~0x7000u;
}
uint32_t CTextureEntry::GetBlendingMode() {
    ev("GetBlendingMode", {lab(this)});
    return static_cast<uint32_t>(ret("GetBlendingMode", 0));
}

// The page's flags: all but the kept bit are replaced.
void CTextureBase::SetFlags(int32_t flags) {
    ev("SetFlags", {lab(this), flags});
    mFlags = static_cast<uint32_t>(flags) | (mFlags & kKeptBySetFlags);
}
CTextureBase::~CTextureBase() {}

// The OpenGL page: only Download (the unit's) and the members its vtable needs.
COpenGLTexture::COpenGLTexture() {}
COpenGLTexture::~COpenGLTexture() {}
double COpenGLTexture::TestUploadSpeed() { return 0.0; }
int32_t COpenGLTexture::Create(int32_t, int32_t, int32_t, int32_t) { return 0; }
int32_t COpenGLTexture::Destroy() { return 0; }
int32_t COpenGLTexture::LockSurface() { return 0; }
int32_t COpenGLTexture::UnlockSurface() { return 0; }
int32_t COpenGLTexture::MipmapsUsed() { return 0; }
int32_t COpenGLTexture::GetPixelCount() { return 0; }
void COpenGLTexture::GetTexture() {}

CCompBase::~CCompBase() {}
int32_t CCompBase::IsNaturallyFlipped() { return 0; }

// The draw primitive: the constructor is another unit's, the destructors record (the OpenGL one is the
// unit's own, whose base destructor is the one the original calls with a flag of 0).
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() { ev("BaseDtor", {lab(this), 0}); }
COpenGLDrawPrimitive::COpenGLDrawPrimitive() {}
int32_t COpenGLDrawPrimitive::Init() { return 0; }
int32_t COpenGLDrawPrimitive::SetBlendingMode(int32_t) { return 0; }
int32_t COpenGLDrawPrimitive::SetTexture(int32_t, void*) { return 0; }
int32_t COpenGLDrawPrimitive::Execute(SPrimitive*) { return 0; }
int32_t COpenGLDrawPrimitive::ExecuteShadow(SPrimitive*) { return 0; }
int32_t COpenGLDrawPrimitive::SetFogParameters(float, float, uint32_t) { return 0; }
int32_t COpenGLDrawPrimitive::BeginDraw() { return 0; }
int32_t COpenGLDrawPrimitive::EndDraw() { return 0; }
int32_t COpenGLDrawPrimitive::DrawTestPoly() { return 0; }
int32_t COpenGLDrawPrimitive::DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawEdgeBox() { return 0; }
int32_t COpenGLDrawPrimitive::DrawPoly(XZType*, int32_t, int32_t, float) { return 0; }
int32_t COpenGLDrawPrimitive::DrawProfileBox() { return 0; }
float COpenGLDrawPrimitive::GetPixelFillRate() { return 0.0f; }
int32_t COpenGLDrawPrimitive::DrawWiredframedBoundingBox(SVertex*, int32_t, int32_t) { return 0; }
int32_t COpenGLDrawPrimitive::GetPixel(int32_t, int32_t, uint32_t*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawDetectPoly(float, int32_t, int32_t, float, float, float, float) { return 0; }

// The cache's request for a picture.
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("TCache.Prepare", {lab(this), lab(entry), fbits(distance)});
}

// The clipper's answer for a box comes from the "CheckBoundingBox" queue (a byte).
uint8_t CClipping::CheckBoundingBox(const sAlignedBox& box, CMatrix& matrix) {
    ev("CheckBoundingBox", {lab(this), cbits(box.mMin[0]), cbits(box.mMin[1]), cbits(box.mMin[2]), cbits(box.mMax[0]),
                            cbits(box.mMax[1]), cbits(box.mMax[2]), lab(&matrix)});
    return static_cast<uint8_t>(ret("CheckBoundingBox", 0));
}

// Moves the three input words round (the fourth is the "TransformW" queue's), so a result shows
// which input went where; the bits are not interpreted, so any pattern is exact.
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    const uint32_t* v = static_cast<const uint32_t*>(in);
    ev("TransformVertex", {lab(this), type, v[0], v[1], v[2]});
    uint32_t* o = static_cast<uint32_t*>(out);
    o[0] = v[1];
    o[1] = v[2] ^ 0x80000000u;
    o[2] = v[0];
    o[3] = static_cast<uint32_t>(ret("TransformW", 0x3f800000));
}

// Turns the x and y of each of four corners by the quarter turns the angle's bits name:
// 0 none, 1 (x, y) -> (y, -x), 2 -> (-x, -y), 3 -> (-y, x). Exact on any bit pattern.
void RotateVertex_Y(SVertex* vertices, float angle, SVertex* centre) {
    uint32_t* w = reinterpret_cast<uint32_t*>(vertices);
    ev("RotateVertex_Y", {cbits(angle), lab(centre), canonical(w[0]), canonical(w[1]), canonical(w[4]), canonical(w[5]),
                          canonical(w[8]), canonical(w[9]), canonical(w[12]), canonical(w[13])});
    const uint32_t turn = cbits(angle) & 3;
    for (int32_t i = 0; i < 4; ++i) {
        const uint32_t x = w[4 * i];
        const uint32_t y = w[4 * i + 1];
        switch (turn) {
        case 1:
            w[4 * i] = y;
            w[4 * i + 1] = x ^ 0x80000000u;
            break;
        case 2:
            w[4 * i] = x ^ 0x80000000u;
            w[4 * i + 1] = y ^ 0x80000000u;
            break;
        case 3:
            w[4 * i] = y ^ 0x80000000u;
            w[4 * i + 1] = x;
            break;
        default:
            break;
        }
    }
}

// The draw primitive's members the frame members call.
int32_t CDrawPrimitive::DrawSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    ev("DrawSprite", {lab(this), sprite->mFlags, sprite->mF08, sprite->mBlendingMode, lab(sprite->mTexture),
                      cbits(sprite->mU0), cbits(sprite->mV0), cbits(sprite->mU1), cbits(sprite->mV1), sprite->mF28,
                      mode});
    const float* corners = static_cast<const float*>(sprite->mVertices);
    for (int32_t i = 0; i < 4; ++i) {
        ev("SpriteCorner", {i, cbits(corners[4 * i]), cbits(corners[4 * i + 1]), cbits(corners[4 * i + 2]),
                            cbits(corners[4 * i + 3])});
    }
    return static_cast<int32_t>(ret("DrawSprite", 1));
}
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, lab(texture)});
    return ret("GetNextPrimitive", 1) != 0 ? &gPrimitive : nullptr;
}
void CDrawPrimitive::ProjectCurrentPrimitive(int32_t type) { ev("ProjectCurrentPrimitive", {lab(this), type}); }
int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    ev("ExecuteCurrentPrimitive", {lab(this), cbits(depth)});
    return static_cast<int32_t>(ret("ExecuteCurrentPrimitive", 1));
}

// The weather's drawing members (still to do).
int32_t CRain::__RenderLightning() {
    ev("RenderLightning", {lab(this)});
    return static_cast<int32_t>(ret("RenderLightning", 1));
}
int32_t CRain::__RenderRain(float x, float y, float z) {
    ev("RenderRain", {lab(this), cbits(x), cbits(y), cbits(z)});
    return static_cast<int32_t>(ret("RenderRain", 1));
}
int32_t CRain::__RenderSnow(float x, float y, float z) {
    ev("RenderSnow", {lab(this), cbits(x), cbits(y), cbits(z)});
    return static_cast<int32_t>(ret("RenderSnow", 1));
}
int32_t CRain::NewRenderLightning(D3DVECTOR* from, D3DVECTOR* to) {
    ev("NewRenderLightning", {lab(this), lab(from), lab(to)});
    return static_cast<int32_t>(ret("NewRenderLightning", 1));
}
int32_t CRain::AddLightningSection(D3DVECTOR* from, D3DVECTOR* to) {
    ev("AddLightningSection", {lab(this), lab(from), lab(to)});
    return static_cast<int32_t>(ret("AddLightningSection", 1));
}

extern "C" {

// ---- Download -------------------------------------------------------------------------------------
// a: bytes per pixel, pitch, flags, width, height, cache width, cache height, the image's row length,
// x, y, whether the entry is a compressed image (its mData then is the object that answers
// IsNaturallyFlipped). `image` holds imageCount halfwords, `buffer` the upload buffer (in and out).
// out: the entry's flags afterwards. Answers what Download answers.
int64_t tpw_fs_download(const int64_t* a, const uint16_t* image, int32_t imageCount, uint8_t* buffer, int64_t* out) {
    gPage.mBytesPerPixel = static_cast<int32_t>(a[0]);
    gPage.mPitch = static_cast<int32_t>(a[1]);
    std::memset(static_cast<void*>(&gEntry), 0xa5, sizeof gEntry);
    gEntry.mFlags = static_cast<uint32_t>(a[2]);
    gEntry.mWidth = static_cast<int16_t>(a[3]);
    gEntry.mHeight = static_cast<int16_t>(a[4]);
    gEntry.mCacheWidth = static_cast<int16_t>(a[5]);
    gEntry.mCacheHeight = static_cast<int16_t>(a[6]);
    gEntry.mPitch = static_cast<int16_t>(a[7]);
    const bool compressed = (gEntry.mFlags & CTextureEntry::kCompressed) != 0;
    gEntry.mData = compressed ? static_cast<void*>(static_cast<CCompBase*>(&gComp)) : static_cast<void*>(gImage);
    std::memset(gImage, 0, sizeof gImage);
    std::memcpy(gImage, image, static_cast<size_t>(imageCount) * 2);
    std::memcpy(gUpload, buffer, kUploadBytes);
    gOpenGLTextureData = gUpload;
    tpw_cl_label(1, &gEntry);
    tpw_cl_label(2, static_cast<void*>(&gPage));
    tpw_cl_label(3, static_cast<CCompBase*>(&gComp));
    const int64_t result = gPage.Download(&gEntry, static_cast<int32_t>(a[8]), static_cast<int32_t>(a[9]));
    std::memcpy(buffer, gUpload, kUploadBytes);
    out[0] = gEntry.mFlags;
    return result;
}

// ---- The draw primitive's destructor --------------------------------------------------------------
// mode 1: a heap object, deleted (the original's flag above 0); 2: an object in place, destroyed (a
// flag below 0); 0: nothing (a flag of 0 is the destructor of a base object, which C++ never calls
// by itself); 3: a null pointer deleted (the original returns at once).
void tpw_fs_destroy(int32_t mode) {
    if (mode == 1) {
        COpenGLDrawPrimitive* object = new COpenGLDrawPrimitive;
        tpw_cl_label(1, static_cast<void*>(object));
        tpw_cl_label(2, static_cast<CDrawPrimitive*>(object));
        delete object;
    } else if (mode == 2) {
        alignas(COpenGLDrawPrimitive) unsigned char storage[sizeof(COpenGLDrawPrimitive)];
        COpenGLDrawPrimitive* object = new (storage) COpenGLDrawPrimitive;
        tpw_cl_label(1, static_cast<void*>(object));
        tpw_cl_label(2, static_cast<CDrawPrimitive*>(object));
        object->~COpenGLDrawPrimitive();
    } else if (mode == 3) {
        COpenGLDrawPrimitive* object = nullptr;
        delete object;
    }
}

// ---- The cache helpers -----------------------------------------------------------------------------
// The description (read in order): the cache (the queue's first entry, mF18C4, the five priority limits),
// six entries (flags, cache width, cache height, priority bits, sort key, next, page, width, height), two
// pages (the entries on it, the texture's flags), the array (a count, then entries). An index of an entry
// is 1..6 (0: null), a page's 1 or 2.
//   op 1: Entry_Specific_Compress(array, a[0], a[1], a[2])      op 2: BalanceCache_ClearTexture(page a[0])
// out: the cache (queue first, mF18C4), the entries (flags, cache width, cache height, sort key, next, page,
// width, height, priority bits), the pages (their entries, the texture's flags).
int64_t tpw_fs_cache(int32_t op, const int64_t* a, const int64_t* description, int64_t* out, int32_t capacity) {
    begin_output(out, capacity);
    Reader in{description, 0};
    CTextureCache* cache = reinterpret_cast<CTextureCache*>(gCacheStorage);
    std::memset(static_cast<void*>(cache), 0xa5, sizeof(CTextureCache));
    cache->mF18AC = CacheEntryAt(in.i32());
    cache->mF18C4 = in.i32();
    cache->mF18EC = in.f32();
    cache->mF18F0 = in.f32();
    cache->mF18F4 = in.f32();
    cache->mF18F8 = in.f32();
    cache->mF18FC = in.f32();
    for (int32_t i = 0; i < kCacheEntries; ++i) {
        CTextureEntry& entry = gCacheEntries[i];
        std::memset(static_cast<void*>(&entry), 0xa5, sizeof entry);
        entry.mFlags = in.u32();
        entry.mCacheWidth = static_cast<int16_t>(in.i32());
        entry.mCacheHeight = static_cast<int16_t>(in.i32());
        entry.mPriority = in.f32();
        entry.mSortKey = in.i32();
        entry.mNext = CacheEntryAt(in.i32());
        const int32_t page = in.i32();
        entry.mTexture = page > 0 ? static_cast<CTextureBase*>(&gCachePages[page - 1]) : nullptr;
        entry.mWidth = static_cast<int16_t>(in.i32());
        entry.mHeight = static_cast<int16_t>(in.i32());
    }
    for (int32_t i = 0; i < kCachePages; ++i) {
        gCacheLinks[i].mTexture = &gCachePages[i];
        gCacheLinks[i].mEntries = CacheEntryAt(in.i32());
        gCachePages[i].mFlags = in.u32();
    }
    const int32_t count = in.i32();
    for (int32_t i = 0; i < kCacheArray; ++i) gCacheArrayPointers[i] = i < count ? CacheEntryAt(in.i32()) : nullptr;

    tpw_cl_label(kLabelCache, cache);
    for (int32_t i = 0; i < kCacheEntries; ++i) tpw_cl_label(kLabelEntry + i, &gCacheEntries[i]);
    for (int32_t i = 0; i < kCachePages; ++i) {
        tpw_cl_label(kLabelPage + i, static_cast<CTextureBase*>(&gCachePages[i]));
        tpw_cl_label(31 + i, &gCacheLinks[i]);
    }

    if (op == 1) {
        cache->Entry_Specific_Compress(gCacheArrayPointers, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]),
                                       static_cast<int32_t>(a[2]));
    } else {
        cache->BalanceCache_ClearTexture(&gCacheLinks[a[0] - 1]);
    }

    put(lab(cache->mF18AC));
    put(cache->mF18C4);
    for (const CTextureEntry& entry : gCacheEntries) {
        put(entry.mFlags);
        put(entry.mCacheWidth);
        put(entry.mCacheHeight);
        put(entry.mSortKey);
        put(lab(entry.mNext));
        put(lab(entry.mTexture));
        put(entry.mWidth);
        put(entry.mHeight);
        put(cbits(entry.mPriority));
    }
    for (int32_t i = 0; i < kCachePages; ++i) {
        put(lab(gCacheLinks[i].mEntries));
        put(gCachePages[i].mFlags);
    }
    return 0;
}
int32_t tpw_fs_count() { return gCount; }

// ---- The particle world -----------------------------------------------------------------------------
// The description: the display (frame count, the word at +0x84, the aspect), the particle object (its two
// frame words, then the 0x78 clip bytes), four entries (flags, page, U offset, V offset, U size, V size),
// four texture slots (entry 1..4 or 0 for a null entry, aspect bits), then the emitters that have anything: their
// number, count, the six box words, whether there is a list and its length, and the records (x, y, z, colour,
// texture slot 0..3 or -1 for none, size, angle, flags).
//   op 1: Prepare()     op 2: PlotParticles()
// out: the object's two frame words and its clip bytes. Answers what the function answers.
int64_t tpw_fs_particles(int32_t op, const int64_t* description, int64_t* out, int32_t capacity) {
    begin_output(out, capacity);
    Reader in{description, 0};
    gei_Display.mFrameCount = in.i32();
    gei_Display.mF84 = in.i32();
    gei_Display.mF04 = in.f32();
    CParticle* particle = Particle();
    std::memset(static_cast<void*>(particle), 0xa5, sizeof(CParticle));
    particle->mF04 = in.i32();
    particle->mF08 = in.i32();
    for (int32_t i = 0; i < CParticle::kEmitters; ++i) particle->mClip[i] = static_cast<uint8_t>(in.u32());
    particle->mGetEmitterParticleCount = HookParticleCount;
    particle->mGetEmitterBoundingBox = HookBoundingBox;
    particle->mBuildDrawingList = HookBuildList;
    particle->mFreeDrawingList = HookFreeList;
    for (int32_t i = 0; i < kParticleEntries; ++i) {
        CTextureEntry& entry = gParticleEntries[i];
        std::memset(static_cast<void*>(&entry), 0xa5, sizeof entry);
        entry.mFlags = in.u32();
        entry.mTexture = PageMarker(in.i32());
        entry.mUOffset = in.f32();
        entry.mVOffset = in.f32();
        entry.mUSize = in.f32();
        entry.mVSize = in.f32();
    }
    for (int32_t i = 0; i < kParticleSlots; ++i) {
        const int32_t entry = in.i32();
        gParticleSlots[i].mEntry = entry > 0 ? &gParticleEntries[entry - 1] : nullptr;
        gParticleSlots[i].mF04 = 0x5a5a5a5a;
        gParticleSlots[i].mAspect = in.f32();
    }
    for (int32_t i = 0; i < CParticle::kEmitters; ++i) {
        gEmitterCount[i] = 0;
        gEmitterLow[i] = PARPOINT3D{0, 0, 0};
        gEmitterHigh[i] = PARPOINT3D{0, 0, 0};
        gEmitterHasList[i] = false;
        gEmitterList[i].clear();
    }
    const int32_t active = in.i32();
    for (int32_t k = 0; k < active; ++k) {
        const int32_t emitter = in.i32();
        gEmitterCount[emitter] = in.i32();
        gEmitterLow[emitter] = PARPOINT3D{in.i32(), in.i32(), in.i32()};
        gEmitterHigh[emitter] = PARPOINT3D{in.i32(), in.i32(), in.i32()};
        gEmitterHasList[emitter] = in.i32() != 0;
        const int32_t length = in.i32();
        gEmitterList[emitter].resize(static_cast<size_t>(length > 0 ? length : 1));
        for (int32_t i = 0; i < length; ++i) {
            PTCL& record = gEmitterList[emitter][i];
            record.mX = in.i32();
            record.mY = in.i32();
            record.mZ = in.i32();
            record.mColour = in.u32();
            const int32_t slot = in.i32();
            record.mTexture = slot < 0 ? ParticleNoTexture() : &gParticleSlots[slot];
            record.mSize = static_cast<uint16_t>(in.u32());
            record.mAngle = static_cast<uint16_t>(in.u32());
            record.mFlags = in.u32();
        }
        tpw_cl_label(kLabelList + k, gEmitterList[emitter].data());
    }
    tpw_cl_label(kLabelCache, particle);
    tpw_cl_label(kLabelDraw, static_cast<CDrawPrimitive*>(&gDraw));
    tpw_cl_label(kLabelClipping, &gClippingObject);
    tpw_cl_label(kLabelFinal, gei_FinalMatrix);
    tpw_cl_label(kLabelTCache, gei_TCache);
    for (int32_t i = 0; i < kParticleEntries; ++i) tpw_cl_label(kLabelEntry + i, &gParticleEntries[i]);
    for (int32_t i = 0; i < 8; ++i) tpw_cl_label(kLabelPage + i, PageMarker(i + 1));

    const int64_t result = op == 1 ? Particle()->CParticle::Prepare() : Particle()->CParticle::PlotParticles();
    put(particle->mF04);
    put(particle->mF08);
    for (int32_t i = 0; i < CParticle::kEmitters; ++i) put(particle->mClip[i]);
    return result;
}

// ---- The rain world --------------------------------------------------------------------------------
// The description: the number of words to set in the object, then pairs (Mac offset, word) (the three
// texture pointers, 0x3348, 0x334c and 0x3350, are entry numbers 0..3); three entries (flags, page, U
// offset, V offset, U size, V size); the display's three words that the frame reads (none); the system's flags;
// the view matrix and the inverse view matrix (16 words each).
//   op 1: Prepare()   op 2: Render()   op 3: RenderLightning2(&from, &to)
// out: every word of the object in the Mac's layout (a texture pointer as its entry's number). Answers what
// the function answers.
int64_t tpw_fs_rain(int32_t op, const int64_t* description, int64_t* out, int32_t capacity) {
    begin_output(out, capacity);
    Reader in{description, 0};
    std::memset(gRainStorage, 0xa5, sizeof gRainStorage);
    CRain* rain = Rain();
    const int32_t words = in.i32();
    for (int32_t i = 0; i < words; ++i) {
        const int32_t offset = in.i32();
        const uint32_t word = in.u32();
        if (offset == 0x3348) {
            rain->mSnowTexture = RainEntryAt(static_cast<int32_t>(word));
        } else if (offset == 0x334c) {
            rain->mLightningTexture = RainEntryAt(static_cast<int32_t>(word));
        } else if (offset == 0x3350) {
            rain->mRainTexture = RainEntryAt(static_cast<int32_t>(word));
        } else {
            std::memcpy(RainWord(offset), &word, 4);
        }
    }
    for (int32_t i = 0; i < kRainEntries; ++i) {
        CTextureEntry& entry = gRainEntries[i];
        std::memset(static_cast<void*>(&entry), 0xa5, sizeof entry);
        entry.mFlags = in.u32();
        entry.mTexture = PageMarker(in.i32());
        entry.mUOffset = in.f32();
        entry.mVOffset = in.f32();
        entry.mUSize = in.f32();
        entry.mVSize = in.f32();
    }
    gei_System.mFlags = in.u32();
    uint32_t matrix[16];
    for (uint32_t& word : matrix) word = in.u32();
    std::memcpy(gViewMatrixStorage, matrix, 64);
    for (uint32_t& word : matrix) word = in.u32();
    std::memcpy(gInverseMatrixStorage, matrix, 64);
    gReadClock = FakeReadClock;
    gIsFirstPerson = FakeIsFirstPerson;

    tpw_cl_label(kLabelRain, rain);
    tpw_cl_label(kLabelDraw, static_cast<CDrawPrimitive*>(&gDraw));
    tpw_cl_label(kLabelTCache, gei_TCache);
    tpw_cl_label(kLabelFrom, &gFrom);
    tpw_cl_label(kLabelTo, &gTo);
    tpw_cl_label(kLabelView, gei_ViewMatrix);
    tpw_cl_label(kLabelInverse, gei_InvViewMatrix);
    for (int32_t i = 0; i < kRainEntries; ++i) tpw_cl_label(kLabelEntry + i, &gRainEntries[i]);
    for (int32_t i = 0; i < 8; ++i) tpw_cl_label(kLabelPage + i, PageMarker(i + 1));

    int64_t result = 0;
    if (op == 1) {
        result = rain->Prepare();
    } else if (op == 2) {
        result = rain->Render();
    } else {
        result = rain->RenderLightning2(&gFrom, &gTo);
    }
    for (int32_t offset = 0; offset < kRainBytes; offset += 4) {
        if (offset == 0x3348) {
            put(lab(rain->mSnowTexture));
        } else if (offset == 0x334c) {
            put(lab(rain->mLightningTexture));
        } else if (offset == 0x3350) {
            put(lab(rain->mRainTexture));
        } else {
            uint32_t word;
            std::memcpy(&word, RainWord(offset), 4);
            put(word);
        }
    }
    return result;
}

}  // extern "C"
