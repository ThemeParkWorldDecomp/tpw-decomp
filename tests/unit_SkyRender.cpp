// Fakes for the sky's per-frame unit (src/tb_sky_render.cpp: CSky::Prepare, RenderAdd and Render).
// The math unit is a real CMatrixPowerPC (src/MatrixPowerPC.cpp is part of the unit, as for
// tests/unit_MeshMatrix.cpp), so the sky's multiplies and vertex transforms are the original's.
// Everything else the sky calls records an event through tests/calllog.h, and the larger things
// (matrices, vertices, primitives) go to a dump of words that the PowerPC side builds the same
// way: floats the translation computed are dumped with every NaN as one value.
//
//  - the draw primitive: GetNextPrimitive hands out the case's records, AddClipPolyList answers
//    from queues, ExecuteCurrentPrimitive dumps the record that was filled, DrawSprite dumps its
//    parameters and corners;
//  - the texture cache, CTextureEntry::Lock and GetBlendingMode, CMatrix::TransformVertex
//    (its four result words come from queues) and RotateVertex_Y (a turn by a quarter turn
//    chosen from the angle) record their arguments.
#include <cstdint>
#include <cstring>
#include <vector>

#include "MapWho.h"
#include "MatrixPowerPC.h"
#include "calllog.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_sky.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

constexpr int32_t kEntries = 24;   // texture entries, labels 1..kEntries
constexpr int32_t kPages = 8;      // texture pages, labels 1..kPages
constexpr int32_t kRecords = 12;   // primitives the draw primitive can hand out

// The sky under test: raw storage, as the unit does not link the sky's constructor.
alignas(CSky) unsigned char gSkyStorage[sizeof(CSky)];
CSky& Sky() { return *reinterpret_cast<CSky*>(gSkyStorage); }

CTextureEntry gEntries[kEntries + 1];
char gPages[kPages + 1];
alignas(CTextureCache) unsigned char gCacheStorage[sizeof(CTextureCache)];
alignas(CRain) unsigned char gRainStorage[sizeof(CRain)];
alignas(CLightManager) unsigned char gLightStorage[sizeof(CLightManager)];
CMatrixPowerPC gUnit;
CMatrix gInverseView;

// The draw primitive object: the sky calls its non-virtual members, which are fakes below.
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

// One primitive the draw primitive can hand out, with its own vertex and index memory.
struct Record {
    uint32_t word14 = 0;   // the Mac's header word at 0x14, which the host's wider texture pointer absorbs
    SPrimitive primitive{};
    std::vector<uint32_t> vertices;
    std::vector<uint16_t> indices;
};
Record gRecords[kRecords];
SPrimitive* gLastReturned = nullptr;
std::vector<uint32_t> gDump;

uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t Cfb(float value) { return Canon(fbits(value)); }
void DumpRaw(uint32_t word) { gDump.push_back(word); }
void DumpFloat(uint32_t word) { gDump.push_back(Canon(word)); }
float Fl(uint32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}
int32_t IndexOf(const SPrimitive* primitive) {
    for (int32_t i = 0; i < kRecords; ++i)
        if (&gRecords[i].primitive == primitive) return i;
    return -1;
}
int32_t EntryLabel(const CTextureEntry* entry) {
    if (entry == nullptr) return 0;
    for (int32_t i = 1; i <= kEntries; ++i)
        if (&gEntries[i] == entry) return i;
    return -1;
}
CTextureEntry* EntryPointer(uint32_t label) { return label == 0 ? nullptr : &gEntries[label]; }

// A primitive as the sky left it: the first ten header words (type, seven the sky never
// writes, vertex count, index count), whether its data pointers are unchanged, and the
// vertex memory (positions and texture coordinates as floats, the two colour words raw).
void DumpRecord(const Record& record) {
    const SPrimitive& p = record.primitive;
    DumpRaw(p.mFlags);
    // The seven words after the type as the Mac lays them out (blending mode, texture, 0x0c,
    // 0x10, 0x14, primitive type, vertex format); the sky writes none of them.
    DumpRaw(p.mBlendingMode);
    DumpRaw(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p.mTexture)));
    uint32_t middle[2];
    std::memcpy(middle, p.mF0C, sizeof middle);
    DumpRaw(middle[0]);
    DumpRaw(middle[1]);
    DumpRaw(record.word14);
    DumpRaw(static_cast<uint32_t>(p.mPrimitiveType));
    DumpRaw(p.mVertexFormat);
    DumpRaw(static_cast<uint32_t>(p.mVertexCount));
    DumpRaw(static_cast<uint32_t>(p.mIndexCount));
    DumpRaw(p.mVertexData == reinterpret_cast<const uint8_t*>(record.vertices.data()) &&
                    p.mIndexData == record.indices.data()
                ? 1
                : 0);
    for (size_t i = 0; i < record.vertices.size(); ++i) {
        uint32_t lane = i % 8;
        if (lane == 4 || lane == 5) {
            DumpRaw(record.vertices[i]);
        } else {
            DumpFloat(record.vertices[i]);
        }
    }
}

}  // namespace

// ---- the objects the sky reaches
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSystem::CSystem() {}
CSystem gei_System;
CRain* gei_Rain = reinterpret_cast<CRain*>(gRainStorage);
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStorage);
CLightManager* gei_LightManager = reinterpret_cast<CLightManager*>(gLightStorage);
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
CTransformBase* gei_MathUnit = &gUnit;
CTransformBase::~CTransformBase() {}
CMatrix gei_ProjectionMatrix;
CMatrix gScreenMatrix;
CMatrix* gei_InvViewMatrix = &gInverseView;
float gViewAngle;
void DebugStr(const unsigned char*) {}
CTextureEntry::CTextureEntry() { std::memset(static_cast<void*>(this), 0, sizeof *this); }
CTextureEntry::~CTextureEntry() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}

// ---- the fakes
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("TCache.Prepare", {lab(this), lab(entry), fbits(distance)});
}
void CTextureEntry::Lock() { ev("Entry.Lock", {lab(this)}); }
uint32_t CTextureEntry::GetBlendingMode() {
    ev("Entry.GetBlendingMode", {lab(this)});
    return static_cast<uint32_t>(ret("Entry.GetBlendingMode", 0));
}
// The matrix and the input are raw words; the four result words come from the queues TV0..TV3.
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    const uint32_t* v = static_cast<const uint32_t*>(in);
    ev("TransformVertex", {type, v[0], v[1], v[2]});
    const uint32_t* words = reinterpret_cast<const uint32_t*>(this);
    for (int i = 0; i < 16; ++i) DumpFloat(words[i]);
    uint32_t* o = static_cast<uint32_t*>(out);
    o[0] = static_cast<uint32_t>(ret("TV0"));
    o[1] = static_cast<uint32_t>(ret("TV1"));
    o[2] = static_cast<uint32_t>(ret("TV2"));
    o[3] = static_cast<uint32_t>(ret("TV3"));
}
// Turns the x and y of each of four corners by the quarter turns the angle's bits name:
// 0 none, 1 (x, y) -> (y, -x), 2 -> (-x, -y), 3 -> (-y, x). Exact on any bit pattern.
void RotateVertex_Y(SVertex* vertices, float angle, SVertex* centre) {
    uint32_t* w = reinterpret_cast<uint32_t*>(vertices);
    ev("RotateVertex_Y", {Cfb(angle), lab(centre), Canon(w[0]), Canon(w[1]), Canon(w[4]), Canon(w[5]),
                          Canon(w[8]), Canon(w[9]), Canon(w[12]), Canon(w[13])});
    uint32_t turn = Cfb(angle) & 3;
    for (int i = 0; i < 4; ++i) {
        uint32_t x = w[4 * i], y = w[4 * i + 1];
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
// Queues: GetNext (the record to hand out, -1 for none), GetNext.cur (the record that becomes the
// one in use: -2 the one handed out, else its index), GetNext.vlim and GetNext.ilim (how many
// bytes of room the in-use record's vertex and index memory have before the limits).
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t a, int32_t primitiveType, uint32_t size, int32_t mode,
                                             void* texture) {
    int64_t pick = ret("GetNext", -1);
    int64_t current = ret("GetNext.cur", -2);
    int64_t vertexRoom = ret("GetNext.vlim", 0);
    int64_t indexRoom = ret("GetNext.ilim", 0);
    ev("GetNextPrimitive", {lab(this), a, primitiveType, size, mode, lab(texture), pick});
    if (pick < 0) return nullptr;
    SPrimitive* result = &gRecords[pick].primitive;
    gLastReturned = result;
    Record& inUse = current == -2 ? gRecords[pick] : gRecords[current];
    mCurrent = &inUse.primitive;
    mVertexEnd = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(inUse.vertices.data()) +
                                                 static_cast<uintptr_t>(vertexRoom));
    mIndexEnd = reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(inUse.indices.data()) +
                                                static_cast<uintptr_t>(indexRoom));
    return result;
}
int32_t CDrawPrimitive::AddClipPolyList(int32_t& vertexCount, uint16_t* indices, int32_t indexCount, int32_t a) {
    int32_t which = indices == Sky().mIndices ? 1 : indices == Sky().mCylinderIndices ? 2 : 0;
    uint32_t hash = 17;
    for (int32_t i = 0; i < indexCount; ++i) hash = hash * 31u + indices[i];
    ev("AddClipPolyList", {lab(this), vertexCount, which, indexCount, a, hash});
    vertexCount = static_cast<int32_t>(ret("Clip.count", vertexCount));
    return static_cast<int32_t>(ret("Clip.result", 0));
}
int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float limit) {
    ev("Execute", {lab(this), Cfb(limit), IndexOf(mCurrent), IndexOf(gLastReturned)});
    if (gLastReturned != nullptr) DumpRecord(gRecords[IndexOf(gLastReturned)]);
    return static_cast<int32_t>(ret("Execute", 1));
}
// The parameter record (the word at 0x14 is never set by the sky and not recorded) and the four
// corners as floats.
int32_t CDrawPrimitive::DrawSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    ev("DrawSprite", {sprite->mFlags, sprite->mF08, sprite->mBlendingMode, lab(sprite->mTexture), Cfb(sprite->mU0),
                      Cfb(sprite->mV0), Cfb(sprite->mU1), Cfb(sprite->mV1), sprite->mF28, mode});
    const uint32_t* corners = static_cast<const uint32_t*>(sprite->mVertices);
    for (int i = 0; i < 16; ++i) DumpFloat(corners[i]);
    return static_cast<int32_t>(ret("DrawSprite", 1));
}

extern "C" {

// Object addresses for the labels: kind 0 sky, 1 draw primitive, 2 texture cache, 3 entry, 4 page.
void* tpw_sr_pointer(int32_t kind, int32_t index) {
    switch (kind) {
    case 0: return gSkyStorage;
    case 1: return static_cast<CDrawPrimitive*>(&gDraw);
    case 2: return gCacheStorage;
    case 3: return &gEntries[index];
    case 4: return &gPages[index];
    }
    return nullptr;
}

// Everything back to a known state: the display filled with 0xa5, entries and records empty.
void tpw_sr_clear() {
    gDump.clear();
    gLastReturned = nullptr;
    std::memset(static_cast<void*>(&gei_Display), 0xa5, sizeof gei_Display);
    for (CTextureEntry& entry : gEntries) std::memset(static_cast<void*>(&entry), 0xa5, sizeof entry);
    for (Record& record : gRecords) {
        record.vertices.clear();
        record.indices.clear();
        record.primitive = SPrimitive{};
    }
    gDraw.SetVertexSize(0);
    gDraw.mCurrent = nullptr;
    gDraw.mVertexEnd = nullptr;
    gDraw.mIndexEnd = nullptr;
    gei_DrawPrimitive = &gDraw;
    gei_MathUnit = &gUnit;
    std::memset(gRainStorage, 0, sizeof gRainStorage);
    std::memset(gLightStorage, 0, sizeof gLightStorage);
}

// The sky from words in the Mac's layout (texture slots hold entry labels, 0 for none) and its
// two halfword arrays, which the words leave out.
void tpw_sr_sky_load(const uint32_t* w, const uint16_t* indices, const uint16_t* cylinderIndices) {
    CSky& s = Sky();
    std::memcpy(static_cast<void*>(&s), w, 0x1fd0);
    s.mSkyTexture = EntryPointer(w[0x1fd0 / 4]);
    s.mColorTexture = EntryPointer(w[0x1fd4 / 4]);
    s.mCylinderTexture = EntryPointer(w[0x1fd8 / 4]);
    for (int i = 0; i < 16; ++i) {
        const uint32_t* a = w + 0x1fdc / 4 + i * 8;
        s.mAdds[i].mTexture = EntryPointer(a[0]);
        s.mAdds[i].mF04 = static_cast<int32_t>(a[1]);
        s.mAdds[i].mF08 = static_cast<int32_t>(a[2]);
        s.mAdds[i].mPosition = {Fl(a[3]), Fl(a[4]), Fl(a[5])};
        s.mAdds[i].mSize = Fl(a[6]);
        s.mAdds[i].mF1C = a[7];
    }
    std::memcpy(static_cast<void*>(&s.mAddCount), w + 0x21dc / 4, 0x2f14 - 0x21dc);
    std::memcpy(s.mIndices, indices, sizeof s.mIndices);
    std::memcpy(s.mCylinderIndices, cylinderIndices, sizeof s.mCylinderIndices);
}

void tpw_sr_sky_store(uint32_t* w, uint16_t* indices, uint16_t* cylinderIndices) {
    CSky& s = Sky();
    std::memcpy(w, static_cast<const void*>(&s), 0x1fd0);
    w[0x1fd0 / 4] = static_cast<uint32_t>(EntryLabel(s.mSkyTexture));
    w[0x1fd4 / 4] = static_cast<uint32_t>(EntryLabel(s.mColorTexture));
    w[0x1fd8 / 4] = static_cast<uint32_t>(EntryLabel(s.mCylinderTexture));
    for (int i = 0; i < 16; ++i) {
        uint32_t* a = w + 0x1fdc / 4 + i * 8;
        a[0] = static_cast<uint32_t>(EntryLabel(s.mAdds[i].mTexture));
        a[1] = static_cast<uint32_t>(s.mAdds[i].mF04);
        a[2] = static_cast<uint32_t>(s.mAdds[i].mF08);
        std::memcpy(a + 3, &s.mAdds[i].mPosition, 12);
        std::memcpy(a + 6, &s.mAdds[i].mSize, 4);
        a[7] = s.mAdds[i].mF1C;
    }
    std::memcpy(w + 0x21dc / 4, static_cast<const void*>(&s.mAddCount), 0x2f14 - 0x21dc);
    std::memcpy(indices, s.mIndices, sizeof s.mIndices);
    std::memcpy(cylinderIndices, s.mCylinderIndices, sizeof s.mCylinderIndices);
}

// A texture entry: flags, the page label (0 for none) and U size, V size, U offset, V offset.
void tpw_sr_entry(int32_t label, uint32_t flags, int32_t page, const uint32_t* f) {
    CTextureEntry& entry = gEntries[label];
    entry.mFlags = flags;
    entry.mTexture = page == 0 ? nullptr : reinterpret_cast<CTextureBase*>(&gPages[page]);
    entry.mUSize = Fl(f[0]);
    entry.mVSize = Fl(f[1]);
    entry.mUOffset = Fl(f[2]);
    entry.mVOffset = Fl(f[3]);
}

// g: display flags, mF04, mZ5C, mZ7C, system flags, renderer, the weather's frame time, the
// light manager's mask, the view angle (all but the flags and the renderer as float bits).
void tpw_sr_globals(const uint32_t* g) {
    gei_Display.mFlags = g[0];
    gei_Display.mF04 = Fl(g[1]);
    gei_Display.mZ5C = Fl(g[2]);
    gei_Display.mZ7C = Fl(g[3]);
    gei_System.mFlags = g[4];
    gei_System.mRenderer = static_cast<int32_t>(g[5]);
    gei_Rain->mDeltaTime = Fl(g[6]);
    gei_LightManager->mF04 = static_cast<int32_t>(g[7]);
    gViewAngle = Fl(g[8]);
}

void tpw_sr_matrices(const uint32_t* inverseView, const uint32_t* projection, const uint32_t* screen) {
    std::memcpy(static_cast<void*>(&gInverseView), inverseView, 64);
    std::memcpy(static_cast<void*>(&gei_ProjectionMatrix), projection, 64);
    std::memcpy(static_cast<void*>(&gScreenMatrix), screen, 64);
}
void tpw_sr_matrices_store(uint32_t* out) {
    std::memcpy(out, static_cast<const void*>(&gInverseView), 64);
    std::memcpy(out + 16, static_cast<const void*>(&gei_ProjectionMatrix), 64);
    std::memcpy(out + 32, static_cast<const void*>(&gScreenMatrix), 64);
}
void tpw_sr_display_store(uint32_t* out) { std::memcpy(out, static_cast<const void*>(&gei_Display), sizeof gei_Display); }

// A primitive: ten header words (type, seven more, vertex count, index count), its vertex
// memory (words) and the size of its index memory (halfwords).
void tpw_sr_record(int32_t i, const uint32_t* header, const uint32_t* vertexWords, int32_t nVertexWords,
                   int32_t nIndices) {
    Record& record = gRecords[i];
    record.vertices.assign(vertexWords, vertexWords + nVertexWords);
    record.indices.assign(static_cast<size_t>(nIndices), 0xa5a5);
    SPrimitive& p = record.primitive;
    p.mFlags = header[0];
    p.mBlendingMode = header[1];
    p.mTexture = reinterpret_cast<void*>(static_cast<uintptr_t>(header[2]));
    std::memcpy(p.mF0C, header + 3, sizeof p.mF0C);
    record.word14 = header[5];
    p.mPrimitiveType = static_cast<int32_t>(header[6]);
    p.mVertexFormat = header[7];
    p.mVertexCount = static_cast<int32_t>(header[8]);
    p.mIndexCount = static_cast<int32_t>(header[9]);
    p.mVertexData = reinterpret_cast<uint8_t*>(record.vertices.data());
    p.mIndexData = record.indices.data();
}

// The draw primitive's buffer state: the vertex size; the record in use and the limits are set
// by GetNextPrimitive.
void tpw_sr_draw(uint32_t vertexSize) {
    gDraw.SetVertexSize(vertexSize);
    gDraw.mCurrent = nullptr;
    gDraw.mVertexEnd = nullptr;
    gDraw.mIndexEnd = nullptr;
}

// fn 0 Prepare, 1 RenderAdd(arg), 2 Render.
int32_t tpw_sr_run(int32_t fn, int32_t arg) {
    switch (fn) {
    case 0: return Sky().Prepare();
    case 1: return Sky().RenderAdd(arg);
    default: return Sky().Render();
    }
}

// All records as the dump holds them (see DumpRecord), appended to the dump.
void tpw_sr_dump_records(int32_t count) {
    for (int32_t i = 0; i < count; ++i) DumpRecord(gRecords[i]);
}
uint32_t tpw_sr_dump_n() { return static_cast<uint32_t>(gDump.size()); }
void tpw_sr_dump_copy(uint32_t* out) { std::memcpy(out, gDump.data(), gDump.size() * 4); }

}  // extern "C"
