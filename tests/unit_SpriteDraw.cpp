// Test entry point for the regular sprite draw path (library unit_SpriteDraw: src/tb_sprites_draw.cpp,
// src/tb_rotatevertex.cpp, src/tb_drawprimitive_addsprite.cpp, with the real sprite manager src/tb_sprites.cpp,
// DrawSprite and AddPoly_TLCTEX src/tb_drawprimitive_sprite.cpp, the real clipper and the real texture entry
// src/tb_textureentry.cpp): SSpriteGameData::Prepare, CalcRegular, SetupVertices, DrawRegular and AddRegular,
// RotateVertex_Y and RotateVertex_Z, CDrawPrimitive::AddSprite and the vertex record's four small members.
//
// tests/test_SpriteDraw.py builds one world twice from one description, in emulator memory for the original and
// through tpw_sd_run for the translation, runs a script of operations on both and compares, after every operation,
// the answer and the state of everything the operations could change (a flat list of words), and at the end the
// recorded calls of the fakes.
//
// The world: four sprite instances, three engine sprites, six texture entries, three arrays of partial texture entries
// (made with new[], as GeneratePartialTexture makes them), two sprite lists, the sprite manager, the height field, the
// display, the light manager, the final matrix, the draw object with four primitive records and its vertex and index
// memory, a sprite record for AddSprite, and a scratch record with four vertices for CalcRegular and SetupVertices.
//
// Real: the sprite manager (Reset, AddSprite, AddPartialSprite, AddList, Prepare, Render and its two renderers), the
// texture entry, DrawSprite, AddPoly_TLCTEX and the clipper. Fakes that record their arguments (floats by their bits, a
// NaN as one value; a pointer into the world as a place, the region's label in the high half): the texture cache's
// Prepare and Remove, CMatrix::TransformVertex, the math unit (Transform and Project), the primitive buffer
// (GetNextPrimitive, both ExecuteCurrentPrimitive, SaveInfo), CalcVertexFogIntensities, GeneratePartialTexture (unit
// w41b's), the three shadow and partial drawing members (unit w41b's), the engine allocator's free and the assertion
// handler.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
// A script queues a few words per operation for several of the fakes.
#define TPW_CALLLOG_QUEUE_VALUES 4096
#include "calllog.cpp"

#include "MapWho.h"
#include "Ptr32.h"
#include "Wavelet.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "tb_clipping.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

// ---- The world's sizes (tests/test_SpriteDraw.py has the same numbers) -----------------------------------
constexpr int32_t kSprites = 4;           // sprite instances, numbered 1..4
constexpr int32_t kEngines = 3;           // engine sprites, 1..3
constexpr int32_t kEntries = 6;           // single texture entries, 1..6
constexpr int32_t kArrays = 3;            // partial texture arrays, one per engine sprite
constexpr int32_t kMaxPart = 4;           // entries in an array
constexpr int32_t kLists = 2;
constexpr int32_t kRecords = 4;           // primitive records
constexpr int32_t kVertexBytes = 0x2000;  // the vertex memory; a record's slice is a quarter of it
constexpr int32_t kIndexEntries = 2048;
constexpr int32_t kInputWords = 128;
constexpr int32_t kHeights = 192;         // the height field's samples: 64 before the field's first, 64 for it, 64 after
constexpr int32_t kHeightsBefore = 64;    // (a sprite outside the grid reads before or after its samples)
constexpr int32_t kMarkers = 8;           // page markers (opaque pointers, never dereferenced)
constexpr int32_t kShown = 16;            // entries of the manager's tables the state shows
constexpr int32_t kShownGroups = 8;
constexpr uint32_t kNoOffset = 0x80000000u;

enum Label : int32_t {
    kLabelDraw = 1,
    kLabelMath = 2,
    kLabelRender = 3,
    kLabelLight = 4,
    kLabelCache = 5,
    kLabelMatrix = 6,
    kLabelManager = 7,
    kLabelHField = 8,
    kLabelMarker = 11,   // 11 .. 18
    kLabelEntry = 21,    // 21 .. 26
    kLabelSprite = 31,   // 31 .. 34
    kLabelEngine = 41,   // 41 .. 43
    kLabelArray = 51,    // 51 .. 53: the block of an array of partial entries
    kLabelList = 61,     // 61, 62
};
// Region numbers of Where(): pointers into the world.
enum Region : int64_t { kVertices = 1, kIndices = 2, kInput = 3, kScratchVertices = 4, kScratchRecord = 5 };

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
uint32_t WordAt(const void* address) {
    uint32_t word;
    std::memcpy(&word, address, 4);
    return word;
}

// ---- The world ----------------------------------------------------------------------------------------------
alignas(16) unsigned char gSpriteStore[kSprites][sizeof(SSpriteGameData)];
alignas(16) unsigned char gEngineStore[kEngines][sizeof(SSpriteEngineData)];
alignas(16) unsigned char gEntryStore[kEntries][sizeof(CTextureEntry)];
alignas(16) unsigned char gListStore[kLists][sizeof(CSpriteList)];
alignas(16) unsigned char gManagerStore[sizeof(CSpriteManager)];
alignas(16) unsigned char gHFieldStore[sizeof(CHField)];
alignas(16) unsigned char gMatrixStore[sizeof(CMatrix)];
alignas(16) unsigned char gCacheStore[16];
char gMarkers[kMarkers];
char gPixelBlock[8];   // what a block sprite's data pointer points at
SSpriteGameData* gListInstances[kLists][kSprites];
bool gSpriteMode[kSprites];   // the word at Mac 0x30 holds the used width and height (a partial sprite), else the angle
sHField* gHeightField;
float* gHeights;

SPrimitive gRecords[kRecords];
alignas(16) uint8_t gVertices[kVertexBytes];
uint16_t gIndices[kIndexEntries];
uint32_t gInput[kInputWords];
SSpriteDrawParameters gSprite;
SSpriteDrawParameters gScratchRecord;
SVertex gScratchVertices[4];
int32_t gNextRecord;

// The arrays of partial texture entries: one slot per engine sprite.
struct ArraySlot {
    char* block;              // the raw allocation (known by its label), nullptr when no array is alive
    CTextureEntry* first;
    int32_t count;
};
ArraySlot gArrays[kArrays];
int32_t gTrackSlot = -1;      // the slot whose block the allocation observer labels next
char* gTrackBlock;
std::size_t gCookie = 16;

// The regions Where() knows.
struct RegionInfo {
    int32_t label;
    const uint8_t* base;
    std::size_t size;
};
RegionInfo gRegions[16];
int32_t gRegionCount;
void AddRegion(int32_t label, const void* base, std::size_t size) {
    gRegions[gRegionCount++] = {label, static_cast<const uint8_t*>(base), size};
}
// A pointer as (region label << 16) + offset, 0 for NULL, -1 for anything else.
int64_t Where(const void* pointer) {
    if (pointer == nullptr) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(pointer);
    for (int32_t i = 0; i < gRegionCount; ++i) {
        if (p >= gRegions[i].base && p < gRegions[i].base + gRegions[i].size) {
            return (static_cast<int64_t>(gRegions[i].label) << 16) + (p - gRegions[i].base);
        }
    }
    return -1;
}

SSpriteGameData* Sprite(int32_t number) {
    return number >= 1 && number <= kSprites ? reinterpret_cast<SSpriteGameData*>(gSpriteStore[number - 1]) : nullptr;
}
SSpriteEngineData* Engine(int32_t number) {
    return number >= 1 && number <= kEngines ? reinterpret_cast<SSpriteEngineData*>(gEngineStore[number - 1]) : nullptr;
}
CTextureEntry* Entry(int32_t number) {
    return number >= 1 && number <= kEntries ? reinterpret_cast<CTextureEntry*>(gEntryStore[number - 1]) : nullptr;
}
CSpriteList* List(int32_t number) {
    return number >= 1 && number <= kLists ? reinterpret_cast<CSpriteList*>(gListStore[number - 1]) : nullptr;
}
CTextureBase* Marker(int32_t number) {
    return number >= 1 && number <= kMarkers ? reinterpret_cast<CTextureBase*>(&gMarkers[number - 1]) : nullptr;
}
int32_t MarkerNumber(const void* pointer) {
    for (int32_t i = 0; i < kMarkers; ++i) {
        if (pointer == &gMarkers[i]) return i + 1;
    }
    return pointer == nullptr ? 0 : -1;
}
CSpriteManager* Manager() { return reinterpret_cast<CSpriteManager*>(gManagerStore); }
CHField* HField() { return reinterpret_cast<CHField*>(gHFieldStore); }
CMatrix* Matrix() { return reinterpret_cast<CMatrix*>(gMatrixStore); }

int32_t SpriteNumber(const SSpriteGameData* sprite) {
    if (sprite == nullptr) return 0;
    for (int32_t i = 1; i <= kSprites; ++i) {
        if (sprite == Sprite(i)) return i;
    }
    return -1;
}
int32_t EngineNumber(const SSpriteEngineData* engine) {
    if (engine == nullptr) return 0;
    for (int32_t i = 1; i <= kEngines; ++i) {
        if (engine == Engine(i)) return i;
    }
    return -1;
}

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
uint8_t* VertexPointer(uint32_t offset) { return offset == kNoOffset ? nullptr : gVertices + static_cast<int32_t>(offset); }
uint16_t* IndexPointer(uint32_t offset) {
    if (offset == kNoOffset) return nullptr;
    return reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(gIndices) + static_cast<int32_t>(offset));
}
uint32_t VertexOffset(const void* pointer) {
    return pointer == nullptr ? kNoOffset : static_cast<uint32_t>(static_cast<const uint8_t*>(pointer) - gVertices);
}
uint32_t IndexOffset(const void* pointer) {
    return pointer == nullptr ? kNoOffset
                              : static_cast<uint32_t>(static_cast<const uint8_t*>(pointer) - reinterpret_cast<const uint8_t*>(gIndices));
}
int32_t VertexSizeOf(uint32_t format) {
    switch (format) {
    case 0xc4: return 0x18;
    case 0x1c4: return 0x20;
    case 0x2c4: return 0x28;
    case 0x3c4: return 0x30;
    }
    return 0;
}

// ---- Arrays of partial texture entries -----------------------------------------------------------------------
void ObserveAllocation(std::size_t size, bool array, void* block) {
    std::memset(block, 0xa5, size);
    if (array && gTrackSlot >= 0) {
        gTrackBlock = static_cast<char*>(block);
        tpw_cl_label(kLabelArray + gTrackSlot, block);
    }
}

// The label of the entry: a single one by its number, one of a live array by the array's label times 100 plus its index.
int32_t EntryLabel(const CTextureEntry* entry) {
    if (entry == nullptr) return 0;
    for (int32_t a = 0; a < kArrays; ++a) {
        const ArraySlot& slot = gArrays[a];
        if (slot.first == nullptr || lab(slot.block) != kLabelArray + a) continue;
        if (entry >= slot.first && entry < slot.first + slot.count) {
            return (kLabelArray + a) * 100 + static_cast<int32_t>(entry - slot.first);
        }
    }
    for (int32_t i = 1; i <= kEntries; ++i) {
        if (entry == Entry(i)) return kLabelEntry + i - 1;
    }
    return -1;
}

// Makes the array of slot `a`: `count` entries made by new[] (so that the engine's delete[] of it runs their destructors),
// each blank apart from the flags, the page marker and a zero data pointer.
CTextureEntry* MakeArray(int32_t a, int32_t count, const uint32_t* flags, const int32_t* pages, const int32_t* data) {
    gTrackSlot = a;
    gTrackBlock = nullptr;
    CTextureEntry* first = new CTextureEntry[count];
    gTrackSlot = -1;
    gCookie = static_cast<std::size_t>(reinterpret_cast<char*>(first) - gTrackBlock);
    for (int32_t i = 0; i < count; ++i) {
        std::memset(static_cast<void*>(&first[i]), 0, sizeof(CTextureEntry));
        first[i].mPlace = &first[i].mOwnPlace;
        first[i].mFlags = flags[i];
        first[i].mTexture = Marker(pages[i]);
        first[i].mData = data[i] != 0 ? gPixelBlock : nullptr;
    }
    gArrays[a] = {gTrackBlock, first, count};
    return first;
}

// ---- The engine around the unit ----------------------------------------------------------------------------------
CRenderMemory gRender;
CLightManager gLight;
CClipping gClipping;

// The primitive buffer's object: its members are fakes below, the rest is empty.
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
alignas(TestDraw) unsigned char gDrawStore[sizeof(TestDraw)];
TestDraw* Draw() { return reinterpret_cast<TestDraw*>(gDrawStore); }

// The math unit: the strided Transform records the matrix and the vertices it is given, then turns each vertex's first three
// words round (a permutation and sign flips chosen by the queue "TransformMode", one number per vertex) and gives it a w from
// the queue "TransformW" (float bits, 1.0 by default); Project answers from the queue.
class FakeMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix* matrix, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count,
                   int32_t type) override {
        ev("Transform", {lab(this), lab(matrix), Where(out), Where(in), outStride, inStride, count, type});
        static const int kOrder[6][3] = {{0, 1, 2}, {1, 2, 0}, {2, 0, 1}, {0, 2, 1}, {2, 1, 0}, {1, 0, 2}};
        uint32_t words[16][3];
        const int32_t n = count < 0 ? 0 : (count > 16 ? 16 : count);
        for (int32_t i = 0; i < n; ++i) {
            std::memcpy(words[i], static_cast<uint8_t*>(in) + static_cast<int64_t>(i) * inStride, 12);
            // (a NaN is turned into the one NaN first: its sign depends on the arithmetic that made it, which the PowerPC and the
            // host do not agree on, and the sign would travel on through the flips below)
            for (uint32_t& word : words[i]) word = Canon(word);
            ev("TransformIn", {Canon(words[i][0]), Canon(words[i][1]), Canon(words[i][2]),
                               Canon(WordAt(static_cast<uint8_t*>(in) + static_cast<int64_t>(i) * inStride + 12))});
        }
        for (int32_t i = 0; i < n; ++i) {
            const int64_t mode = ret("TransformMode", 0);
            const int32_t permutation = static_cast<int32_t>(mode % 6);
            const uint32_t flips = static_cast<uint32_t>(mode / 6) % 8;
            uint32_t result[4];
            for (int32_t k = 0; k < 3; ++k) {
                result[k] = words[i][kOrder[permutation][k]];
                if ((flips >> k) & 1) result[k] ^= 0x80000000u;
            }
            result[3] = static_cast<uint32_t>(ret("TransformW", 0x3f800000));
            std::memcpy(static_cast<uint8_t*>(out) + static_cast<int64_t>(i) * outStride, result, 16);
            ev("TransformOut", {Canon(result[0]), Canon(result[1]), Canon(result[2]), Canon(result[3])});
        }
    }
    float Project(void* vertices, int32_t stride, int32_t count, int32_t type) override {
        ev("Project", {lab(this), Where(vertices), stride, count, type});
        return FromBits(static_cast<uint32_t>(ret("Project", 0)));
    }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
alignas(FakeMath) unsigned char gMathStore[sizeof(FakeMath)];
FakeMath* Math() { return reinterpret_cast<FakeMath*>(gMathStore); }

}  // namespace

// The classes' constructors and destructors other translation units define.
CTransformBase::~CTransformBase() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CDisplay::CDisplay() {}
CRenderMemory::CRenderMemory() {}
CLightManager::CLightManager() {}
CSystem::CSystem() {}
CTextureSettings::CTextureSettings() {}

CDisplay gei_Display;
CSystem gei_System;
CTextureSettings gei_TextureSettings;
CErrorHandler gei_ErrorHandler;
CDrawPrimitive* gei_DrawPrimitive = nullptr;
CRenderMemory* gei_RenderMemory = &gRender;
CLightManager* gei_LightManager = &gLight;
CClipping* gei_Clipping = &gClipping;
CTransformBase* gei_MathUnit = nullptr;
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
CSpriteManager* gei_SpriteManager = reinterpret_cast<CSpriteManager*>(gManagerStore);
CHField* gei_HField = reinterpret_cast<CHField*>(gHFieldStore);
CMatrix* gei_FinalMatrix = reinterpret_cast<CMatrix*>(gMatrixStore);
CTextureLoadManager* gei_TLoadManager = nullptr;
float gViewAngle = 0.0f;
float gSineTable[256];
float gSpriteHotX = 0.0f;
float gSpriteHotY = 0.0f;
uint8_t* gProcTextureBuffer = nullptr;
const int32_t gTextureBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};

namespace {
// The engine allocator: nothing here asks it for memory; freeing is recorded.
void* FakeMalloc(uint32_t) { return nullptr; }
void FakeFree(void*& block) {
    ev("Free", {lab(block)});
    block = nullptr;
}
}  // namespace
TbMallocFn gMalloc = FakeMalloc;
TbFreeFn gFree = FakeFree;

void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
CTextureEntry* CTextureLoadManager::LoadFile(char*) { return nullptr; }
CWavelet::~CWavelet() {}
CWavelet* gWaveLoadWavelet = nullptr;

// ---- The texture cache -------------------------------------------------------------------------------------------
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("Cache.Prepare", {lab(this), EntryLabel(entry), CanonBits(distance)});
}
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("Cache.Remove", {lab(this), EntryLabel(entry)});
    return 1;
}

// The view matrix transform: records the matrix, where the output goes, where the input is, the type and the three input
// words, then writes four words from the queue "TV" (x, y, depth, w).
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    const uint32_t* source = static_cast<const uint32_t*>(in);
    ev("TransformVertex", {lab(this), Where(out), Where(in), type, Canon(source[0]), Canon(source[1]), Canon(source[2])});
    uint32_t result[4];
    for (uint32_t& word : result) word = static_cast<uint32_t>(ret("TV", 0));
    std::memcpy(out, result, 16);
}

// The primitive buffer. GetNextPrimitive hands out a record: the one the queue names, or by default the next one in turn
// made fresh (no vertices or indices, its slice of the memories, no flags, no depth); a queued -1 hands out none.
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, MarkerNumber(texture)});
    const int64_t wanted = ret("GetNextPrimitive", 0);
    if (wanted == -1) {
        mCurrent = nullptr;
        return nullptr;
    }
    SPrimitive* record = nullptr;
    if (wanted >= 1 && wanted <= kRecords) {
        record = Record(wanted);
    } else {
        const int32_t k = gNextRecord % kRecords;
        ++gNextRecord;
        record = &gRecords[k];
        record->mFlags = 0;
        record->mVertexCount = 0;
        record->mIndexCount = 0;
        record->mVertexData = gVertices + k * (kVertexBytes / kRecords);
        record->mIndexData = gIndices + k * (kIndexEntries / kRecords);
        record->mDepth = 0.0f;
    }
    mCurrent = record;
    record->mBlendingMode = static_cast<uint32_t>(mode);
    record->mPrimitiveType = primitiveType;
    record->mVertexFormat = vertexFormat;
    record->mTexture = texture;
    record->mF34 = -1;
    SetVertexSize(VertexSizeOf(vertexFormat));
    return record;
}

int32_t CDrawPrimitive::ExecuteCurrentPrimitive() {
    ev("ExecuteCurrentPrimitive", {lab(this)});
    mCurrent = nullptr;
    return static_cast<int32_t>(ret("Execute", 1));
}

// Like the real one, nothing happens (and 1 is answered) when there is no primitive or it is empty; else the primitive is
// executed, which clears it.
int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    ev("ExecuteCurrentPrimitiveDepth", {lab(this), CanonBits(depth)});
    const SPrimitive* primitive = mCurrent;
    if (primitive == nullptr || primitive->mVertexCount == 0 || primitive->mIndexCount == 0) return 1;
    mCurrent = nullptr;
    return static_cast<int32_t>(ret("ExecuteDepth", 1));
}

void CRenderMemory::SaveInfo(char* vertexEnd, uint16_t* indexEnd) {
    ev("SaveInfo", {lab(this), Where(vertexEnd), Where(indexEnd)});
}

// The light manager: whether anything is fogged comes from the queue "Fog".
uint8_t CLightManager::CalcVertexFogIntensities(SFVF_TLCTEX* vertices, int32_t count, int32_t stride) {
    ev("CalcVertexFogIntensities", {lab(this), Where(vertices), count, stride});
    return static_cast<uint8_t>(ret("Fog", 0));
}

// The partial texture generator and the shadow and partial drawing members belong to unit w41b.
// GeneratePartialTexture makes the engine sprite's array when the queue "PartCount" says so (its flags and page markers from
// the queues "PartFlags" and "PartPage") and answers 1; with a count of zero it makes nothing and answers 0.
int32_t SSpriteEngineData::GeneratePartialTexture(int32_t x, int32_t y) {
    ev("GeneratePartialTexture", {lab(this), x, y});
    const int32_t count = static_cast<int32_t>(ret("PartCount", 0));
    if (count <= 0) return 0;
    const int32_t n = count > kMaxPart ? kMaxPart : count;
    const int32_t slot = EngineNumber(this) - 1;
    uint32_t flags[kMaxPart];
    int32_t pages[kMaxPart];
    int32_t data[kMaxPart];
    for (int32_t i = 0; i < n; ++i) {
        flags[i] = static_cast<uint32_t>(ret("PartFlags", 0));
        pages[i] = static_cast<int32_t>(ret("PartPage", 0));
        data[i] = 0;
    }
    mTexture = MakeArray(slot, n, flags, pages, data);
    mPartCount = static_cast<uint16_t>(n);
    return 1;
}
int32_t SSpriteGameData::DrawRegularShadow() {
    ev("DrawRegularShadow", {lab(this)});
    return static_cast<int32_t>(ret("Shadow", 1));
}
int32_t SSpriteGameData::AddRegularShadow(CTextureEntry* shadowTexture) {
    ev("AddRegularShadow", {lab(this), EntryLabel(shadowTexture)});
    return static_cast<int32_t>(ret("AddShadow", 1));
}
int32_t SSpriteGameData::DrawPartial() {
    ev("DrawPartial", {lab(this)});
    return static_cast<int32_t>(ret("DrawPartial", 1));
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

enum Op : int32_t {
    kOpPrepare = 1,
    kOpCalcRegular = 2,
    kOpSetupVertices = 3,
    kOpDrawRegular = 4,
    kOpAddRegular = 5,
    kOpAddSprite = 6,
    kOpReset = 7,
    kOpAddList = 8,
    kOpManagerPrepare = 9,
    kOpManagerRender = 10,
    kOpSetSprite = 11,
    kOpSetEngine = 12,
    kOpSetDisplay = 13,
    kOpSetEntry = 14,
};

// A sprite's word at a Mac offset: the host member that holds it.
void SetSpriteWord(SSpriteGameData* s, int32_t offset, uint32_t value) {
    switch (offset) {
    case 0x00: std::memcpy(&s->mScreenX, &value, 4); break;
    case 0x04: std::memcpy(&s->mScreenY, &value, 4); break;
    case 0x08: std::memcpy(&s->mDepth, &value, 4); break;
    case 0x0c: std::memcpy(&s->mScreenW, &value, 4); break;
    case 0x10: s->mF10 = static_cast<int32_t>(value); break;
    case 0x14: std::memcpy(&s->mPosition.x, &value, 4); break;
    case 0x18: std::memcpy(&s->mPosition.y, &value, 4); break;
    case 0x1c: std::memcpy(&s->mPosition.z, &value, 4); break;
    case 0x24: s->mF24 = static_cast<int32_t>(value); break;
    case 0x28: s->mSprite = Engine(static_cast<int32_t>(value)); break;
    case 0x2c: std::memcpy(&s->mF2C, &value, 4); break;
    case 0x30:
        s->SetAngle(FromBits(value));
        gSpriteMode[SpriteNumber(s) - 1] = false;
        break;
    case 0x100:   // (not a Mac offset) the used width and height as the halfwords at 0x30 and 0x32
        s->mUsedWidth = static_cast<int16_t>(value >> 16);
        s->mUsedHeight = static_cast<int16_t>(value & 0xffff);
        gSpriteMode[SpriteNumber(s) - 1] = true;
        break;
    case 0x34: s->mUsedX = static_cast<int16_t>(value); break;
    case 0x36: s->mUsedY = static_cast<int16_t>(value); break;
    default: break;
    }
}
void SetEngineWord(SSpriteEngineData* e, int32_t offset, uint32_t value) {
    switch (offset) {
    case 0x00:
        // 0 none, 1..6 a single entry, 100 + k the array of slot k.
        if (value >= 100) {
            e->mTexture = gArrays[value - 100].first;
        } else {
            e->mTexture = Entry(static_cast<int32_t>(value));
        }
        break;
    case 0x04: e->mData = value != 0 ? gPixelBlock : nullptr; break;
    case 0x0c: e->mWidth = static_cast<uint16_t>(value); break;
    case 0x0e: e->mHeight = static_cast<uint16_t>(value); break;
    case 0x10: e->mUsedWidth = static_cast<uint16_t>(value); break;
    case 0x12: e->mUsedHeight = static_cast<uint16_t>(value); break;
    case 0x14: e->mOffsetX = static_cast<int16_t>(value); break;
    case 0x16: e->mPartCount = static_cast<uint16_t>(value); break;
    default: break;
    }
}

void PutRecord(const SPrimitive* p) {
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
    Put(VertexOffset(p->mVertexData));
    Put(IndexOffset(p->mIndexData));
    Put(CanonBits(p->mDepth));
    Put(static_cast<uint32_t>(p->mF34));
}

void PutSpriteRecord(const SSpriteDrawParameters& p) {
    Put(p.mFlags);
    if (p.mVertices == nullptr) {
        Put(kNoOffset);
    } else if (p.mVertices == gScratchVertices) {
        Put(0xfffffffeu);
    } else {
        Put(static_cast<uint32_t>(static_cast<const uint8_t*>(p.mVertices) - reinterpret_cast<const uint8_t*>(gInput)));
    }
    Put(static_cast<uint32_t>(p.mF08));
    Put(p.mBlendingMode);
    Put(static_cast<uint32_t>(MarkerNumber(p.mTexture)));
    Put(p.mF14);
    Put(CanonBits(p.mU0));
    Put(CanonBits(p.mV0));
    Put(CanonBits(p.mU1));
    Put(CanonBits(p.mV1));
    Put(static_cast<uint32_t>(p.mF28));
}

// The whole state, in the order tests/test_SpriteDraw.py reads the original's.
void PutState(int64_t result, const int32_t* heapBase) {
    Put(static_cast<uint32_t>(result));
    Put(static_cast<uint32_t>(Draw()->VertexSize()));
    Put(static_cast<uint32_t>(RecordId(Draw()->mCurrent)));
    Put(VertexOffset(Draw()->mVertexEnd));
    Put(IndexOffset(Draw()->mIndexEnd));
    Put(static_cast<uint32_t>(gNextRecord));
    Put(static_cast<uint32_t>(SpriteNumber(gHotSprite)));
    for (int32_t k = 0; k < kRecords; ++k) PutRecord(&gRecords[k]);
    for (int32_t i = 0; i < kVertexBytes / 4; ++i) Put(Canon(WordAt(gVertices + 4 * i)));
    for (int32_t i = 0; i < kIndexEntries; i += 2) {
        Put(static_cast<uint32_t>(gIndices[i]) | (static_cast<uint32_t>(gIndices[i + 1]) << 16));
    }
    PutSpriteRecord(gSprite);
    PutSpriteRecord(gScratchRecord);
    for (const SVertex& v : gScratchVertices) {
        Put(CanonBits(v.x));
        Put(CanonBits(v.y));
        Put(CanonBits(v.z));
        Put(CanonBits(v.w));
    }
    for (int32_t i = 1; i <= kSprites; ++i) {
        const SSpriteGameData* s = Sprite(i);
        Put(CanonBits(s->mScreenX));
        Put(CanonBits(s->mScreenY));
        Put(CanonBits(s->mDepth));
        Put(CanonBits(s->mScreenW));
        Put(static_cast<uint32_t>(s->mF10));
        Put(CanonBits(s->mPosition.x));
        Put(CanonBits(s->mPosition.y));
        Put(CanonBits(s->mPosition.z));
        Put(WordAt(s->mF20));
        Put(static_cast<uint32_t>(s->mF24));
        Put(static_cast<uint32_t>(EngineNumber(s->mSprite)));
        Put(CanonBits(s->mF2C));
        if (gSpriteMode[i - 1]) {
            Put((static_cast<uint32_t>(static_cast<uint16_t>(s->mUsedWidth)) << 16) | static_cast<uint16_t>(s->mUsedHeight));
        } else {
            Put(Canon(fbits(s->Angle())));
        }
        Put(static_cast<uint32_t>(static_cast<uint16_t>(s->mUsedX)));
        Put(static_cast<uint32_t>(static_cast<uint16_t>(s->mUsedY)));
        Put(WordAt(s->mF38));
        Put(WordAt(s->mF38 + 4));
        Put(static_cast<uint32_t>(SpriteNumber(s->mNext)));
    }
    for (int32_t i = 1; i <= kEngines; ++i) {
        const SSpriteEngineData* e = Engine(i);
        Put(static_cast<uint32_t>(EntryLabel(e->mTexture)));
        Put(e->mData != nullptr ? 1u : 0u);
        Put(CanonBits(e->mAspect));
        Put(e->mWidth);
        Put(e->mHeight);
        Put(e->mUsedWidth);
        Put(e->mUsedHeight);
        Put(static_cast<uint32_t>(static_cast<uint16_t>(e->mOffsetX)));
        Put(e->mPartCount);
    }
    for (int32_t i = 1; i <= kEntries; ++i) {
        const CTextureEntry* e = Entry(i);
        Put(e->mFlags);
        Put(static_cast<uint32_t>(MarkerNumber(e->mTexture)));
        Put(e->mData != nullptr ? 1u : 0u);
    }
    for (int32_t a = 0; a < kArrays; ++a) {
        const ArraySlot& slot = gArrays[a];
        const bool alive = slot.first != nullptr && lab(slot.block) == kLabelArray + a;
        Put(alive ? static_cast<uint32_t>(slot.count) : 0u);
        for (int32_t i = 0; i < kMaxPart; ++i) {
            if (alive && i < slot.count) {
                Put(slot.first[i].mFlags);
                Put(static_cast<uint32_t>(MarkerNumber(slot.first[i].mTexture)));
                Put(slot.first[i].mData != nullptr ? 1u : 0u);
            } else {
                Put(0);
                Put(0);
                Put(0);
            }
        }
    }
    CSpriteManager* m = Manager();
    Put(static_cast<uint32_t>(m->mF2404));
    Put(static_cast<uint32_t>(m->mF2408));
    Put(static_cast<uint32_t>(m->mF240C));
    Put(static_cast<uint32_t>(lab(m->mTextures)));
    for (int32_t i = 0; i < kShown; ++i) Put(i < m->mF2404 ? static_cast<uint32_t>(SpriteNumber(m->mSprites[i])) : 0u);
    for (int32_t i = 0; i < kShown; ++i) Put(i < m->mF2408 ? static_cast<uint32_t>(SpriteNumber(m->mPartialSprites[i])) : 0u);
    for (int32_t i = 0; i < 4; ++i) {
        uint32_t number = 0;
        for (int32_t l = 1; l <= kLists; ++l) {
            if (i < m->mF240C && m->mLists[i] == List(l)) number = static_cast<uint32_t>(l);
        }
        Put(number);
    }
    for (int32_t g = 0; g < kShownGroups; ++g) {
        const SSpriteGroup& group = gSpriteGroups[g];
        const auto index = [](const SSpriteGroup* p) -> uint32_t {
            if (p == nullptr) return 0xfffffffeu;
            if (reinterpret_cast<uintptr_t>(p) == ~static_cast<uintptr_t>(0)) return 0xffffffffu;
            return static_cast<uint32_t>(p - gSpriteGroups);
        };
        Put(index(group.mAbove));
        Put(index(group.mBelow));
        Put(group.mPage == ~static_cast<uintptr_t>(0) ? 0xffffffffu
                                                       : static_cast<uint32_t>(MarkerNumber(reinterpret_cast<const void*>(group.mPage))));
        Put(static_cast<uint32_t>(SpriteNumber(group.mFirst)));
        Put(static_cast<uint32_t>(group.mBlend));
    }
    for (int32_t k = 0; k < 4; ++k) Put(static_cast<uint32_t>(tpw_cl_heap[k] - heapBase[k]));
}

constexpr int32_t kStateWords = 1 + 5 + 1 + kRecords * 13 + kVertexBytes / 4 + kIndexEntries / 2 + 2 * 11 + 16 +
                                kSprites * 18 + kEngines * 9 + kEntries * 3 + kArrays * (1 + kMaxPart * 3) +
                                4 + kShown + kShown + 4 + kShownGroups * 5 + 4;

}  // namespace

extern "C" {

int32_t tpw_sd_state_words() { return kStateWords; }

// The description, in order (tests/test_SpriteDraw.py, `flat`):
//   the number of operations and, for each, eight words (code, seven arguments);
//   the display's flags, aspect, +0x938, hot x, hot y, the hot sprite's number and the system's renderer; the view angle; the light
//   manager's colour; the final matrix's 16 words; the height field (present, columns, rows, inverse cell sizes x and z, origin x
//   and z, cell sizes x and z, then its 192 heights, the field's own beginning at the 65th); the draw object (vertex size, current record, the ends of the vertex and
//   index memory as offsets, the next record to hand out); four records of 13 words; the fills of the vertex and index
//   memories; the 128 input words; the sprite record (11 words); the scratch record (11 words) and its 16 vertex words; the sine
//   table (256 words); six texture entries (flags, page marker, u offset, v offset, u size, v size, data); three engine sprites
//   (texture, data, aspect, width, height, used width, used height, x offset, y offset or part count); three arrays (count, then
//   four times flags, page marker, data); four sprites (20 words: x, y, depth, w, flags, position, filler, colour, engine, size,
//   angle, used width, used height, used x, used y, two fillers, mode: with a mode of 1 the field at Mac 0x30 holds the used width
//   and height, else the angle); two lists (count, then the four sprites); the manager's blob texture (an entry number).
// The state after each operation goes to `out`, kStateWords words each. Answers the number of operations run.
int32_t tpw_sd_run(const int64_t* description, uint32_t* out) {
    gOut = out;
    gCount = 0;
    Reader in{description, 0};
    const int32_t operations = in.i32();
    const int64_t* script = description + in.at;
    in.at += operations * 8;

    // Regions for Where().
    gRegionCount = 0;
    AddRegion(kVertices, gVertices, sizeof gVertices);
    AddRegion(kIndices, gIndices, sizeof gIndices);
    AddRegion(kInput, gInput, sizeof gInput);
    AddRegion(kScratchVertices, gScratchVertices, sizeof gScratchVertices);
    AddRegion(kScratchRecord, &gScratchRecord, sizeof gScratchRecord);
    for (int32_t i = 1; i <= kSprites; ++i) AddRegion(kLabelSprite + i - 1, Sprite(i), sizeof(SSpriteGameData));

    // The globals the functions read.
    std::memset(gSpriteGroups, 0, sizeof gSpriteGroups);
    std::memset(gMathStore, 0, sizeof gMathStore);
    std::memset(gDrawStore, 0, sizeof gDrawStore);
    new (gMathStore) FakeMath;
    new (gDrawStore) TestDraw;
    gei_MathUnit = Math();
    gei_DrawPrimitive = Draw();
    gei_Display.mFlags = in.u32();
    gei_Display.mF04 = in.f32();
    gei_Display.mF938 = in.f32();
    gSpriteHotX = in.f32();
    gSpriteHotY = in.f32();
    const int32_t hot = in.i32();
    gHotSprite = Sprite(hot);
    gei_System.mRenderer = in.i32();
    gViewAngle = in.f32();
    gLight.mF00 = 0;
    gLight.mF04 = in.i32();
    {
        uint32_t words[16];
        for (uint32_t& word : words) word = in.u32();
        std::memcpy(static_cast<void*>(Matrix()), words, sizeof words);
    }

    // The height field.
    std::memset(gHFieldStore, 0xa5, sizeof gHFieldStore);
    const bool hasField = in.i32() != 0;
    const int32_t columns = in.i32();
    const int32_t rows = in.i32();
    const float invX = in.f32();
    const float invZ = in.f32();
    const float originX = in.f32();
    const float originZ = in.f32();
    const float cellX = in.f32();
    const float cellZ = in.f32();
    if (gHeightField == nullptr) {
        gHeightField = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
        gHeights = static_cast<float*>(tpw::ArenaAllocZeroed(sizeof(float) * kHeights));
    }
    for (int32_t i = 0; i < kHeights; ++i) gHeights[i] = in.f32();
    if (hasField) {
        gHeightField->mF08[0] = originX;
        gHeightField->mF08[1] = originZ;
        gHeightField->mF08[2] = cellX;
        gHeightField->mF08[3] = cellZ;
        gHeightField->mHeights = gHeights + kHeightsBefore;
        HField()->mF00 = gHeightField;
        HField()->mGridWidth = columns;
        HField()->mGridHeight = rows;
        HField()->mInvCellX = invX;
        HField()->mInvCellZ = invZ;
    }

    // The draw object and its records.
    Draw()->SetVertexSize(in.i32());
    const int32_t current = in.i32();
    const uint32_t vertexEnd = in.u32();
    const uint32_t indexEnd = in.u32();
    gNextRecord = in.i32();
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
    const uint32_t vertexFill = in.u32();
    for (int32_t i = 0; i < kVertexBytes / 4; ++i) std::memcpy(gVertices + 4 * i, &vertexFill, 4);
    const uint16_t indexFill = static_cast<uint16_t>(in.u32());
    for (uint16_t& entry : gIndices) entry = indexFill;
    for (uint32_t& word : gInput) word = in.u32();

    // The sprite record and the scratch record.
    const auto readRecord = [&](SSpriteDrawParameters& p, void* vertices) {
        p.mFlags = in.u32();
        const uint32_t offset = in.u32();
        p.mVertices = vertices != nullptr ? vertices : (offset == kNoOffset ? nullptr : reinterpret_cast<uint8_t*>(gInput) + offset);
        p.mF08 = in.i32();
        p.mBlendingMode = in.u32();
        p.mTexture = Marker(in.i32());
        p.mF14 = in.u32();
        p.mU0 = in.f32();
        p.mV0 = in.f32();
        p.mU1 = in.f32();
        p.mV1 = in.f32();
        p.mF28 = in.i32();
    };
    readRecord(gSprite, nullptr);
    readRecord(gScratchRecord, gScratchVertices);
    for (SVertex& v : gScratchVertices) {
        v.x = in.f32();
        v.y = in.f32();
        v.z = in.f32();
        v.w = in.f32();
    }
    for (float& entry : gSineTable) entry = in.f32();

    // Entries, engine sprites, arrays, sprites, lists, the manager.
    for (int32_t i = 1; i <= kEntries; ++i) {
        CTextureEntry* e = Entry(i);
        std::memset(static_cast<void*>(e), 0xa5, sizeof(CTextureEntry));
        e->mFlags = in.u32();
        e->mTexture = Marker(in.i32());
        e->mUOffset = in.f32();
        e->mVOffset = in.f32();
        e->mUSize = in.f32();
        e->mVSize = in.f32();
        e->mData = in.i32() != 0 ? gPixelBlock : nullptr;
    }
    // Arrays first (the engine sprites point into them), then the engine sprites.
    const int64_t* engineWords = description + in.at;
    in.at += kEngines * 9;
    for (int32_t a = 0; a < kArrays; ++a) {
        const int32_t count = in.i32();
        uint32_t flags[kMaxPart];
        int32_t pages[kMaxPart];
        int32_t data[kMaxPart];
        for (int32_t i = 0; i < kMaxPart; ++i) {
            flags[i] = in.u32();
            pages[i] = in.i32();
            data[i] = in.i32();
        }
        gArrays[a] = {nullptr, nullptr, 0};
        if (count > 0) MakeArray(a, count > kMaxPart ? kMaxPart : count, flags, pages, data);
    }
    {
        Reader words{engineWords, 0};
        for (int32_t i = 1; i <= kEngines; ++i) {
            SSpriteEngineData* e = Engine(i);
            std::memset(static_cast<void*>(e), 0xa5, sizeof(SSpriteEngineData));
            const uint32_t texture = words.u32();
            e->mTexture = nullptr;
            SetEngineWord(e, 0x00, texture);
            e->mData = words.i32() != 0 ? gPixelBlock : nullptr;
            e->mAspect = words.f32();
            e->mWidth = static_cast<uint16_t>(words.i32());
            e->mHeight = static_cast<uint16_t>(words.i32());
            e->mUsedWidth = static_cast<uint16_t>(words.i32());
            e->mUsedHeight = static_cast<uint16_t>(words.i32());
            e->mOffsetX = static_cast<int16_t>(words.i32());
            e->mPartCount = static_cast<uint16_t>(words.i32());
        }
    }
    for (int32_t i = 1; i <= kSprites; ++i) {
        SSpriteGameData* s = Sprite(i);
        std::memset(static_cast<void*>(s), 0xa5, sizeof(SSpriteGameData));
        for (int32_t offset : {0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c}) SetSpriteWord(s, offset, in.u32());
        const uint32_t filler20 = in.u32();
        std::memcpy(s->mF20, &filler20, 4);
        SetSpriteWord(s, 0x24, in.u32());
        SetSpriteWord(s, 0x28, in.u32());
        SetSpriteWord(s, 0x2c, in.u32());
        const uint32_t angle = in.u32();
        const uint32_t usedWidth = in.u32();
        const uint32_t usedHeight = in.u32();
        SetSpriteWord(s, 0x34, in.u32());
        SetSpriteWord(s, 0x36, in.u32());
        const uint32_t filler38 = in.u32();
        const uint32_t filler3c = in.u32();
        std::memcpy(s->mF38, &filler38, 4);
        std::memcpy(s->mF38 + 4, &filler3c, 4);
        s->mNext = nullptr;
        gSpriteMode[i - 1] = in.i32() != 0;
        if (gSpriteMode[i - 1]) {
            s->mUsedWidth = static_cast<int16_t>(usedWidth);
            s->mUsedHeight = static_cast<int16_t>(usedHeight);
        } else {
            s->SetAngle(FromBits(angle));
        }
    }
    for (int32_t l = 1; l <= kLists; ++l) {
        CSpriteList* list = List(l);
        std::memset(static_cast<void*>(list), 0xa5, sizeof(CSpriteList));
        list->mCount = in.i32();
        for (int32_t i = 0; i < kSprites; ++i) gListInstances[l - 1][i] = Sprite(in.i32());
        list->mInstances = gListInstances[l - 1];
    }
    std::memset(gManagerStore, 0, sizeof gManagerStore);
    {
        CSpriteManager* m = Manager();
        m->mTextures = Entry(in.i32());
    }

    // Labels.
    tpw_cl_label(kLabelDraw, static_cast<CDrawPrimitive*>(Draw()));
    tpw_cl_label(kLabelMath, Math());
    tpw_cl_label(kLabelRender, &gRender);
    tpw_cl_label(kLabelLight, &gLight);
    tpw_cl_label(kLabelCache, gei_TCache);
    tpw_cl_label(kLabelMatrix, Matrix());
    tpw_cl_label(kLabelManager, Manager());
    tpw_cl_label(kLabelHField, HField());
    for (int32_t i = 0; i < kMarkers; ++i) tpw_cl_label(kLabelMarker + i, &gMarkers[i]);
    for (int32_t i = 1; i <= kEntries; ++i) tpw_cl_label(kLabelEntry + i - 1, Entry(i));
    for (int32_t i = 1; i <= kSprites; ++i) tpw_cl_label(kLabelSprite + i - 1, Sprite(i));
    for (int32_t i = 1; i <= kEngines; ++i) tpw_cl_label(kLabelEngine + i - 1, Engine(i));
    for (int32_t i = 1; i <= kLists; ++i) tpw_cl_label(kLabelList + i - 1, List(i));

    int32_t heapBase[4];
    for (int32_t k = 0; k < 4; ++k) heapBase[k] = tpw_cl_heap[k];

    for (int32_t n = 0; n < operations; ++n) {
        const int64_t* op = script + 8 * n;
        const int64_t* a = op + 1;
        ev("op", {n, op[0]});
        int64_t result = 0;
        switch (static_cast<Op>(op[0])) {
        case kOpPrepare:
            result = Sprite(static_cast<int32_t>(a[0]))->Prepare();
            break;
        case kOpCalcRegular:
            result = Sprite(static_cast<int32_t>(a[0]))->CalcRegular(&gScratchRecord);
            break;
        case kOpSetupVertices:
            result = Sprite(static_cast<int32_t>(a[0]))
                         ->SetupVertices(gScratchVertices, FromBits(static_cast<uint32_t>(a[1])), FromBits(static_cast<uint32_t>(a[2])));
            break;
        case kOpDrawRegular:
            result = Sprite(static_cast<int32_t>(a[0]))->DrawRegular();
            break;
        case kOpAddRegular:
            result = Sprite(static_cast<int32_t>(a[0]))->AddRegular();
            break;
        case kOpAddSprite:
            result = gei_DrawPrimitive->AddSprite(&gSprite, static_cast<int32_t>(a[0]));
            break;
        case kOpReset:
            Manager()->Reset();
            break;
        case kOpAddList:
            result = Manager()->AddList(List(static_cast<int32_t>(a[0])));
            break;
        case kOpManagerPrepare:
            result = Manager()->Prepare();
            break;
        case kOpManagerRender: {
            // A sprite added twice since the last Reset would link itself into its own kind and the original (and the
            // translation) would walk that ring for ever: such a frame is not rendered.
            bool twice = false;
            for (int32_t i = 0; i < Manager()->mF2404; ++i) {
                for (int32_t j = 0; j < i; ++j) twice = twice || Manager()->mSprites[i] == Manager()->mSprites[j];
            }
            result = twice ? -2 : Manager()->Render();
            break;
        }
        case kOpSetSprite:
            SetSpriteWord(Sprite(static_cast<int32_t>(a[0])), static_cast<int32_t>(a[1]), static_cast<uint32_t>(a[2]));
            break;
        case kOpSetEngine:
            SetEngineWord(Engine(static_cast<int32_t>(a[0])), static_cast<int32_t>(a[1]), static_cast<uint32_t>(a[2]));
            break;
        case kOpSetDisplay:
            gei_Display.mFlags = static_cast<uint32_t>(a[0]);
            break;
        case kOpSetEntry:
            Entry(static_cast<int32_t>(a[0]))->mFlags = static_cast<uint32_t>(a[1]);
            break;
        }
        PutState(result, heapBase);
    }
    return operations;
}

// ---- The leaves of the vertex record and the rotations, on their own ------------------------------------------------
// ReturnPtr on a record at offset `base` of a block of 32 words; answers the offset of the result from the block.
int64_t tpw_sd_return_ptr(uint8_t* block, int32_t size, int32_t index) {
    SFVF_TLCTEX* result = reinterpret_cast<SFVF_TLCTEX*>(block)->ReturnPtr(size, index);
    return reinterpret_cast<uint8_t*>(result) - block;
}
void tpw_sd_set_uv(uint8_t* block, int32_t set, float u, float v) { reinterpret_cast<SFVF_TLCTEX*>(block)->SetUV(set, u, v); }
void tpw_sd_set_diffuse(uint8_t* block, uint32_t colour) { reinterpret_cast<SFVF_TLCTEX*>(block)->SetDiffuse(colour); }
void tpw_sd_set_vertex(uint8_t* block, const float* vertex) {
    SVertex v;
    std::memcpy(&v, vertex, sizeof v);
    reinterpret_cast<SFVF_TLCTEX*>(block)->SetVertex(v);
}
void tpw_sd_rotate_y(float* vertices, float angle, float* centre, const float* table) {
    std::memcpy(gSineTable, table, sizeof gSineTable);
    RotateVertex_Y(reinterpret_cast<SVertex*>(vertices), angle, reinterpret_cast<SVertex*>(centre));
}
void tpw_sd_rotate_z(float* vertices, float angle) { RotateVertex_Z(reinterpret_cast<SVertex*>(vertices), angle); }

}  // extern "C"
