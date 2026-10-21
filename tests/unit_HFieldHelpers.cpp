// Harness for the height field's helpers and the face sorter (src/engine/DrawPrimitive.cpp, src/engine/tb_hfield.cpp).
//
// Everything the helpers call outside the unit is a fake that records through calllog:
//  - CLightManager::CalcIntensities (CalcFullLighting): records the colour offset and the point
//    count, dumps the six floats of every point (position, normal), and writes a colour per
//    point that mixes them, so a wrong input shows in the output too;
//  - the math unit's strided Transform and CClipping::CPP_TestData (CalcLine): Transform records
//    its arguments and rewrites the vertices as integers of the words it was given; CPP_TestData
//    records its arguments, dumps the vertices and writes a code per point from the same words;
//  - the assertion.
// CsTexture::GetTextureEntry and CTextureEntry::GetTexture are inline in the headers and run as
// they are (the PowerPC side runs the original's copies of them).
//
// The objects are built by extern "C" calls from tests/test_HFieldHelpers.py, which builds the
// same world in emulator memory. NaNs are canonical (0x7fc00000) wherever a word is recorded,
// dumped or mixed: the two processors differ in the NaN they make.
#include <cstdint>
#include <cstring>
#include <vector>

#include "MapWho.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/d3d_types.h"
#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

constexpr int32_t kEntries = 16;   // texture entries, numbers 1..kEntries
constexpr int32_t kPages = 8;      // texture pages, numbers 1..kPages

// The objects the helpers reach, as raw storage where the unit links no constructor.
alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
alignas(16) unsigned char gLightStorage[sizeof(CLightManager)];
CClipping gClipping;
CMatrix gFinalMatrix;
sHField gRecord;
alignas(16) unsigned char gEntryStorage[kEntries + 1][sizeof(CTextureEntry)];
char gPages[kPages + 1];   // a page is only an address: the later the label, the higher it lies

// The case's blocks from the arena (the record's pointers are 32-bit offsets from its base).
std::vector<void*> gArenaBlocks;
void* ArenaBlock(size_t bytes) {
    void* block = tpw::ArenaAllocZeroed(bytes + 16);
    gArenaBlocks.push_back(block);
    return block;
}
void FreeArenaBlocks() {
    for (void* block : gArenaBlocks) tpw::ArenaFree(block);
    gArenaBlocks.clear();
}

std::vector<uint32_t> gDump;   // what the fakes saw, in words
int32_t gIntensityCalls = 0;
const uint8_t* gCodesBase = nullptr;       // the arrays CalcLine works on: offsets are recorded
const SVertex* gVerticesBase = nullptr;

uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t Word(const void* p) {
    uint32_t w;
    std::memcpy(&w, p, 4);
    return w;
}
CTextureEntry* EntryAt(int32_t label) { return label == 0 ? nullptr : reinterpret_cast<CTextureEntry*>(gEntryStorage[label]); }

}  // namespace

// ---- the engine objects the helpers read
CSystem::CSystem() {}
CSystem gei_System;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CLightManager* gei_LightManager = reinterpret_cast<CLightManager*>(gLightStorage);
CClipping* gei_Clipping = &gClipping;
CMatrix* gei_FinalMatrix = &gFinalMatrix;

// ---- the fakes
// The assertion records. In a case that takes the math unit away it also throws: the original
// goes on to read through the NULL pointer (the PowerPC side faults there), which the host
// must not do.
namespace {
struct AssertionRaised {};
bool gAssertionThrows = false;
}  // namespace
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
    if (gAssertionThrows) throw AssertionRaised();
}

// Records the colour row (as a word offset from mVertexColours) and the count; dumps each point's
// position and normal; writes a colour per point from those six words, the point number and the
// call number.
void CLightManager::CalcIntensities(uint32_t* colours, _D3DVECTOR* normals, SVertex* vertices, int32_t count) {
    ev("CalcIntensities", {lab(this), colours - &Field().mVertexColours[0][0], count});
    ++gIntensityCalls;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t v[3] = {Canon(Word(&vertices[i].x)), Canon(Word(&vertices[i].y)), Canon(Word(&vertices[i].z))};
        const uint32_t n[3] = {Canon(Word(&normals[i].x)), Canon(Word(&normals[i].y)), Canon(Word(&normals[i].z))};
        for (uint32_t w : v) gDump.push_back(w);
        for (uint32_t w : n) gDump.push_back(w);
        colours[i] = v[0] * 3 + v[1] * 5 + v[2] * 7 + n[0] * 11 + n[1] * 13 + n[2] * 17 + static_cast<uint32_t>(i) * 19 +
                     static_cast<uint32_t>(gIntensityCalls) * 23;
    }
}

// The math unit: Transform (the strided overload) records and rewrites each vertex from the
// words it read; the other members are never called.
CTransformBase::~CTransformBase() {}
namespace {
class FakeMath : public CTransformBase {
public:
    ~FakeMath() override {}
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override { ev("Transform6"); }
    void Transform(sMatrix* m, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                   int32_t type) override {
        ev("Transform", {lab(this), lab(m), static_cast<const uint8_t*>(out) - reinterpret_cast<const uint8_t*>(gVerticesBase),
                         static_cast<const uint8_t*>(in) - reinterpret_cast<const uint8_t*>(gVerticesBase), outStride,
                         inStride, count, type});
        for (int32_t i = 0; i < count; ++i) {
            uint32_t w[4];
            std::memcpy(w, static_cast<const uint8_t*>(in) + static_cast<int64_t>(i) * inStride, sizeof w);
            for (uint32_t& word : w) word = Canon(word);
            const uint32_t k = static_cast<uint32_t>(i) * 16;
            const uint32_t r[4] = {w[0] * 3 + 1 + k, w[1] * 5 + 2 + k, w[2] * 7 + 3 + k, w[3] * 9 + w[0] + w[1] + w[2] + 4};
            std::memcpy(static_cast<uint8_t*>(out) + static_cast<int64_t>(i) * outStride, r, sizeof r);
        }
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMath gMath;
}  // namespace
CTransformBase* gei_MathUnit = &gMath;

// Records the arguments (the result buffer is the caller's stack), dumps the vertices as the
// test sees them, and writes a code per point from their words, masked.
void CClipping::CPP_TestData(uint8_t* result, void* clipping, uint8_t* codes, const void* vertices, int32_t count,
                             int32_t stride, uint32_t mask) {
    ev("CPP_TestData", {lab(clipping), codes - gCodesBase,
                        static_cast<const uint8_t*>(vertices) - reinterpret_cast<const uint8_t*>(gVerticesBase), count,
                        stride, mask});
    for (int32_t i = 0; i < count; ++i) {
        uint32_t w[4];
        std::memcpy(w, static_cast<const uint8_t*>(vertices) + static_cast<int64_t>(i) * stride, sizeof w);
        for (uint32_t& word : w) {
            word = Canon(word);
            gDump.push_back(word);
        }
        codes[i] = static_cast<uint8_t>((w[0] + w[1] * 3 + w[2] * 5 + w[3] * 7) >> 5) & mask;
    }
}

// ---- the calls from the Python half
extern "C" {

// Addresses the log labels: 0 the field, 1 the light manager, 2 the math unit, 3 the matrix, 4 the clipper.
void* tpw_hh_pointer(int32_t kind) {
    switch (kind) {
    case 0: return &Field();
    case 1: return gei_LightManager;
    case 2: return gei_MathUnit;
    case 3: return gei_FinalMatrix;
    default: return gei_Clipping;
    }
}

uint32_t tpw_hh_dump_n() { return static_cast<uint32_t>(gDump.size()); }
void tpw_hh_dump_copy(uint32_t* out) { std::memcpy(out, gDump.data(), gDump.size() * 4); }

// ---- CheckGamutArea: the row counts, the row bytes (128 low, then 128 high), the 0x100 bytes
// before them in the object, and the area. Bit 0 of the answer is the result; bit 8 says the
// object is unchanged, bit 9 the area.
int32_t tpw_hh_gamut(int32_t first, int32_t last, const int8_t* rows, const uint8_t* before, int8_t* area) {
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    std::memcpy(field.mPad28, before, 0x100);
    field.mF128 = first;
    field.mF12C = last;
    std::memcpy(field.mPad130, rows, 0x100);
    unsigned char objectBefore[sizeof(CHField)];
    std::memcpy(objectBefore, static_cast<const void*>(&field), sizeof field);
    int8_t areaBefore[4];
    std::memcpy(areaBefore, area, 4);
    const int32_t result = field.CheckGamutArea(area);
    const int32_t same = std::memcmp(objectBefore, static_cast<const void*>(&field), sizeof field) == 0;
    const int32_t areaSame = std::memcmp(areaBefore, area, 4) == 0;
    return result | same << 8 | areaSame << 9;
}

// ---- ProcessTextureParameters: the four corner bytes, then four more that must stay (in and
// out); the quad's two halfwords. Returns whether the quad is unchanged.
int32_t tpw_hh_process(uint8_t* corners, uint16_t flags, uint16_t second) {
    sHFieldQuad quad;
    quad.mF00 = flags;
    quad.mF02 = second;
    alignas(4) uint8_t work[8];
    std::memcpy(work, corners, 8);
    Field().ProcessTextureParameters(reinterpret_cast<uint32_t*>(work), &quad);
    std::memcpy(corners, work, 8);
    return quad.mF00 == flags && quad.mF02 == second;
}

// ---- the sorts. The arrays hold `n` items and the sort works on the part from `start` on
// (so the caller can leave words before it and see that they are not touched).

// Pages as 32-bit words (pointers that are never followed).
void tpw_hh_qsort_textures(uint32_t* words, int32_t n, int32_t start, int32_t low, int32_t high) {
    std::vector<void*> pages(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) pages[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(words[i]));
    Field().QSort_textures(pages.data() + start, low, high);
    for (int32_t i = 0; i < n; ++i) words[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pages[i]));
}

// Quads as four words each: the page, mFirstVertex, mF08, and the two counts packed.
void tpw_hh_qsort_quads(uint32_t* words, int32_t n, int32_t start, int32_t low, int32_t high) {
    std::vector<QuadSaveInfo> quads(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        quads[i].mTexture = reinterpret_cast<CTextureBase*>(static_cast<uintptr_t>(words[4 * i]));
        quads[i].mFirstVertex = static_cast<int32_t>(words[4 * i + 1]);
        quads[i].mF08 = static_cast<int32_t>(words[4 * i + 2]);
        quads[i].mCount1 = static_cast<int16_t>(words[4 * i + 3] >> 16);
        quads[i].mCount2 = static_cast<int16_t>(words[4 * i + 3] & 0xffff);
    }
    Field().qSortTextures_lpTex(quads.data() + start, low, high);
    for (int32_t i = 0; i < n; ++i) {
        words[4 * i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(quads[i].mTexture));
        words[4 * i + 1] = static_cast<uint32_t>(quads[i].mFirstVertex);
        words[4 * i + 2] = static_cast<uint32_t>(quads[i].mF08);
        words[4 * i + 3] = static_cast<uint32_t>(static_cast<uint16_t>(quads[i].mCount1)) << 16 |
                           static_cast<uint16_t>(quads[i].mCount2);
    }
}

// Keys as float words, three 16-bit indices per face.
void tpw_hh_qsort_faces(uint32_t* keys, uint16_t* faces, int32_t n, int32_t start, int32_t low, int32_t high) {
    std::vector<float> values(static_cast<size_t>(n));
    std::memcpy(values.data(), keys, static_cast<size_t>(n) * 4);
    QSortFaces_float(values.data() + start, faces + 3 * start, low, high);
    std::memcpy(keys, values.data(), static_cast<size_t>(n) * 4);
}

// ---- the world GetTexturesUsed reads. First the pieces, then the call.

// The rows in view (first, end, then 128 low and 128 high columns), the record's cells a row
// (mF18[0]) and quads (flags, texture number) with `before` quads in front of the first cell,
// the texture setting's flags, the procedural rectangle (mF234, mF238, mF23C, mF240) and the
// procedural entries (labels; the rest of the 2,500 are NULL).
void tpw_hh_used_field(int32_t first, int32_t end, const int8_t* rows, uint32_t cellsPerRow, const uint16_t* quads,
                       int32_t quadCount, int32_t before, uint32_t settings, const int32_t* rectangle,
                       const int32_t* proc, int32_t procCount) {
    FreeArenaBlocks();
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    std::memset(&gRecord, 0xa5, sizeof gRecord);
    field.mF00 = &gRecord;
    field.mF128 = first;
    field.mF12C = end;
    std::memcpy(field.mPad130, rows, 0x100);
    gRecord.mF18[0] = cellsPerRow;
    sHFieldQuad* block = static_cast<sHFieldQuad*>(ArenaBlock(sizeof(sHFieldQuad) * static_cast<size_t>(quadCount)));
    for (int32_t i = 0; i < quadCount; ++i) {
        block[i].mF00 = quads[2 * i];
        block[i].mF02 = quads[2 * i + 1];
    }
    gRecord.mQuads = block + before;
    gei_TextureSettings.mFlags = settings;
    field.mF234 = rectangle[0];
    field.mF238 = rectangle[1];
    field.mF23C = rectangle[2];
    field.mF240 = rectangle[3];
    for (int32_t i = 0; i < CHField::kMaxOverlays; ++i) field.mPrepareEntries[i].mTexture = i < procCount ? EntryAt(proc[i]) : nullptr;
}

// The mesh textures: `count` records, each naming a texture object (0 for none).
void tpw_hh_used_textures(const int32_t* objects, int32_t count) {
    sTexture* records = static_cast<sTexture*>(ArenaBlock(sizeof(sTexture) * static_cast<size_t>(count)));
    for (int32_t i = 0; i < count; ++i) {
        records[i].mF00 = 0;
        records[i].mF02 = 0;
        records[i].mTexture = nullptr;
    }
    Field().mF04 = records;
    for (int32_t i = 0; i < count; ++i) {
        if (objects[i] == 0) continue;
        records[i].mTexture = reinterpret_cast<CsTexture*>(ArenaBlock(sizeof(CsTexture)));
    }
}

// One texture object's first entry (and a second one that must never be read). Objects are the
// records' indices, so the record `index` gets its entries set here.
void tpw_hh_used_object(int32_t index, int32_t first, int32_t second) {
    CsTexture* texture = Field().mF04[index].mTexture;
    CTextureEntry** entries = static_cast<CTextureEntry**>(ArenaBlock(sizeof(CTextureEntry*) * 2));
    entries[0] = EntryAt(first);
    entries[1] = EntryAt(second);
    texture->mEntries = entries;
    texture->mEntryCount = 2;
    texture->mReferences = 1;
}

// A texture entry: its flags and the page it is on (the page is only read when flag 0x1000 is set).
void tpw_hh_used_entry(int32_t label, uint32_t flags, int32_t page) {
    CTextureEntry* entry = EntryAt(label);
    std::memset(static_cast<void*>(entry), 0xa5, sizeof(CTextureEntry));
    entry->mFlags = flags;
    entry->mTexture = page == 0 ? nullptr : reinterpret_cast<CTextureBase*>(&gPages[page]);
}

// The call: `count` starts as given; the page list is `pages` (capacity words) and the entry list
// `entries`. Pages come back as labels (0 for NULL, -1 for a pointer that is not a page).
int32_t tpw_hh_used_run(int32_t countBefore, int32_t* pagesOut, int32_t pageSlots, int32_t* entriesOut,
                        int32_t entrySlots, int32_t* rest) {
    CHField& field = Field();
    std::vector<void*> pages(static_cast<size_t>(pageSlots), reinterpret_cast<void*>(static_cast<uintptr_t>(0xa5a5a5a5)));
    std::vector<CTextureEntry*> entries(static_cast<size_t>(entrySlots),
                                        reinterpret_cast<CTextureEntry*>(static_cast<uintptr_t>(0xa5a5a5a5)));
    unsigned char objectBefore[sizeof(CHField)];
    std::memcpy(objectBefore, static_cast<const void*>(&field), sizeof field);
    int32_t count = countBefore;
    field.GetTexturesUsed(pages.data(), count, entries.data());
    *rest = std::memcmp(objectBefore, static_cast<const void*>(&field), sizeof field) == 0;
    for (int32_t i = 0; i < pageSlots; ++i) {
        void* page = pages[i];
        int32_t label = -1;
        if (page == nullptr) label = 0;
        else if (page >= static_cast<void*>(&gPages[1]) && page <= static_cast<void*>(&gPages[kPages]))
            label = static_cast<int32_t>(static_cast<char*>(page) - gPages);
        pagesOut[i] = label;
    }
    for (int32_t i = 0; i < entrySlots; ++i) {
        CTextureEntry* entry = entries[i];
        int32_t label = -1;
        if (entry == nullptr) label = 0;
        else
            for (int32_t k = 1; k <= kEntries; ++k)
                if (entry == EntryAt(k)) label = k;
        entriesOut[i] = label;
    }
    return count;
}

// ---- the world CalcFullLighting and CalcLine read: the record, the grid size and the heights.
// `before` floats in front of the first height are readable (the last row of CalcFullLighting
// reads 128 floats back).
void tpw_hh_grid(int32_t width, int32_t height, uint32_t inverseRange, const uint32_t* record, const uint32_t* heights,
                 int32_t heightCount, int32_t before) {
    FreeArenaBlocks();
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    std::memset(&gRecord, 0xa5, sizeof gRecord);
    field.mF00 = &gRecord;
    field.mGridWidth = width;
    field.mGridHeight = height;
    std::memcpy(&field.mInvHeightRange, &inverseRange, 4);
    std::memcpy(gRecord.mF08, record, 16);
    float* block = static_cast<float*>(ArenaBlock(sizeof(float) * static_cast<size_t>(heightCount)));
    std::memcpy(block, heights, sizeof(float) * static_cast<size_t>(heightCount));
    gRecord.mHeights = block + before;
    gDump.clear();
    gIntensityCalls = 0;
}

// CalcFullLighting: the system's light frame and flags, the object's mark. Returns bit 0: the rest of
// the object is unchanged (everything but the colours and the mark); the mark goes to *mark, the
// colours (128 x 128 words) to `colours`.
int32_t tpw_hh_lighting(int32_t lightFrame, uint32_t systemFlags, int32_t mark, uint32_t* colours, int32_t* markOut) {
    CHField& field = Field();
    gei_System.mLightFrame = lightFrame;
    gei_System.mFlags = systemFlags;
    field.mF12968 = mark;
    unsigned char objectBefore[sizeof(CHField)];
    std::memcpy(objectBefore, static_cast<const void*>(&field), sizeof field);
    field.CalcFullLighting();
    *markOut = field.mF12968;
    std::memcpy(colours, field.mVertexColours, sizeof field.mVertexColours);
    // The colours and the mark are expected to change: put them back before the comparison.
    unsigned char objectAfter[sizeof(CHField)];
    std::memcpy(objectAfter, static_cast<const void*>(&field), sizeof field);
    const size_t coloursAt = offsetof(CHField, mVertexColours);
    std::memcpy(objectAfter + coloursAt, objectBefore + coloursAt, sizeof field.mVertexColours);
    const size_t markAt = offsetof(CHField, mF12968);
    std::memcpy(objectAfter + markAt, objectBefore + markAt, 4);
    return std::memcmp(objectBefore, objectAfter, sizeof field) == 0;
}

// CalcLine: the arrays are the caller's (poison around the used part); `codePad` bytes and
// `vertexPad` vertices lie in front of the first item. With noMath the math unit is NULL: the
// assertion fires and the call is abandoned there. Returns bit 0: the object and the record
// are unchanged, bit 1: the call was abandoned.
int32_t tpw_hh_line(uint8_t* codes, int32_t codePad, uint32_t* vertices, int32_t vertexPad, uint32_t* spare, int32_t low,
                    int32_t high, int32_t row, int32_t noMath) {
    CHField& field = Field();
    gCodesBase = codes + codePad;
    gVerticesBase = reinterpret_cast<const SVertex*>(vertices + 4 * vertexPad);
    unsigned char objectBefore[sizeof(CHField)];
    std::memcpy(objectBefore, static_cast<const void*>(&field), sizeof field);
    unsigned char recordBefore[sizeof(sHField)];
    std::memcpy(recordBefore, &gRecord, sizeof gRecord);
    int32_t abandoned = 0;
    CTransformBase* math = gei_MathUnit;
    if (noMath != 0) {
        gei_MathUnit = nullptr;
        gAssertionThrows = true;
    }
    try {
        field.CalcLine(codes + codePad, reinterpret_cast<SVertex*>(vertices + 4 * vertexPad), spare, low, high, row);
    } catch (const AssertionRaised&) {
        abandoned = 1;
    }
    gei_MathUnit = math;
    gAssertionThrows = false;
    const int32_t same = std::memcmp(objectBefore, static_cast<const void*>(&field), sizeof field) == 0 &&
                         std::memcmp(recordBefore, &gRecord, sizeof gRecord) == 0;
    return same | abandoned << 1;
}

}  // extern "C"
