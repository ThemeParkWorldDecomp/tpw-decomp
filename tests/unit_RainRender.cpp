// Test entry point for the weather's drawing and the draw primitive's polygon adder and sprite
// (library unit_RainRender: src/tb_drawprimitive_sprite.cpp, src/tb_rain_render.cpp, and the
// real clipper src/tb_clipping.cpp and src/tb_clipping_edges.cpp): CDrawPrimitive::AddPoly_TLCTEX
// and DrawSprite, CRain::__LightningFlash, __RenderLightning, __RenderRain and __RenderSnow, and
// GetCycleCount, CRain::AddLightning and SLightning::Init (src/tb_rain_lightning.cpp).
//
// tests/test_RainRender.py builds one world twice from one description, in emulator memory for the
// original and through tpw_rr_run for the translation, runs one function on both and compares the
// answer, the recorded calls and the state of everything the call could change.
//
// The clipper (CPP_TestData, ClipPoly and the edge clippers) is real on both sides, and so are
// AddPoly_TLCTEX and DrawSprite when the weather draws (the weather's tests see the vertices and
// indices that really land in the primitive's memory). Fakes that record their arguments (floats by
// their bits, a NaN as one value): the primitive buffer (GetNextPrimitive, both ExecuteCurrentPrimitive,
// ProjectCurrentPrimitive, the render memory's SaveInfo), the light manager's CalcVertexFogIntensities,
// the math unit (the strided Transform moves words round; Project answers from a queue),
// CRain::RenderLightning2, the platform timer, rand and the assertion handler.
//
// Pointers into the world are recorded as a place: the region's number in the high half and the byte
// offset in the low half, so the PowerPC side (which has its own addresses) records the same numbers.
#include <cstdint>
#include <cstring>
#include <new>

#include "MapWho.h"
#include "calllog.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "tb_clipping.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rain.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

// ---- The world's sizes (tests/test_RainRender.py has the same numbers) ----------------------------------
constexpr int32_t kRecords = 4;                // primitive records
constexpr int32_t kVertexBytes = 0x6000;       // the vertex memory
constexpr int32_t kIndexEntries = 4096;        // the index memory
constexpr int32_t kInputWords = 512;           // the polygon's and the sprite's input vertices
constexpr int32_t kRainBytes = 0xa1cc;         // the Mac's CRain; the host's is 12 bytes longer after 0x3354
constexpr int32_t kEntries = 3;                // texture entries the rain can point at
constexpr int32_t kMarkers = 8;                // page markers (opaque pointers, never dereferenced)
constexpr int32_t kTransformWords = 6144;      // the most words of the math unit's input that are kept
constexpr uint32_t kNoOffset = 0x80000000u;    // a pointer field that is NULL

enum Region : int64_t { kVertices = 1, kIndices = 2, kInput = 3 };
enum Label : int32_t {
    kLabelDraw = 1,
    kLabelMath = 2,
    kLabelRender = 3,
    kLabelLight = 4,
    kLabelRain = 5,
    kLabelMarker = 11,   // 11 .. 18
    kLabelEntry = 21,    // 21 .. 23
};

// A NaN (as float bits) as one value: its sign and payload depend on the processor.
uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t CanonBits(float value) { return Canon(fbits(value)); }

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// ---- The world ------------------------------------------------------------------------------------------
SPrimitive gRecords[kRecords];
alignas(16) uint8_t gVertices[kVertexBytes];
uint16_t gIndices[kIndexEntries];
uint32_t gInput[kInputWords];
alignas(CRain) unsigned char gRainStorage[sizeof(CRain)];
alignas(CTextureEntry) unsigned char gEntryStorage[kEntries][sizeof(CTextureEntry)];
char gMarkers[kMarkers];
SSpriteDrawParameters gSprite;
uint32_t gTransformInput[kTransformWords];
int32_t gTransformWords;
uint32_t gRandState;
int32_t gRandCalls;

CRain* Rain() { return reinterpret_cast<CRain*>(gRainStorage); }
CTextureEntry* Entry(int32_t number) {
    return number > 0 ? reinterpret_cast<CTextureEntry*>(gEntryStorage[number - 1]) : nullptr;
}
CTextureBase* Marker(int32_t number) {
    return number > 0 ? reinterpret_cast<CTextureBase*>(&gMarkers[number - 1]) : nullptr;
}
int32_t MarkerNumber(const void* pointer) {
    for (int32_t i = 0; i < kMarkers; ++i) {
        if (pointer == &gMarkers[i]) return i + 1;
    }
    return pointer == nullptr ? 0 : -1;
}

// The host address of the word at Mac offset `offset` of the rain object (the three texture
// pointers, 4 bytes each on the Mac, are 8 here).
unsigned char* RainWord(int32_t offset) { return gRainStorage + (offset < 0x3354 ? offset : offset + 12); }

SPrimitive* Record(int64_t number) {
    return number >= 1 && number <= kRecords ? &gRecords[number - 1] : nullptr;
}
int64_t RecordId(const void* record) {
    if (record == nullptr) return 0;
    for (int32_t i = 0; i < kRecords; ++i) {
        if (record == &gRecords[i]) return i + 1;
    }
    return -1;
}

const uint8_t* Base(Region region) {
    switch (region) {
    case kVertices: return gVertices;
    case kIndices: return reinterpret_cast<const uint8_t*>(gIndices);
    default: return reinterpret_cast<const uint8_t*>(gInput);
    }
}
int64_t Place(Region region, const void* pointer) {
    if (pointer == nullptr) return 0;
    return (static_cast<int64_t>(region) << 32) + (static_cast<const uint8_t*>(pointer) - Base(region));
}
uint32_t Offset(Region region, const void* pointer) {
    if (pointer == nullptr) return kNoOffset;
    return static_cast<uint32_t>(static_cast<const uint8_t*>(pointer) - Base(region));
}
uint8_t* VertexPointer(uint32_t offset) { return offset == kNoOffset ? nullptr : gVertices + static_cast<int32_t>(offset); }
uint16_t* IndexPointer(uint32_t offset) {
    if (offset == kNoOffset) return nullptr;
    return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(gIndices) + static_cast<int32_t>(offset));
}

// The bytes of a vertex of the format GetNextPrimitive was given (the real one asserts on an unknown one).
int32_t VertexSizeOf(uint32_t format) {
    switch (format) {
    case 0xc4: return 0x18;
    case 0x1c4: return 0x20;
    case 0x2c4: return 0x28;
    case 0x3c4: return 0x30;
    }
    return 0;
}

// The test's draw primitive: the buffer's members are fakes below, the rest is empty.
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
alignas(TestDraw) unsigned char gDrawStorage[sizeof(TestDraw)];
TestDraw* Draw() { return reinterpret_cast<TestDraw*>(gDrawStorage); }

// The math unit: the strided Transform keeps its input words for the state and turns each triple
// round (a permutation and sign flips chosen by the queue "TransformMode", one number per call), so
// what the weather did to its points shows in the vertices; Project answers from the queue.
class FakeMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix* matrix, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                   int32_t type) override {
        const uint32_t* m = reinterpret_cast<const uint32_t*>(matrix);
        ev("Transform", {lab(this), matrix == reinterpret_cast<sMatrix*>(Rain()->mMatrix), out == in, outStride, inStride,
                         count, type});
        ev("TransformMatrix", {Canon(m[0]), Canon(m[1]), Canon(m[2]), Canon(m[3]), Canon(m[4]), Canon(m[5]), Canon(m[6]),
                               Canon(m[7])});
        ev("TransformMatrix", {Canon(m[8]), Canon(m[9]), Canon(m[10]), Canon(m[11]), Canon(m[12]), Canon(m[13]),
                               Canon(m[14]), Canon(m[15])});
        const int64_t mode = ret("TransformMode", 0);
        const int32_t permutation = static_cast<int32_t>(mode % 6);
        const uint32_t flips = static_cast<uint32_t>(mode / 6) % 8;
        static const int kOrder[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {0, 2, 1}, {2, 1, 0}, {1, 0, 2}};
        gTransformWords = 0;
        for (int32_t i = 0; i < count; ++i) {
            uint32_t w[3];
            std::memcpy(w, static_cast<uint8_t*>(in) + static_cast<int64_t>(i) * inStride, 12);
            for (int32_t k = 0; k < 3; ++k) {
                if (gTransformWords < kTransformWords) gTransformInput[gTransformWords++] = Canon(w[k]);
            }
            uint32_t r[3];
            for (int32_t k = 0; k < 3; ++k) {
                r[k] = w[kOrder[permutation][k]];
                if ((flips >> k) & 1) r[k] ^= 0x80000000u;
            }
            std::memcpy(static_cast<uint8_t*>(out) + static_cast<int64_t>(i) * outStride, r, 12);
        }
    }
    float Project(void* vertices, int32_t stride, int32_t count, int32_t type) override {
        ev("Project", {lab(this), Place(kVertices, vertices), stride, count, type});
        return FromBits(static_cast<uint32_t>(ret("Project", 0)));
    }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
alignas(FakeMath) unsigned char gMathStorage[sizeof(FakeMath)];
FakeMath* Math() { return reinterpret_cast<FakeMath*>(gMathStorage); }

CRenderMemory gRender;
CLightManager gLight;
CClipping gClipping;

}  // namespace

// The classes' constructors and destructors that other translation units define.
CTransformBase::~CTransformBase() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CDisplay::CDisplay() {}
CRenderMemory::CRenderMemory() {}
CLightManager::CLightManager() {}

CDisplay gei_Display;
CDrawPrimitive* gei_DrawPrimitive = nullptr;
CRenderMemory* gei_RenderMemory = &gRender;
CLightManager* gei_LightManager = &gLight;
CClipping* gei_Clipping = &gClipping;
CTransformBase* gei_MathUnit = nullptr;

// Link-only dependencies of the bolt renderer appended to w64's shared source file.
// This module fakes RenderLightning2, so none of these are reached by its tests.
CMatrix* gei_FinalMatrix = nullptr;
int32_t Random() {
    ev("UnexpectedRandom");
    return static_cast<int32_t>(ret("Random", 511));
}
void CMatrix::TransformVertex(void*, void*, int32_t type) {
    ev("UnexpectedTransformVertex", {lab(this), type});
}

void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}

// A reproducible rand: a 32-bit linear congruence, 15 bits from its top half. The state and the number
// of calls are part of the state the test compares.
int32_t msl_rand() {
    ++gRandCalls;
    gRandState = gRandState * 1664525u + 1013904223u;
    return static_cast<int32_t>((gRandState >> 16) & 0x7fff);
}

// The platform timer: the cycle counter comes from the queue "Cycles" (64 bits).
uint64_t SamsUtilities::UTimer::GetAbsoluteCycles() {
    ev("GetAbsoluteCycles");
    return static_cast<uint64_t>(ret("Cycles", 0));
}

// The primitive buffer. GetNextPrimitive hands out the record the queue names (a record number, 1 by
// default, 0 for none) and fills its header in as the real one does; the others record and answer.
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, lab(texture)});
    mCurrent = Record(ret("GetNextPrimitive", 1));
    if (mCurrent == nullptr) return nullptr;
    mCurrent->mBlendingMode = static_cast<uint32_t>(mode);
    mCurrent->mPrimitiveType = primitiveType;
    mCurrent->mVertexFormat = vertexFormat;
    mCurrent->mTexture = texture;
    mCurrent->mF34 = -1;
    SetVertexSize(VertexSizeOf(vertexFormat));
    return mCurrent;
}

int32_t CDrawPrimitive::ExecuteCurrentPrimitive() {
    ev("ExecuteCurrentPrimitive", {lab(this)});
    return static_cast<int32_t>(ret("Execute", 1));
}

int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    ev("ExecuteCurrentPrimitiveDepth", {lab(this), CanonBits(depth)});
    return static_cast<int32_t>(ret("ExecuteDepth", 1));
}

void CDrawPrimitive::ProjectCurrentPrimitive(int32_t type) { ev("ProjectCurrentPrimitive", {lab(this), type}); }

void CRenderMemory::SaveInfo(char* vertexEnd, uint16_t* indexEnd) {
    ev("SaveInfo", {lab(this), Place(kVertices, vertexEnd), Place(kIndices, indexEnd)});
}

// The light manager: whether anything is fogged comes from the queue "Fog".
uint8_t CLightManager::CalcVertexFogIntensities(SFVF_TLCTEX* vertices, int32_t count, int32_t stride) {
    ev("CalcVertexFogIntensities", {lab(this), Place(kVertices, vertices), count, stride});
    return static_cast<uint8_t>(ret("Fog", 0));
}

// RenderLightning2 is another unit's: it records which bolt it was given (the index of the vector in
// the bolt array: 3k is bolt k's start, 3k + 1 its end), the members it reads and the points.
int32_t CRain::RenderLightning2(D3DVECTOR* from, D3DVECTOR* to) {
    const auto where = [this](const D3DVECTOR* p) -> int64_t {
        const uint8_t* b = reinterpret_cast<const uint8_t*>(mLightning);
        const uint8_t* q = reinterpret_cast<const uint8_t*>(p);
        return q >= b && q < b + sizeof mLightning ? (q - b) / 12 : -1;
    };
    ev("RenderLightning2", {lab(this), where(from), where(to), mCurrentLightning, mFA198, mLightningColor, mLightningDepth,
                            mF3308, mDeltaMilliseconds});
    ev("RenderLightning2Points", {CanonBits(from->x), CanonBits(from->y), CanonBits(from->z), CanonBits(to->x),
                                  CanonBits(to->y), CanonBits(to->z)});
    return static_cast<int32_t>(ret("RenderLightning2", 1));
}

namespace {

// A description is a flat list of numbers, read in order.
struct Reader {
    const int64_t* data;
    int32_t at;
    int64_t next() { return data[at++]; }
    int32_t i32() { return static_cast<int32_t>(next()); }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    float f32() { return FromBits(u32()); }
};

// The state is a flat list of words.
uint32_t* gOut;
int32_t gCount;
void Put(uint32_t value) { gOut[gCount++] = value; }

}  // namespace

extern "C" {

// The description, in order:
//   op, eight arguments, the display's flags and aspect, the light manager's colour, the random seed;
//   the draw primitive: its vertex size, the current record's number, the ends of the vertex and index
//   memory (offsets, kNoOffset for NULL);
//   four records of 13 words (flags, blending mode, texture marker, two filler words, type, format,
//   vertex count, index count, vertex data offset, index data offset, depth bits, last field);
//   the fill of the vertex memory (a word) and of the index memory (a halfword);
//   the 512 input words; the sprite (flags, vertices as an offset in the input, colour, blending mode,
//   texture marker, filler, u0, v0, u1, v1, last word); the rain's 10,355 words in the Mac's layout
//   (the texture pointers 0x3348, 0x334c and 0x3350 are entry numbers 0..3); three texture entries
//   (flags, page marker, U offset, V offset, U size, V size).
//   ops: 1 AddPoly_TLCTEX(offset in the input, count, stride, clip given, all, any, flags)
//        2 DrawSprite(mode)   3 __LightningFlash   4 __RenderLightning   5 __RenderRain(x, y, z bits)
//        6 __RenderSnow(x, y, z bits)   7 GetCycleCount
//        8 CRain::AddLightning(start, end, a, b = the eight arguments)
//        9 SLightning::Init on bolt slot 2 (start, end, time, seeded = the eight arguments)
// out: the answer, the draw primitive's four words, the four records, the vertex memory (words), the
// index memory (halfwords, in 2,048 words), the input words, the sprite, the rain object in the Mac's
// layout (a texture pointer as its entry's number), the random state and number of calls, and the words
// the math unit's Transform was given (their number, then 6,144 words). Answers the number of words.
int32_t tpw_rr_run(const int64_t* description, uint32_t* out) {
    gOut = out;
    gCount = 0;
    Reader in{description, 0};
    const int32_t op = in.i32();
    int64_t a[8];
    for (int64_t& argument : a) argument = in.next();

    // The globals the functions read.
    std::memset(gMathStorage, 0, sizeof gMathStorage);
    std::memset(gDrawStorage, 0, sizeof gDrawStorage);
    gei_Display.mFlags = in.u32();
    gei_Display.mF04 = in.f32();
    gLight.mF00 = 0;
    gLight.mF04 = in.i32();
    gRandState = in.u32();
    gRandCalls = 0;
    new (gMathStorage) FakeMath;
    gei_MathUnit = Math();

    // The draw primitive.
    new (gDrawStorage) TestDraw;
    gei_DrawPrimitive = Draw();
    Draw()->SetVertexSize(in.i32());
    const int32_t current = in.i32();
    const uint32_t vertexEnd = in.u32();
    const uint32_t indexEnd = in.u32();

    // The records.
    std::memset(gRecords, 0xa5, sizeof gRecords);
    for (int32_t k = 0; k < kRecords; ++k) {
        SPrimitive* p = &gRecords[k];
        p->mFlags = in.u32();
        p->mBlendingMode = in.u32();
        p->mTexture = Marker(in.i32());
        uint32_t filler[2] = {in.u32(), in.u32()};
        std::memcpy(p->mF0C, filler, 8);
        p->mPrimitiveType = in.i32();
        p->mVertexFormat = in.u32();
        p->mVertexCount = in.i32();
        p->mIndexCount = in.i32();
        p->mVertexData = VertexPointer(in.u32());
        p->mIndexData = IndexPointer(in.u32());
        const uint32_t depth = in.u32();
        std::memcpy(&p->mDepth, &depth, 4);
        p->mF34 = in.i32();
        p->mTexture1 = nullptr;
        p->mTexture2 = nullptr;
    }
    Draw()->mCurrent = Record(current);
    Draw()->mVertexEnd = VertexPointer(vertexEnd);
    Draw()->mIndexEnd = IndexPointer(indexEnd);

    // The memory areas.
    const uint32_t vertexFill = in.u32();
    for (int32_t i = 0; i < kVertexBytes / 4; ++i) std::memcpy(gVertices + 4 * i, &vertexFill, 4);
    const uint16_t indexFill = static_cast<uint16_t>(in.u32());
    for (uint16_t& entry : gIndices) entry = indexFill;
    for (uint32_t& word : gInput) word = in.u32();

    // The sprite.
    gSprite.mFlags = in.u32();
    const uint32_t spriteVertices = in.u32();
    gSprite.mVertices = spriteVertices == kNoOffset ? nullptr : reinterpret_cast<uint8_t*>(gInput) + spriteVertices;
    gSprite.mF08 = in.i32();
    gSprite.mBlendingMode = in.u32();
    gSprite.mTexture = Marker(in.i32());
    gSprite.mF14 = in.u32();
    gSprite.mU0 = in.f32();
    gSprite.mV0 = in.f32();
    gSprite.mU1 = in.f32();
    gSprite.mV1 = in.f32();
    gSprite.mF28 = in.i32();

    // The rain and its three texture entries.
    std::memset(gRainStorage, 0xa5, sizeof gRainStorage);
    CRain* rain = Rain();
    for (int32_t offset = 0; offset < kRainBytes; offset += 4) {
        const uint32_t word = in.u32();
        if (offset == 0x3348) {
            rain->mSnowTexture = Entry(static_cast<int32_t>(word));
        } else if (offset == 0x334c) {
            rain->mLightningTexture = Entry(static_cast<int32_t>(word));
        } else if (offset == 0x3350) {
            rain->mRainTexture = Entry(static_cast<int32_t>(word));
        } else {
            std::memcpy(RainWord(offset), &word, 4);
        }
    }
    for (int32_t k = 0; k < kEntries; ++k) {
        CTextureEntry* entry = Entry(k + 1);
        std::memset(static_cast<void*>(entry), 0xa5, sizeof(CTextureEntry));
        entry->mFlags = in.u32();
        entry->mTexture = Marker(in.i32());
        entry->mUOffset = in.f32();
        entry->mVOffset = in.f32();
        entry->mUSize = in.f32();
        entry->mVSize = in.f32();
    }
    gTransformWords = 0;
    std::memset(gTransformInput, 0, sizeof gTransformInput);

    tpw_cl_label(kLabelDraw, static_cast<CDrawPrimitive*>(Draw()));
    tpw_cl_label(kLabelMath, Math());
    tpw_cl_label(kLabelRender, &gRender);
    tpw_cl_label(kLabelLight, &gLight);
    tpw_cl_label(kLabelRain, rain);
    for (int32_t i = 0; i < kMarkers; ++i) tpw_cl_label(kLabelMarker + i, &gMarkers[i]);
    for (int32_t i = 0; i < kEntries; ++i) tpw_cl_label(kLabelEntry + i, Entry(i + 1));

    int64_t result = 0;
    ClipCodeType clip = {static_cast<uint8_t>(a[4]), static_cast<uint8_t>(a[5])};
    switch (op) {
    case 1: {
        SFVF_TLCTEX* vertices = reinterpret_cast<SFVF_TLCTEX*>(reinterpret_cast<uint8_t*>(gInput) + a[0]);
        result = gei_DrawPrimitive->AddPoly_TLCTEX(vertices, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                                                   a[3] != 0 ? &clip : nullptr, static_cast<int32_t>(a[6]));
        break;
    }
    case 2:
        result = gei_DrawPrimitive->DrawSprite(&gSprite, static_cast<int32_t>(a[0]));
        break;
    case 3:
        result = rain->__LightningFlash();
        break;
    case 4:
        result = rain->__RenderLightning();
        break;
    case 5:
        result = rain->__RenderRain(FromBits(static_cast<uint32_t>(a[0])), FromBits(static_cast<uint32_t>(a[1])),
                                    FromBits(static_cast<uint32_t>(a[2])));
        break;
    case 6:
        result = rain->__RenderSnow(FromBits(static_cast<uint32_t>(a[0])), FromBits(static_cast<uint32_t>(a[1])),
                                    FromBits(static_cast<uint32_t>(a[2])));
        break;
    case 8:
        result = rain->AddLightning({FromBits(static_cast<uint32_t>(a[0])), FromBits(static_cast<uint32_t>(a[1])),
                                     FromBits(static_cast<uint32_t>(a[2]))},
                                    {FromBits(static_cast<uint32_t>(a[3])), FromBits(static_cast<uint32_t>(a[4])),
                                     FromBits(static_cast<uint32_t>(a[5]))},
                                    static_cast<int32_t>(a[6]), static_cast<int32_t>(a[7]));
        break;
    case 9:
        rain->mLightning[2].Init({FromBits(static_cast<uint32_t>(a[0])), FromBits(static_cast<uint32_t>(a[1])),
                                  FromBits(static_cast<uint32_t>(a[2]))},
                                 {FromBits(static_cast<uint32_t>(a[3])), FromBits(static_cast<uint32_t>(a[4])),
                                  FromBits(static_cast<uint32_t>(a[5]))},
                                 static_cast<int32_t>(a[6]), static_cast<int32_t>(a[7]));
        break;
    default:
        result = GetCycleCount();
        break;
    }

    // The state.
    Put(static_cast<uint32_t>(result));
    Put(static_cast<uint32_t>(Draw()->VertexSize()));
    Put(static_cast<uint32_t>(RecordId(Draw()->mCurrent)));
    Put(Offset(kVertices, Draw()->mVertexEnd));
    Put(Offset(kIndices, Draw()->mIndexEnd));
    for (int32_t k = 0; k < kRecords; ++k) {
        const SPrimitive* p = &gRecords[k];
        uint32_t filler[2];
        std::memcpy(filler, p->mF0C, 8);
        Put(p->mFlags);
        Put(p->mBlendingMode);
        Put(static_cast<uint32_t>(MarkerNumber(p->mTexture)));
        Put(filler[0]);
        Put(filler[1]);
        Put(static_cast<uint32_t>(p->mPrimitiveType));
        Put(p->mVertexFormat);
        Put(static_cast<uint32_t>(p->mVertexCount));
        Put(static_cast<uint32_t>(p->mIndexCount));
        Put(Offset(kVertices, p->mVertexData));
        Put(Offset(kIndices, p->mIndexData));
        Put(CanonBits(p->mDepth));
        Put(static_cast<uint32_t>(p->mF34));
    }
    for (int32_t i = 0; i < kVertexBytes / 4; ++i) {
        uint32_t word;
        std::memcpy(&word, gVertices + 4 * i, 4);
        Put(Canon(word));
    }
    for (int32_t i = 0; i < kIndexEntries; i += 2) Put(static_cast<uint32_t>(gIndices[i]) | (static_cast<uint32_t>(gIndices[i + 1]) << 16));
    for (uint32_t word : gInput) Put(Canon(word));
    Put(gSprite.mFlags);
    Put(Offset(kInput, gSprite.mVertices));
    Put(static_cast<uint32_t>(gSprite.mF08));
    Put(gSprite.mBlendingMode);
    Put(static_cast<uint32_t>(MarkerNumber(gSprite.mTexture)));
    Put(gSprite.mF14);
    Put(CanonBits(gSprite.mU0));
    Put(CanonBits(gSprite.mV0));
    Put(CanonBits(gSprite.mU1));
    Put(CanonBits(gSprite.mV1));
    Put(static_cast<uint32_t>(gSprite.mF28));
    for (int32_t offset = 0; offset < kRainBytes; offset += 4) {
        if (offset == 0x3348) {
            Put(static_cast<uint32_t>(lab(rain->mSnowTexture)));
        } else if (offset == 0x334c) {
            Put(static_cast<uint32_t>(lab(rain->mLightningTexture)));
        } else if (offset == 0x3350) {
            Put(static_cast<uint32_t>(lab(rain->mRainTexture)));
        } else {
            uint32_t word;
            std::memcpy(&word, RainWord(offset), 4);
            // A bolt's nine float words: a NaN as one value (its sign and payload depend on the processor).
            const bool boltFloat = offset >= 0x3008 && offset < 0x3308 && (offset - 0x3008) % 0x30 < 0x24;
            Put(boltFloat ? Canon(word) : word);
        }
    }
    Put(gRandState);
    Put(static_cast<uint32_t>(gRandCalls));
    Put(static_cast<uint32_t>(gTransformWords));
    for (int32_t i = 0; i < kTransformWords; ++i) Put(gTransformInput[i]);
    return gCount;
}

}  // extern "C"
