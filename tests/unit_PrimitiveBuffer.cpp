// Fakes and driver for the draw primitive buffer unit (src/tb_drawprimitive_buffer.cpp).
//
// Everything the unit calls is a fake that records its arguments through tests/calllog.h:
// the render memory (LoadInfo and the rest), the clipper, the light manager, the math
// unit, the face sorter, the assertion, and the renderer's Execute (a method of the test's
// own CDrawPrimitive). Pointers into the test's memory are recorded as a place: the
// region's number in the high half and the byte offset from its start in the low half, so
// the PowerPC side (which has its own addresses) records the same numbers.
//
// The test owns a table of render memory records (the Mac's 0x38 bytes become 0x40 here),
// a vertex area, an index area and a list of input indices. Case data comes in through the
// tpw_pb_* functions; tests/test_PrimitiveBuffer.py builds the same memory on the PowerPC
// side and compares the call logs, the results and the state dumped after each operation.
#include "MapWho.h"
#include "calllog.h"
#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include <cstdint>
#include <cstring>
#include <new>

using namespace calllog;

namespace {

enum Region : int64_t { kVertices = 1, kIndices = 2, kInput = 3 };
enum {
    kRecords = 40,         // records in the table
    kDumped = 8,           // the records the state dump covers
    kScratchRecord = 8,    // index of the record LoadInfo(6) is made to return
    kVertexBytes = 0x2800,
    kIndexEntries = 768,
    kInputEntries = 128,
};
constexpr uint32_t kNoOffset = 0x80000000u;   // a pointer field that is NULL
constexpr uint64_t kHashStart = 0xcbf29ce484222325ull;

SRenderLoadInfo gRecords[kRecords];
alignas(16) uint8_t gVertices[kVertexBytes];
uint16_t gIndices[kIndexEntries];
uint16_t gInput[kInputEntries];
int64_t gStream[4096];   // numbers for the clipper's fakes, in the order they are read
int32_t gStreamCount;
int32_t gStreamNext;
int32_t gVertexCount;    // AddClipPolyList's by-reference count

// Record number (1-based) as a primitive, NULL for any other number.
SPrimitive* Record(int64_t number) {
    if (number < 1 || number > kRecords) return nullptr;
    return reinterpret_cast<SPrimitive*>(&gRecords[number - 1]);
}

// 1 + the record's index, 0 for NULL, -1 for anything else.
int64_t RecordId(const void* record) {
    if (record == nullptr) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(record);
    const uint8_t* base = reinterpret_cast<const uint8_t*>(gRecords);
    if (p < base || p >= base + sizeof gRecords) return -1;
    if ((p - base) % sizeof(SRenderLoadInfo) != 0) return -1;
    return (p - base) / sizeof(SRenderLoadInfo) + 1;
}

const uint8_t* Base(Region region) {
    switch (region) {
    case kVertices: return gVertices;
    case kIndices: return reinterpret_cast<const uint8_t*>(gIndices);
    default: return reinterpret_cast<const uint8_t*>(gInput);
    }
}

// Region number and offset of a pointer into one of the test's areas; 0 for NULL.
int64_t Place(Region region, const void* pointer) {
    if (pointer == nullptr) return 0;
    return (static_cast<int64_t>(region) << 32) + (static_cast<const uint8_t*>(pointer) - Base(region));
}

// A pointer field as a 32-bit offset in the area (kNoOffset for NULL), and back.
uint32_t Offset(Region region, const void* pointer) {
    if (pointer == nullptr) return kNoOffset;
    return static_cast<uint32_t>(static_cast<const uint8_t*>(pointer) - Base(region));
}
uint8_t* VertexPointer(uint32_t offset) { return offset == kNoOffset ? nullptr : gVertices + static_cast<int32_t>(offset); }
uint16_t* IndexPointer(uint32_t offset) {
    if (offset == kNoOffset) return nullptr;
    return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(gIndices) + static_cast<int32_t>(offset));
}

// The low 32 bits of a pointer-sized opaque value (a texture).
int64_t Low32(const void* pointer) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu); }

int64_t Stream() { return gStreamNext < gStreamCount ? gStream[gStreamNext++] : 0; }

// A NaN as one value, since a PowerPC add and a host add may leave different payloads.
uint32_t Canon(uint32_t w) { return (w & 0x7f800000) == 0x7f800000 && (w & 0x007fffff) != 0 ? 0x7fc00000 : w; }
uint32_t CanonBits(float f) { return Canon(fbits(f)); }

uint64_t Hash(uint64_t h, uint32_t w) { return (h ^ w) * 0x100000001b3ull; }
int64_t Positive(uint64_t h) { return static_cast<int64_t>(h & 0x7fffffffffffffffull); }

// The test's primitive object: Execute records the whole record and answers from the queue
// (and the queue "ExecuteMove" may replace the current primitive, as a real renderer could).
class TestPrimitive : public CDrawPrimitive {
public:
    int32_t Init() override { return 1; }
    int32_t SetBlendingMode(int32_t) override { return 1; }
    int32_t SetTexture(int32_t, void*) override { return 1; }
    int32_t Execute(SPrimitive* p) override {
        ev("Execute", {RecordId(p), p->mFlags, p->mBlendingMode, Low32(p->mTexture), p->mPrimitiveType, p->mVertexFormat,
                       p->mVertexCount, p->mIndexCount, Place(kVertices, p->mVertexData), Place(kIndices, p->mIndexData),
                       CanonBits(p->mDepth), p->mF34});
        const int64_t move = ret("ExecuteMove");
        if (move != 0) mCurrent = Record(move);
        return static_cast<int32_t>(ret("Execute", 1));
    }
    int32_t ExecuteShadow(SPrimitive*) override { return 1; }
    int32_t SetFogParameters(float, float, uint32_t) override { return 1; }
    int32_t BeginDraw() override { return 1; }
    int32_t EndDraw() override { return 1; }
    int32_t DrawTestPoly() override { return 1; }
    int32_t DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) override { return 1; }
};

alignas(TestPrimitive) unsigned char gStorage[sizeof(TestPrimitive)];
TestPrimitive* gPrimitive;

// The math unit: Project answers from the queue (and may change the current record's depth
// first, to show when the buffer reads it).
class FakeMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix*, void*, void*, int32_t, int32_t, int32_t, int32_t) override {}
    float Project(void* vertices, int32_t stride, int32_t count, int32_t type) override {
        ev("Project", {lab(this), Place(kVertices, vertices), stride, count, type});
        const int64_t poke = ret("ProjectPoke");
        if (poke != 0) {
            const uint32_t word = static_cast<uint32_t>(poke);
            std::memcpy(&gPrimitive->mCurrent->mDepth, &word, 4);
        }
        const uint32_t word = static_cast<uint32_t>(ret("Project"));
        float result;
        std::memcpy(&result, &word, 4);
        return result;
    }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMath gMath;
CRenderMemory gRender;
CLightManager gLight;
CClipping gClipping;

}  // namespace

CTransformBase::~CTransformBase() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CSystem::CSystem() {}
CDisplay::CDisplay() {}
CRenderMemory::CRenderMemory() {}
CLightManager::CLightManager() {}
CSystem gei_System;
CDisplay gei_Display;
CRenderMemory* gei_RenderMemory = &gRender;
CLightManager* gei_LightManager = &gLight;
CClipping* gei_Clipping = &gClipping;
CTransformBase* gei_MathUnit = &gMath;

void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}

// The render memory: the record it returns comes from the queue (a record number, 0 for none).
SRenderLoadInfo* CRenderMemory::LoadInfo(int32_t flags) {
    ev("LoadInfo", {lab(this), flags});
    return reinterpret_cast<SRenderLoadInfo*>(Record(ret("LoadInfo")));
}

SRenderLoadInfo* CRenderMemory::LoadInfo_CopyVDB(int32_t flags) {
    ev("LoadInfo_CopyVDB", {lab(this), flags});
    return reinterpret_cast<SRenderLoadInfo*>(Record(ret("CopyVDB")));
}

void CRenderMemory::SaveInfo(char* vertexEnd, uint16_t* indexEnd) {
    ev("SaveInfo", {lab(this), Place(kVertices, vertexEnd), Place(kIndices, indexEnd)});
}

uint8_t* CRenderMemory::GetVertexEndPosition() {
    ev("GetVertexEndPosition", {lab(this)});
    return gVertices + ret("VertexEnd");
}

uint16_t* CRenderMemory::GetIndexEndPosition() {
    ev("GetIndexEndPosition", {lab(this)});
    return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(gIndices) + ret("IndexEnd"));
}

// The face sorter: records the keys (hashed, and the first five) and the index triples it
// was given, for at most 100 faces.
void QSortFaces_float(float* keys, uint16_t* indices, int32_t low, int32_t high) {
    const int32_t n = high + 1 > 0 && high + 1 <= 100 ? high + 1 : 0;
    uint64_t hashKeys = kHashStart;
    uint64_t hashIndices = kHashStart;
    uint32_t first[5] = {0, 0, 0, 0, 0};
    for (int32_t i = 0; i < n; ++i) {
        uint32_t w;
        std::memcpy(&w, keys + i, 4);
        w = Canon(w);
        hashKeys = Hash(hashKeys, w);
        if (i < 5) first[i] = w;
        for (int32_t k = 0; k < 3; ++k) hashIndices = Hash(hashIndices, indices[i * 3 + k]);
    }
    const uint8_t* keyRecord = reinterpret_cast<const uint8_t*>(keys) - offsetof(SRenderLoadInfo, mData);
    ev("QSortFaces_float", {RecordId(keyRecord), Place(kIndices, indices), low, high, n, Positive(hashKeys),
                            Positive(hashIndices), first[0], first[1], first[2], first[3], first[4]});
}

// The clipper's test: the outside codes come from the stream, as the real one works them out.
void CClipping::CPP_TestData(uint8_t* result, void* clipping, uint8_t* codes, const void* vertices, int32_t count,
                             int32_t stride, uint32_t mask) {
    ev("CPP_TestData",
       {lab(clipping), result != nullptr, codes != nullptr, Place(kVertices, vertices), count, stride, mask});
    uint32_t all = 0xff;
    uint32_t any = 0;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t code = static_cast<uint32_t>(Stream()) & mask;
        codes[i] = static_cast<uint8_t>(code);
        all &= code;
        any |= code;
    }
    result[0] = static_cast<uint8_t>(all);
    result[1] = static_cast<uint8_t>(any);
}

// The clip: the polygon (its vertex count, then the numbers), and how many vertices it adds,
// come from the stream.
void CClipping::ClipPoly(int32_t planes, SFVF_TLCTEX* vertices, VertexClipList* list, int32_t& count, int32_t stride) {
    ev("ClipPoly", {lab(this), planes, Place(kVertices, vertices), list->mCount, Place(kIndices, list->mList),
                    list->mList[0], list->mList[1], list->mList[2], count, stride});
    const int32_t n = static_cast<int32_t>(Stream());
    const int32_t added = static_cast<int32_t>(Stream());
    for (int32_t i = 0; i < n; ++i) list->mList[i] = static_cast<uint16_t>(Stream());
    list->mCount = n;
    count += added;
}

// The light manager: whether anything is fogged comes from the queue.
uint8_t CLightManager::CalcVertexFogIntensities(SFVF_TLCTEX* vertices, int32_t count, int32_t stride) {
    ev("CalcVertexFogIntensities", {lab(this), Place(kVertices, vertices), count, stride});
    return static_cast<uint8_t>(ret("Fog"));
}

namespace {

// The words of a record as the PowerPC side lays them out (13 words): flags, blending
// mode, texture, the two filler words, type, format, counts, the two data places, the
// depth's bits and the last field.
void DumpRecord(const SPrimitive* p, uint32_t* out) {
    out[0] = p->mFlags;
    out[1] = p->mBlendingMode;
    out[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p->mTexture));
    std::memcpy(out + 3, p->mF0C, 8);
    out[5] = static_cast<uint32_t>(p->mPrimitiveType);
    out[6] = p->mVertexFormat;
    out[7] = static_cast<uint32_t>(p->mVertexCount);
    out[8] = static_cast<uint32_t>(p->mIndexCount);
    out[9] = Offset(kVertices, p->mVertexData);
    out[10] = Offset(kIndices, p->mIndexData);
    out[11] = CanonBits(p->mDepth);
    out[12] = static_cast<uint32_t>(p->mF34);
}

}  // namespace

extern "C" {

// w: the object's words 0..10 (7, 9 and 10 are the current record's number, the vertex end
// and the index end as offsets), then the system flags, the renderer and the display flags.
void tpw_pb_begin(const uint32_t* w) {
    std::memset(gRecords, 0xa5, sizeof gRecords);
    std::memset(gVertices, 0xa5, sizeof gVertices);
    std::memset(gIndices, 0xa5, sizeof gIndices);
    std::memset(gInput, 0xa5, sizeof gInput);
    std::memset(gStorage, 0xa5, sizeof gStorage);
    gPrimitive = new (gStorage) TestPrimitive;
    for (int i = 0; i < 11; ++i) gPrimitive->mFields[i] = w[i];
    gPrimitive->mCurrent = Record(w[7]);
    gPrimitive->mVertexEnd = VertexPointer(w[9]);
    gPrimitive->mIndexEnd = IndexPointer(w[10]);
    gei_System.mFlags = w[11];
    gei_System.mRenderer = static_cast<int32_t>(w[12]);
    gei_Display.mFlags = w[13];
    gVertexCount = 0;
    gStreamCount = 0;
    gStreamNext = 0;
    tpw_cl_label(1, gPrimitive);
    tpw_cl_label(2, &gRender);
    tpw_cl_label(3, &gClipping);
    tpw_cl_label(4, &gLight);
    tpw_cl_label(5, &gMath);
}

// One record of the table from its 13 words (see DumpRecord); data places are offsets.
void tpw_pb_record(int32_t k, const uint32_t* w) {
    SPrimitive* p = Record(k + 1);
    p->mFlags = w[0];
    p->mBlendingMode = w[1];
    p->mTexture = reinterpret_cast<void*>(static_cast<uintptr_t>(w[2]));
    std::memcpy(p->mF0C, w + 3, 8);
    p->mPrimitiveType = static_cast<int32_t>(w[5]);
    p->mVertexFormat = w[6];
    p->mVertexCount = static_cast<int32_t>(w[7]);
    p->mIndexCount = static_cast<int32_t>(w[8]);
    p->mVertexData = VertexPointer(w[9]);
    p->mIndexData = IndexPointer(w[10]);
    std::memcpy(&p->mDepth, w + 11, 4);
    p->mF34 = static_cast<int32_t>(w[12]);
}

// The three memory areas (words of the vertex area, index entries, input entries) and the
// stream of numbers the clipper's fakes read.
void tpw_pb_data(const uint32_t* vertices, const uint16_t* indices, const uint16_t* input, const int64_t* stream,
                 int32_t streamCount) {
    std::memcpy(gVertices, vertices, sizeof gVertices);
    std::memcpy(gIndices, indices, sizeof gIndices);
    std::memcpy(gInput, input, sizeof gInput);
    gStreamCount = streamCount < 4096 ? streamCount : 4096;
    std::memcpy(gStream, stream, sizeof(int64_t) * gStreamCount);
    gStreamNext = 0;
}

// Operations: 1 GetNextPrimitive(flags, type, format, mode, texture), 2 DuplicateCurrent
// (the same), 3 ExternExecute(record), 4 ExecuteCurrentPrimitive(), 5 the float one (bits),
// 6 ExecutePrimitiveIfInstant(record), 7 AddClipPolyList(count, input offset, indices,
// mask), 8 ApplyFog(vertex offset, count), 9 CheckFog, 10 Project(type), 11 RoundOff,
// 12 sets the system flags, renderer and display flags (a[0..2]), 13 the current record's
// vertex and index counts (what the caller does after filling a primitive). Pointers come
// back as record numbers.
int64_t tpw_pb_op(int32_t op, const int32_t* a) {
    TestPrimitive& d = *gPrimitive;
    void* texture = reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(a[4])));
    switch (op) {
    case 1: return RecordId(d.GetNextPrimitive(a[0], a[1], static_cast<uint32_t>(a[2]), a[3], texture));
    case 2: return RecordId(d.DuplicateCurrentPrimitive(a[0], a[1], static_cast<uint32_t>(a[2]), a[3], texture));
    case 3: return d.ExternExecute(Record(a[0]));
    case 4: return d.ExecuteCurrentPrimitive();
    case 5: {
        float depth;
        std::memcpy(&depth, a, 4);
        return d.ExecuteCurrentPrimitive(depth);
    }
    case 6: return d.ExecutePrimitiveIfInstant(Record(a[0]));
    case 7:
        gVertexCount = a[0];
        return d.AddClipPolyList(gVertexCount, gInput + a[1], a[2], a[3]);
    case 8:
        d.ApplyFog(reinterpret_cast<SFVF_TLCTEX*>(gVertices + a[0]), a[1]);
        return 0;
    case 9:
        d.CheckFogOnCurrentPrimitive();
        return 0;
    case 10:
        d.ProjectCurrentPrimitive(a[0]);
        return 0;
    case 11:
        d.RoundOffPrimitive();
        return 0;
    case 12:
        gei_System.mFlags = static_cast<uint32_t>(a[0]);
        gei_System.mRenderer = a[1];
        gei_Display.mFlags = static_cast<uint32_t>(a[2]);
        return 0;
    case 13:
        d.mCurrent->mVertexCount = a[0];
        d.mCurrent->mIndexCount = a[1];
        return 0;
    }
    return -99;
}

// Everything observable after an operation, in the order the PowerPC side builds it: the
// object's 11 words (7, 9 and 10 as the record number and the two offsets), the by-reference
// count, the eight dumped records, 24 words behind the scratch record's offset 0x28, then
// the index area as 16-bit entries and the vertex area as words.
int32_t tpw_pb_state(uint32_t* out) {
    int32_t n = 0;
    const TestPrimitive& d = *gPrimitive;
    for (int i = 0; i < 11; ++i) out[n++] = d.mFields[i];
    out[7] = static_cast<uint32_t>(RecordId(d.mCurrent));
    out[9] = Offset(kVertices, d.mVertexEnd);
    out[10] = Offset(kIndices, d.mIndexEnd);
    out[n++] = static_cast<uint32_t>(gVertexCount);
    for (int k = 0; k < kDumped; ++k) {
        DumpRecord(Record(k + 1), out + n);
        n += 13;
    }
    const uint8_t* scratch = reinterpret_cast<const uint8_t*>(&gRecords[kScratchRecord]) + offsetof(SRenderLoadInfo, mData);
    for (int i = 0; i < 24; ++i) {
        uint32_t w;
        std::memcpy(&w, scratch + 4 * i, 4);
        out[n++] = Canon(w);
    }
    for (int i = 0; i < kIndexEntries; ++i) out[n++] = gIndices[i];
    for (int i = 0; i < kVertexBytes / 4; ++i) {
        uint32_t w;
        std::memcpy(&w, gVertices + 4 * i, 4);
        out[n++] = w;
    }
    return n;
}

}  // extern "C"
