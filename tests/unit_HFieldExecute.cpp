// Fakes for CHField::Execute (src/tb_hfield_execute.cpp). Everything Execute calls records an
// event through tests/calllog.h; the larger things (the vertices handed to the clipper, the
// quads handed to the sort, a primitive when it is executed) go to a dump of words that the
// PowerPC side builds the same way (tests/test_HFieldExecute.py):
//
//  - the height field's helpers: CalcFullLighting records; CalcLine writes the case's next row
//    of outside codes, points and spare words; GetTexturesUsed writes the case's first page, count
//    and texture entry list; ProcessTextureParameters records the corner bytes it gets and the
//    quad, then writes the case's next four bytes; qSortTextures_lpTex dumps the quads and
//    reorders them as the queue "Sort" says (0 none, 1 stable by page label, 2 reversed);
//  - the lights: SMapLinkType::PrepareLights (with the light chain it finds), PrepareLighting;
//  - the draw primitive: GetNextPrimitive and DuplicateCurrentPrimitive hand out the case's
//    primitives (queues "GetNext" and "Duplicate": a primitive number, -1 for NULL);
//    CheckFogOnCurrentPrimitive and ProjectCurrentPrimitive record the last primitive's vertex
//    count and flip bits of its blending mode (queues "Fog.xor" and "Project.xor", so the order
//    in which Execute stores and reads them shows); ExecuteCurrentPrimitive dumps the last primitive and answers
//    from the queue "Execute";
//  - CClipping::ClipPoly dumps the polygon it gets (all words but the specular one, which
//    is stack garbage in the original) and writes the case's next vertex count of made-up
//    vertices.
#include <cstdint>
#include <cstring>
#include <vector>

#include "Light.h"
#include "MapWho.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

constexpr int32_t kEntries = 24;    // texture entries, numbers 1..kEntries
constexpr int32_t kPages = 8;       // texture pages, numbers 1..kPages
constexpr int32_t kRecords = 24;    // primitives the draw primitive can hand out

// The objects Execute reaches, as raw storage where the unit does not link a constructor.
alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
alignas(CMapWho) unsigned char gMapStorage[sizeof(CMapWho)];
alignas(CLightManager) unsigned char gLightStorage[sizeof(CLightManager)];
sHField gRecord;
sHFieldQuad* gQuads = nullptr;    // in the arena (sHField::mQuads is a Ptr32)
int32_t gQuadCount = 0;
SMapLinkType gLightList;          // the head of global list 6 when the case has one
char gLightChainObject;           // what gev_LightChain holds before the call
CClipping gClipping;
CTextureEntry gEntries[kEntries + 1];
char gPages[kPages + 1];

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

// The primitives: headers, index memory each, and one vertex memory they point into.
SPrimitive gPrimitives[kRecords];
std::vector<uint16_t> gIndexAreas[kRecords];
int32_t gVertexOffsets[kRecords];   // in vertices from the start of gVertexArea
std::vector<uint32_t> gVertexArea;
int32_t gLast = -1;                 // the primitive handed out last

// The case's data for the helpers, one item per call.
std::vector<uint8_t> gLineCodes;     // 128 a call
std::vector<uint32_t> gLinePoints;   // 512 a call
std::vector<uint32_t> gLineSpare;    // 128 a call
std::vector<uint32_t> gCornerWords;
std::vector<int32_t> gClipCounts;
uint32_t gClipSalt = 0;
int32_t gLineNext = 0, gCornerNext = 0, gClipNext = 0;   // calls made so far

// The item for the next call of a helper: the case's list in order, the last one again
// when the list runs out.
int32_t NextItem(int32_t& next, size_t count) {
    const int32_t k = next < static_cast<int32_t>(count) ? next : static_cast<int32_t>(count) - 1;
    ++next;
    return k;
}
int32_t gTexturePage = 0;
int32_t gTextureCount = 0;
std::vector<int32_t> gTextureEntries;

std::vector<uint32_t> gDump;

uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t Word(const void* p) {
    uint32_t w;
    std::memcpy(&w, p, 4);
    return w;
}
void* PagePointer(int32_t label) {
    if (label == 0) return nullptr;
    if (label < 0) return reinterpret_cast<void*>(static_cast<uintptr_t>(0xa5a5a5a5a5a5a5a5ull));
    return &gPages[label];
}
CTextureEntry* EntryPointer(int32_t label) { return label == 0 ? nullptr : &gEntries[label]; }
int32_t VertexOffset(const void* p) {
    return static_cast<int32_t>(static_cast<const uint8_t*>(p) - reinterpret_cast<const uint8_t*>(gVertexArea.data()));
}

// A primitive's header as the dump keeps it: flags, blending mode, page label, vertex and
// index counts, mF34, whether its data pointers are the ones it was given; then its indices.
void DumpPrimitive(int32_t i) {
    const SPrimitive& p = gPrimitives[i];
    gDump.push_back(p.mFlags);
    gDump.push_back(p.mBlendingMode);
    gDump.push_back(static_cast<uint32_t>(lab(p.mTexture)));
    gDump.push_back(static_cast<uint32_t>(p.mVertexCount));
    gDump.push_back(static_cast<uint32_t>(p.mIndexCount));
    gDump.push_back(static_cast<uint32_t>(p.mF34));
    gDump.push_back(p.mVertexData == reinterpret_cast<uint8_t*>(gVertexArea.data() + 8 * gVertexOffsets[i]) &&
                            p.mIndexData == gIndexAreas[i].data()
                        ? 1
                        : 0);
    for (uint16_t index : gIndexAreas[i]) gDump.push_back(index);
}

}  // namespace

// ---- the objects Execute reaches
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSystem::CSystem() {}
CSystem gei_System;
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);
CLightManager* gei_LightManager = reinterpret_cast<CLightManager*>(gLightStorage);
CMatrix gei_LightMatrix;
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
CClipping* gei_Clipping = &gClipping;
CLight* gev_LightChain = nullptr;
CTextureEntry::CTextureEntry() { std::memset(static_cast<void*>(this), 0, sizeof *this); }
CTextureEntry::~CTextureEntry() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}

// ---- the fakes
void SMapLinkType::PrepareLights() { ev("PrepareLights", {lab(this), lab(gev_LightChain)}); }
int32_t CLightManager::PrepareLighting(CMatrix* world, sAlignedBox* box) {
    ev("PrepareLighting", {lab(this), lab(world), lab(box)});
    return 1;
}
void CHField::CalcFullLighting() { ev("CalcFullLighting", {lab(this)}); }
// Records the row's arguments; writes the case's next row (the last one again when they run out).
void CHField::CalcLine(uint8_t* codes, SVertex* vertices, uint32_t* spare, int32_t low, int32_t high, int32_t row) {
    ev("CalcLine", {lab(this), low, high, row});
    if (gLineCodes.empty()) return;
    const int32_t k = NextItem(gLineNext, gLineCodes.size() / 128);
    std::memcpy(codes, &gLineCodes[128 * k], 128);
    std::memcpy(vertices, &gLinePoints[512 * k], 512 * 4);
    std::memcpy(spare, &gLineSpare[128 * k], 128 * 4);
}
void CHField::GetTexturesUsed(void** textures, int32_t& count, CTextureEntry** entries) {
    ev("GetTexturesUsed", {lab(this), lab(entries)});
    textures[0] = PagePointer(gTexturePage);
    count = gTextureCount;
    for (size_t i = 0; i < gTextureEntries.size(); ++i) entries[i] = EntryPointer(gTextureEntries[i]);
}
// The four corner bytes in memory order (first byte highest), the quad's number and words.
void CHField::ProcessTextureParameters(uint32_t* corners, sHFieldQuad* quad) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(corners);
    const int64_t number = quad - gQuads;
    const uint32_t bytes = (uint32_t{b[0]} << 24) | (uint32_t{b[1]} << 16) | (uint32_t{b[2]} << 8) | b[3];
    ev("ProcessTextureParameters", {lab(this), bytes, number,
                                    number >= 0 && number < gQuadCount ? quad->mF00 : -1,
                                    number >= 0 && number < gQuadCount ? quad->mF02 : -1});
    if (gCornerWords.empty()) return;
    const uint32_t w = gCornerWords[NextItem(gCornerNext, gCornerWords.size())];
    uint8_t* out = reinterpret_cast<uint8_t*>(corners);
    out[0] = static_cast<uint8_t>(w >> 24);
    out[1] = static_cast<uint8_t>(w >> 16);
    out[2] = static_cast<uint8_t>(w >> 8);
    out[3] = static_cast<uint8_t>(w);
}
void CHField::qSortTextures_lpTex(QuadSaveInfo* quads, int32_t low, int32_t high) {
    ev("qSortTextures_lpTex", {lab(this), lab(quads), low, high});
    for (int32_t i = 0; i <= high; ++i) {
        gDump.push_back(static_cast<uint32_t>(lab(quads[i].mTexture)));
        gDump.push_back(static_cast<uint32_t>(quads[i].mFirstVertex));
        gDump.push_back(static_cast<uint32_t>(quads[i].mF08));
        gDump.push_back(static_cast<uint32_t>(static_cast<uint16_t>(quads[i].mCount1)));
        gDump.push_back(static_cast<uint32_t>(static_cast<uint16_t>(quads[i].mCount2)));
    }
    const int64_t mode = ret("Sort", 0);
    if (mode == 1) {
        // insertion sort by page label, stable
        for (int32_t i = 1; i <= high; ++i) {
            QuadSaveInfo q = quads[i];
            int32_t j = i - 1;
            while (j >= 0 && lab(quads[j].mTexture) > lab(q.mTexture)) {
                quads[j + 1] = quads[j];
                --j;
            }
            quads[j + 1] = q;
        }
    } else if (mode == 2) {
        for (int32_t i = 0, j = high; i < j; ++i, --j) {
            QuadSaveInfo q = quads[i];
            quads[i] = quads[j];
            quads[j] = q;
        }
    }
}
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    const int64_t pick = ret("GetNext", 0);
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, lab(texture), pick});
    if (pick < 0) return nullptr;
    gLast = static_cast<int32_t>(pick);
    return &gPrimitives[pick];
}
SPrimitive* CDrawPrimitive::DuplicateCurrentPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat,
                                                      int32_t mode, void* texture) {
    const int64_t pick = ret("Duplicate", -1);
    ev("DuplicateCurrentPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, lab(texture), pick});
    if (pick < 0) return nullptr;
    gLast = static_cast<int32_t>(pick);
    return &gPrimitives[pick];
}
// The vertex count of the primitive handed out last (0 before any).
static int32_t LastVertexCount() { return gLast >= 0 ? gPrimitives[gLast].mVertexCount : 0; }
void CDrawPrimitive::CheckFogOnCurrentPrimitive() {
    ev("CheckFogOnCurrentPrimitive", {lab(this), gLast, LastVertexCount()});
    if (gLast >= 0) gPrimitives[gLast].mBlendingMode ^= static_cast<uint32_t>(ret("Fog.xor", 0));
}
void CDrawPrimitive::ProjectCurrentPrimitive(int32_t type) {
    ev("ProjectCurrentPrimitive", {lab(this), type, gLast, LastVertexCount()});
    if (gLast >= 0) gPrimitives[gLast].mBlendingMode ^= static_cast<uint32_t>(ret("Project.xor", 0));
}
int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    ev("ExecuteCurrentPrimitive", {lab(this), Canon(fbits(depth)), gLast});
    if (gLast >= 0) DumpPrimitive(gLast);
    return static_cast<int32_t>(ret("Execute", 1));
}
// Dumps the `count` vertices at `in` (all words but the specular one), then makes up the
// case's next count of vertices at `out`.
int32_t CClipping::ClipPoly(int32_t planes, SFVF_TLCTEX* out, SFVF_TLCTEX* in, int32_t& count, int32_t stride) {
    ev("ClipPoly", {lab(this), planes, VertexOffset(out), count, stride});
    for (int32_t i = 0; i < count && i < 21; ++i) {
        const uint8_t* v = reinterpret_cast<const uint8_t*>(in) + i * stride;
        for (int32_t j = 0; j < 8; ++j) {
            if (j == 5) continue;
            const uint32_t w = Word(v + 4 * j);
            gDump.push_back(j >= 6 ? Canon(w) : w);
        }
    }
    const int32_t k = gClipNext;
    const int32_t made = gClipCounts.empty() ? 3 : gClipCounts[NextItem(gClipNext, gClipCounts.size())];
    uint8_t* o = reinterpret_cast<uint8_t*>(out);
    for (int32_t i = 0; i < made; ++i) {
        for (int32_t j = 0; j < 8; ++j) {
            const uint32_t w = 0x9e3779b9u * static_cast<uint32_t>(k * 64 + i * 8 + j + 1) + gClipSalt;
            std::memcpy(o + i * stride + 4 * j, &w, 4);
        }
    }
    count = made;
    return 1;
}

extern "C" {

// The address of an object the test labels: 0 the height field, 1 the draw primitive, 2 the
// light manager, 3 the light matrix, 4 the clipper, 5 the light list head, 6 the texture
// entry list, 7 the quad records, 8 the light chain object, 9 page i, 10 entry i, 11 primitive i.
void* tpw_hx_pointer(int32_t kind, int32_t i) {
    switch (kind) {
    case 0: return &Field();
    case 1: return &gDraw;
    case 2: return gei_LightManager;
    case 3: return &gei_LightMatrix;
    case 4: return &gClipping;
    case 5: return &gLightList;
    case 6: return gQuadTextures;
    case 7: return gQuadSaveInfo;
    case 8: return &gLightChainObject;
    case 9: return &gPages[i];
    case 10: return &gEntries[i];
    case 11: return &gPrimitives[i];
    }
    return nullptr;
}

// The height field: the grid point rows and the cell rows (first, end, then 128 low and 128 high
// columns each), the colours of rows `firstColourRow`.. (128 words a row), the record's row
// length and quads (two halfwords each).
void tpw_hx_field(const int32_t* points, const int32_t* cells, const uint32_t* colours, int32_t firstColourRow,
                  int32_t colourRows, uint32_t rowLength, const uint16_t* quads, int32_t quadCount) {
    std::memset(gFieldStorage, 0xa5, sizeof gFieldStorage);
    CHField& f = Field();
    SGamutQuads* rows[2] = {f.VertexRows(), f.QuadRows()};
    const int32_t* values[2] = {points, cells};
    for (int32_t k = 0; k < 2; ++k) {
        rows[k]->mFirstRow = values[k][0];
        rows[k]->mEndRow = values[k][1];
        for (int32_t i = 0; i < 128; ++i) {
            rows[k]->mLow[i] = static_cast<int8_t>(values[k][2 + i]);
            rows[k]->mHigh[i] = static_cast<int8_t>(values[k][130 + i]);
        }
    }
    for (int32_t r = 0; r < colourRows; ++r) std::memcpy(f.mVertexColours[firstColourRow + r], colours + 128 * r, 512);
    std::memset(&gRecord, 0xa5, sizeof gRecord);
    gRecord.mF18[0] = rowLength;
    tpw::ArenaFree(gQuads);
    gQuads = static_cast<sHFieldQuad*>(tpw::ArenaAlloc(sizeof(sHFieldQuad) * (quadCount + 1)));
    for (int32_t i = 0; i < quadCount; ++i) {
        gQuads[i].mF00 = quads[2 * i];
        gQuads[i].mF02 = quads[2 * i + 1];
    }
    gQuadCount = quadCount;
    gRecord.mQuads = gQuads;
    f.mF00 = &gRecord;
}

// The globals: display flags, system flags, whether global list 6 has a head, whether the
// light chain holds something, the nine facing values (bits).
void tpw_hx_globals(uint32_t display, uint32_t system, int32_t haveList, int32_t haveChain, const uint32_t* face) {
    gei_Display.mFlags = display;
    gei_System.mFlags = system;
    std::memset(gMapStorage, 0xa5, sizeof gMapStorage);
    gei_MapWho->mF14[6] = haveList != 0 ? &gLightList : nullptr;
    gev_LightChain = haveChain != 0 ? reinterpret_cast<CLight*>(&gLightChainObject) : nullptr;
    float* globals[9] = {&gFaceX0, &gFaceX1, &gFaceX2, &gFaceY0, &gFaceY1, &gFaceY2, &gFaceW0, &gFaceW1, &gFaceW2};
    for (int32_t i = 0; i < 9; ++i) std::memcpy(globals[i], &face[i], 4);
    for (auto& q : gQuadSaveInfo) {
        q.mTexture = static_cast<CTextureBase*>(PagePointer(-1));
        q.mFirstVertex = static_cast<int32_t>(0xa5a5a5a5u);
        q.mF08 = static_cast<int32_t>(0xa5a5a5a5u);
        q.mCount1 = static_cast<int16_t>(0xa5a5);
        q.mCount2 = static_cast<int16_t>(0xa5a5);
    }
}

// A texture entry: flags, page number, U size, V size, U offset, V offset (bits), mF40.
void tpw_hx_entry(int32_t label, uint32_t flags, int32_t page, const uint32_t* place, int32_t f40) {
    CTextureEntry& e = gEntries[label];
    e.mFlags = flags;
    e.mTexture = static_cast<CTextureBase*>(PagePointer(page));
    std::memcpy(&e.mUSize, &place[0], 4);
    std::memcpy(&e.mVSize, &place[1], 4);
    std::memcpy(&e.mUOffset, &place[2], 4);
    std::memcpy(&e.mVOffset, &place[3], 4);
    e.mF40 = f40;
}

// The vertex memory (8 words a vertex).
void tpw_hx_vertices(const uint32_t* words, int32_t count) { gVertexArea.assign(words, words + count); }

// Primitive i: flags, blending mode, page number, vertex count, index count, mF34; its first
// vertex in the vertex memory; its index memory.
void tpw_hx_record(int32_t i, const uint32_t* header, int32_t vertexOffset, const uint16_t* indices, int32_t indexCount) {
    SPrimitive& p = gPrimitives[i];
    std::memset(static_cast<void*>(&p), 0xa5, sizeof p);
    p.mFlags = header[0];
    p.mBlendingMode = header[1];
    p.mTexture = PagePointer(static_cast<int32_t>(header[2]));
    p.mVertexCount = static_cast<int32_t>(header[3]);
    p.mIndexCount = static_cast<int32_t>(header[4]);
    p.mF34 = static_cast<int32_t>(header[5]);
    gIndexAreas[i].assign(indices, indices + indexCount);
    gVertexOffsets[i] = vertexOffset;
    p.mVertexData = reinterpret_cast<uint8_t*>(gVertexArea.data() + 8 * vertexOffset);
    p.mIndexData = gIndexAreas[i].data();
}

// The helpers' data: CalcLine's rows, ProcessTextureParameters' corner words, ClipPoly's
// vertex counts and the salt of its made-up vertices, GetTexturesUsed's page, count and list.
void tpw_hx_lines(const uint8_t* codes, const uint32_t* points, const uint32_t* spare, int32_t calls) {
    gLineCodes.assign(codes, codes + 128 * calls);
    gLinePoints.assign(points, points + 512 * calls);
    gLineSpare.assign(spare, spare + 128 * calls);
}
void tpw_hx_corners(const uint32_t* words, int32_t calls) {
    gCornerWords.assign(words, words + calls);
}
void tpw_hx_clips(const int32_t* counts, int32_t calls, uint32_t salt) {
    gClipCounts.assign(counts, counts + calls);
    gClipSalt = salt;
}
void tpw_hx_textures(int32_t page, int32_t count, const int32_t* entries, int32_t n) {
    gTexturePage = page;
    gTextureCount = count;
    gTextureEntries.assign(entries, entries + n);
}

int32_t tpw_hx_run() {
    gDump.clear();
    gLast = -1;
    gLineNext = gCornerNext = gClipNext = 0;
    return Field().Execute();
}

// After the call: every primitive's header and indices (DumpPrimitive's shape) go to the dump.
void tpw_hx_dump_records(int32_t count) {
    for (int32_t i = 0; i < count; ++i) DumpPrimitive(i);
}
uint32_t tpw_hx_dump_n() { return static_cast<uint32_t>(gDump.size()); }
void tpw_hx_dump_copy(uint32_t* out) { std::memcpy(out, gDump.data(), gDump.size() * 4); }
void tpw_hx_vertices_store(uint32_t* out) { std::memcpy(out, gVertexArea.data(), gVertexArea.size() * 4); }

// The first `count` quad records (page label, first vertex, mF08, the two counts), the nine
// facing values, the light chain's label and the two row blocks of the height field.
void tpw_hx_state(int32_t count, int32_t* quads, uint32_t* face, int32_t* chain, int32_t* rows) {
    for (int32_t i = 0; i < count; ++i) {
        const QuadSaveInfo& q = gQuadSaveInfo[i];
        quads[5 * i] = lab(q.mTexture);
        quads[5 * i + 1] = q.mFirstVertex;
        quads[5 * i + 2] = q.mF08;
        quads[5 * i + 3] = q.mCount1;
        quads[5 * i + 4] = q.mCount2;
    }
    const float* globals[9] = {&gFaceX0, &gFaceX1, &gFaceX2, &gFaceY0, &gFaceY1, &gFaceY2, &gFaceW0, &gFaceW1, &gFaceW2};
    for (int32_t i = 0; i < 9; ++i) std::memcpy(&face[i], globals[i], 4);
    *chain = lab(gev_LightChain);
    CHField& f = Field();
    const SGamutQuads* blocks[2] = {f.VertexRows(), f.QuadRows()};
    for (int32_t k = 0; k < 2; ++k) {
        rows[258 * k] = blocks[k]->mFirstRow;
        rows[258 * k + 1] = blocks[k]->mEndRow;
        for (int32_t i = 0; i < 128; ++i) {
            rows[258 * k + 2 + i] = blocks[k]->mLow[i];
            rows[258 * k + 130 + i] = blocks[k]->mHigh[i];
        }
    }
}

}  // extern "C"
