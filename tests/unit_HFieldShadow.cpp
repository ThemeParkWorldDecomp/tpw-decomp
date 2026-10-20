// Recording fakes and state wrappers for the height field shadow unit.
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <new>
#include <vector>
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_textureentry.h"
using namespace calllog;
namespace {
constexpr int32_t kEntries = 24, kPages = 8, kRecords = 24;
alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
sHField gRecord;
sHFieldQuad* gQuads = nullptr;
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

std::vector<uint8_t> gLineCodes;
std::vector<uint32_t> gLinePoints;
std::vector<int32_t> gClipCounts;
std::vector<uint32_t> gDump;
uint32_t gClipSalt;
int32_t gLineNext = 0, gClipNext = 0;
bool gDirectLine = false;
uint8_t* gCodesBase;
SVertex* gPointsBase;
const void* gLastTransform;
int32_t gUnchanged;
int32_t gHeightCount;
float* gHeights = nullptr;
float* gAlternate = nullptr;

// Snapshot all field bytes and file data; the draw and load wrappers mask only their output members.
struct Snapshot {
    unsigned char field[sizeof(CHField)];
    sHField record;
    std::vector<uint32_t> heights;
    std::vector<uint32_t> adjusted;

    Snapshot() : record(gRecord), heights(gHeightCount), adjusted(gHeightCount) {
        std::memcpy(field, gFieldStorage, sizeof field);
        std::memcpy(heights.data(), gHeights, 4 * gHeightCount);
        std::memcpy(adjusted.data(), gAlternate, 4 * gHeightCount);
    }
    void Mask(const void* member, size_t size) {
        const ptrdiff_t offset = static_cast<const unsigned char*>(member) - gFieldStorage;
        std::memcpy(field + offset, member, size);
    }
    bool Unchanged() const {
        return std::memcmp(field, gFieldStorage, sizeof field) == 0 &&
            std::memcmp(&record, &gRecord, sizeof record) == 0 &&
            std::memcmp(heights.data(), gHeights, 4 * gHeightCount) == 0 &&
            std::memcmp(adjusted.data(), gAlternate, 4 * gHeightCount) == 0;
    }
};
// The item for the next call of a helper: the case's list in order, the last one again
// when the list runs out.
int32_t NextItem(int32_t& next, size_t count) {
    const int32_t k = next < static_cast<int32_t>(count) ? next : static_cast<int32_t>(count) - 1;
    ++next;
    return k;
}
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
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSystem::CSystem() {}
CSystem gei_System;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CSky::CSky() {}
CSky::~CSky() {}
CSky gei_Sky;
CMatrix gMatrix;
CMatrix* gei_FinalMatrix = &gMatrix;
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
CClipping* gei_Clipping = &gClipping;
float gFaceX0, gFaceX1, gFaceX2, gFaceY0, gFaceY1, gFaceY2, gFaceW0, gFaceW1, gFaceW2;
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    const int64_t pick = ret("GetNext", 0);
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, lab(texture), pick});
    if (pick < 0) return nullptr;
    gLast = static_cast<int32_t>(pick);
    return &gPrimitives[pick];
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

// Sky and blending accessors remain independent recording callees.
CTextureEntry* CSky::GetSkyTEntry() {
    ev("GetSkyTEntry", {lab(this)});
    return mSkyTexture;
}
D3DVECTOR* CSky::GetPosition() {
    ev("GetPosition", {lab(this)});
    return &mPosition[0];
}
uint32_t CTextureEntry::GetBlendingMode() {
    ev("GetBlendingMode", {lab(this)});
    return static_cast<uint32_t>(ret("Blend", 0));
}
struct AssertionRaised {};
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
    throw AssertionRaised();
}
CTransformBase::~CTransformBase() {}
namespace {
class FakeMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix* matrix, void* out, void* in, int32_t os, int32_t is, int32_t count, int32_t type) override {
        const uint8_t* base = gDirectLine ? reinterpret_cast<uint8_t*>(gPointsBase) : static_cast<uint8_t*>(out);
        ev("Transform", {lab(this), lab(matrix), static_cast<uint8_t*>(out) - base,
                         static_cast<uint8_t*>(in) - base, os, is, count, type});
        gLastTransform = out;
        const int32_t k = gLineNext;
        for (int32_t i = 0; i < count; ++i) {
            const uint8_t* source = static_cast<uint8_t*>(in) + i * is;
            uint32_t words[4];
            std::memcpy(words, source, sizeof words);
            // w is uninitialized in DrawSkyShadow's stack rows; the direct helper test poisons it.
            for (int32_t j = 0; j < (gDirectLine ? 4 : 3); ++j) gDump.push_back(Canon(words[j]));
            if (gDirectLine) {
                for (uint32_t& word : words) word = Canon(word);
                const uint32_t n = static_cast<uint32_t>(i) * 16;
                const uint32_t result[4] = {words[0] * 3 + 1 + n, words[1] * 5 + 2 + n,
                    words[2] * 7 + 3 + n, words[3] * 9 + words[0] + words[1] + words[2] + 4};
                std::memcpy(static_cast<uint8_t*>(out) + i * os, result, sizeof result);
            } else {
                std::memcpy(static_cast<uint8_t*>(out) + i * os, &gLinePoints[512 * k + i * 4], 16);
            }
        }
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
FakeMath gMath;
}
CTransformBase* gei_MathUnit = &gMath;
void CClipping::CPP_TestData(uint8_t* result, void* clip, uint8_t* codes, const void* points, int32_t count,
                             int32_t stride, uint32_t mask) {
    const uint8_t* base = gDirectLine ? reinterpret_cast<uint8_t*>(gPointsBase) : static_cast<const uint8_t*>(gLastTransform);
    ev("CPP_TestData", {lab(clip), gDirectLine ? codes - gCodesBase : 0,
                        static_cast<const uint8_t*>(points) - base, count, stride, mask});
    for (int32_t i = 0; i < count; ++i) {
        uint32_t words[4];
        std::memcpy(words, static_cast<const uint8_t*>(points) + stride * i, sizeof words);
        for (uint32_t word : words) gDump.push_back(Canon(word));
        if (gDirectLine) {
            for (uint32_t& word : words) word = Canon(word);
            codes[i] = static_cast<uint8_t>((words[0] + words[1] * 3 + words[2] * 5 + words[3] * 7) >> 5) & mask;
        } else {
            codes[i] = gLineCodes[128 * gLineNext + i];
        }
    }
    result[0] = 0x12;
    result[1] = 0x34;
    result[2] = 0x56;
    result[3] = 0x78;
    if (!gDirectLine) {
        ++gLineNext;
    }
}

// Loader allocations are the nothrow calls in the translated source; ordinary fixture allocations are not recorded.
namespace {
void* gProcAllocation = nullptr;
void* gLoggerAllocation = nullptr;
void* gOldAllocation = nullptr;
void* gDiscardedLogger = nullptr;
}
void* operator new(size_t size, const std::nothrow_t&) noexcept {
    const bool proc = size == sizeof(CProcTextureHandler);
    const int32_t ok = static_cast<int32_t>(ret(proc ? "NewProc" : "NewLogger", 1));
    ev("new", {proc ? 0x10014 : static_cast<int64_t>(size), ok});
    if (ok == 0) return nullptr;
    void* p = std::malloc(size);
    if (proc) gProcAllocation = p;
    else gLoggerAllocation = p;
    return p;
}
namespace SamsUtilities {
ULogManager* gLogger;
ULogManager::ULogManager() { ev("LoggerCtor", {newlab(this)}); }
void ULogManager::SetContext(const char* context, const char* file, int32_t line, uint32_t flags) {
    ev("SetContext", {lab(this), line, flags}, {context, file});
    if (ret("ClearLogger", 0) != 0) {
        gDiscardedLogger = gLogger;
        gLogger = nullptr;
    }
}
void ULogManager::Log(const char* text, ...) { ev("Log", {lab(this)}, {text}); }
}
CProcTextureHandler::CProcTextureHandler() { ev("ProcCtor", {newlab(this)}); }
CProcTextureHandler::~CProcTextureHandler() {
    ev("ProcDtor", {lab(this)});
    if (this == gProcAllocation) gProcAllocation = nullptr;
    if (this == gOldAllocation) gOldAllocation = nullptr;
}
int32_t CProcTextureHandler::Init(int32_t count, int32_t size) {
    ev("ProcInit", {lab(this), count, size});
    return static_cast<int32_t>(ret("Init", 1));
}
bool CProcTextureHandler::Load(char* name) {
    ev("ProcLoad", {lab(this)}, {name});
    return ret("Load", 1) != 0;
}
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t severity, int32_t code) {
    ev("Push", {lab(this), severity, code});
}
extern "C" {
void* tpw_hx_pointer(int32_t kind, int32_t i) {
    switch (kind) {
    case 0: return &Field();
    case 1: return &gDraw;
    case 3: return &gMatrix;
    case 4: return &gClipping;
    case 9: return &gPages[i];
    case 10: return &gEntries[i];
    case 11: return &gPrimitives[i];
    case 12: return &gei_Sky;
    case 13: return &gMath;
    case 14: return &gei_ErrorHandler;
    }
    return nullptr;
}
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
    gRecord.mQuads = gQuads;
    f.mF00 = &gRecord;
}

void tpw_hx_globals(uint32_t display, uint32_t system, const uint32_t* face) {
    gei_Display.mFlags = display;
    gei_System.mFlags = system;
    const uint32_t* f = face;
    float* globals[9] = {&gFaceX0, &gFaceX1, &gFaceX2, &gFaceY0, &gFaceY1, &gFaceY2, &gFaceW0, &gFaceW1, &gFaceW2};
    for (int32_t i = 0; i < 9; ++i) std::memcpy(globals[i], f + i, 4);
}
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

void tpw_hx_lines(const uint8_t* codes, const uint32_t* points, int32_t calls) {
    gLineCodes.assign(codes, codes + 128 * calls);
    gLinePoints.assign(points, points + 512 * calls);
}
void tpw_hx_clips(const int32_t* counts, int32_t calls, uint32_t salt) {
    gClipCounts.assign(counts, counts + calls);
    gClipSalt = salt;
}
// The grid read by CalcShadowLine; either the file heights or the adjusted copy is selected.
void tpw_hs_grid(const uint32_t* coords, const uint32_t* heights, const uint32_t* adjusted, int32_t n,
                 int32_t width, int32_t before, int32_t useAdjusted, uint32_t lift) {
    tpw::ArenaFree(gHeights);
    tpw::ArenaFree(gAlternate);
    gHeights = static_cast<float*>(tpw::ArenaAlloc(4 * n));
    gAlternate = static_cast<float*>(tpw::ArenaAlloc(4 * n));
    gHeightCount = n;
    std::memcpy(gHeights, heights, 4 * n);
    std::memcpy(gAlternate, adjusted, 4 * n);
    gRecord.mHeights = gHeights + before;
    std::memcpy(gRecord.mF08, coords, 16);
    Field().mGridWidth = width;
    Field().mF12970 = useAdjusted ? gAlternate + before : nullptr;
    std::memcpy(&gei_Display.mF938, &lift, 4);
}
// Retained primitive, previous and current sky position, colour and persistent coordinate factors.
void tpw_hs_shadow(int32_t retained, int32_t entry, const uint32_t* sky, const uint32_t* previous,
                   uint32_t colour, const uint32_t* scale, int32_t vertexLimit, int32_t indexLimit) {
    Field().mShadowPrimitive = retained < 0 ? nullptr : &gPrimitives[retained];
    gei_Sky.mSkyTexture = EntryPointer(entry);
    std::memcpy(&gei_Sky.mPosition[0], sky, 12);
    std::memcpy(Field().mPad295C, previous, 12);
    gei_Display.mF8FC = static_cast<int32_t>(colour);
    gShadowUInitialized = static_cast<uint8_t>(scale[0]);
    gShadowVInitialized = static_cast<uint8_t>(scale[1]);
    std::memcpy(&gShadowUScale, scale + 2, 4);
    std::memcpy(&gShadowVScale, scale + 3, 4);
    gDraw.mVertexEnd = reinterpret_cast<uint8_t*>(gVertexArea.data()) + 32 * vertexLimit;
    gDraw.mIndexEnd = gIndexAreas[0].data() + indexLimit;
}
int32_t tpw_hs_run() {
    gDump.clear();
    gLineNext = gClipNext = 0;
    gLast = -1;
    gDirectLine = false;
    // Draw cases cover complete grid rows. Transform can overwrite every point after dumping its inputs.
    Snapshot before;
    const int32_t result = Field().DrawSkyShadow();
    before.Mask(&Field().mShadowPrimitive, sizeof Field().mShadowPrimitive);
    before.Mask(Field().mPad295C, sizeof Field().mPad295C);
    gUnchanged = before.Unchanged();
    return result;
}
void tpw_hs_state(uint32_t* out) {
    out[0] = static_cast<uint32_t>(lab(Field().mShadowPrimitive));
    std::memcpy(out + 1, Field().mPad295C, 12);
    out[4] = gShadowUInitialized;
    out[5] = gShadowVInitialized;
    std::memcpy(out + 6, &gShadowUScale, 4);
    std::memcpy(out + 7, &gShadowVScale, 4);
    const float* globals[9] = {&gFaceX0, &gFaceX1, &gFaceX2, &gFaceY0, &gFaceY1, &gFaceY2, &gFaceW0, &gFaceW1, &gFaceW2};
    for (int32_t i = 0; i < 9; ++i) std::memcpy(out + 8 + i, globals[i], 4);
    out[17] = static_cast<uint32_t>(gUnchanged);
}
int32_t tpw_hs_line(uint8_t* codes, uint32_t* points, int32_t pad, int32_t low, int32_t high, int32_t row,
                    int32_t noMath) {
    gDump.clear();
    gDirectLine = true;
    gei_MathUnit = noMath ? nullptr : &gMath;
    gCodesBase = codes + pad;
    gPointsBase = reinterpret_cast<SVertex*>(points + pad * 4);
    Snapshot before;
    int32_t abandoned = 0;
    try {
        Field().CalcShadowLine(codes + pad, reinterpret_cast<SVertex*>(points + pad * 4), low, high, row);
    } catch (const AssertionRaised&) {
        abandoned = 1;
    }
    gei_MathUnit = &gMath;
    return abandoned | (before.Unchanged() ? 2 : 0);
}
void tpw_hs_load_setup(int32_t renderer, int32_t haveOld, int32_t haveLogger, int32_t count) {
    std::free(gProcAllocation);
    std::free(gLoggerAllocation);
    std::free(gOldAllocation);
    // (a logger that SetContext discarded without a second allocation is still the logger allocation)
    if (gDiscardedLogger != gLoggerAllocation) std::free(gDiscardedLogger);
    gDiscardedLogger = nullptr;
    gProcAllocation = gLoggerAllocation = gOldAllocation = nullptr;
    gDump.clear();
    std::memset(gFieldStorage, 0xa5, sizeof gFieldStorage);
    gOldAllocation = haveOld ? std::malloc(sizeof(CProcTextureHandler)) : nullptr;
    Field().mProcTextures = static_cast<CProcTextureHandler*>(gOldAllocation);
    gLoggerAllocation = haveLogger ? std::malloc(sizeof(SamsUtilities::ULogManager)) : nullptr;
    SamsUtilities::gLogger = static_cast<SamsUtilities::ULogManager*>(gLoggerAllocation);
    gei_System.mRenderer = renderer;
    gei_TextureSettings.mProcTextureParam = count;
}
void* tpw_hs_load_pointer(int32_t kind) {
    return kind == 0 ? gOldAllocation : gLoggerAllocation;
}
int32_t tpw_hs_load(char* name, int32_t size, int32_t* state) {
    unsigned char before[sizeof(CHField)];
    std::memcpy(before, gFieldStorage, sizeof before);
    const int32_t result = Field().LoadProcTextures(name, size);
    state[0] = lab(Field().mProcTextures);
    state[1] = lab(SamsUtilities::gLogger);
    const ptrdiff_t offset = reinterpret_cast<unsigned char*>(&Field().mProcTextures) - gFieldStorage;
    std::memcpy(before + offset, &Field().mProcTextures, sizeof Field().mProcTextures);
    state[2] = std::memcmp(before, gFieldStorage, sizeof before) == 0;
    // Clear the tracked addresses freed by delete to avoid fixture double frees next case.
    if (Field().mProcTextures != gOldAllocation) gOldAllocation = nullptr;
    if (Field().mProcTextures != gProcAllocation) gProcAllocation = nullptr;
    return result;
}
// After the call: every primitive's header and indices (DumpPrimitive's shape) go to the dump.
void tpw_hx_dump_records(int32_t count) {
    for (int32_t i = 0; i < count; ++i) DumpPrimitive(i);
}
uint32_t tpw_hx_dump_n() { return static_cast<uint32_t>(gDump.size()); }
void tpw_hx_dump_copy(uint32_t* out) { std::memcpy(out, gDump.data(), gDump.size() * 4); }
void tpw_hx_vertices_store(uint32_t* out) { std::memcpy(out, gVertexArea.data(), gVertexArea.size() * 4); }

}
