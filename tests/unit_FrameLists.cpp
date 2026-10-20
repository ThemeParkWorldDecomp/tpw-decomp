// Fakes and the test harness for the frame's lists (src/tb_renderlist.cpp, src/tb_sprites.cpp,
// src/tb_starfield_frame.cpp): the render list, the sprite manager and the starfield.
//
// Everything the three files call outside themselves is a fake here that records its name and
// every argument through calllog (objects as the test's labels, pointers into a block as
// label * 0x1000 + offset): the render memory, the math unit, the draw primitive, the
// sprites' own drawing members, the render list entry's members, the texture cache, the
// texture entries, the error stack and the allocator. Their answers come from queues the
// test fills (ret). tests/test_FrameLists.py builds the same world on the PowerPC side from
// the same description and compares the logs, the answers and the state dumps.
//
// The world is built piece by piece through tpw_fl_make (one call per object, kinds below) so
// that the Python half and this one share the description. Objects that hold 32-bit pointer
// fields of a loaded file (nodes, face sets, texture records, texture objects and the blocks
// they point to) come from the arena, as the game's do.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "MapWho.h"
#include "Wavelet.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_loadmanager.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"
#include "tb_starfield.h"
#include "tb_textureentry.h"
#include "tb_texturecache.h"

using namespace calllog;

// ---- The test's blocks -------------------------------------------------------------------
namespace {

struct Block {
    int32_t label;
    uint8_t* base;
    size_t size;
    bool arena;
    bool owned = true;   // false: a view of something else (a page of the slab, a new[] array)
};
std::vector<Block> gBlocks;

uint8_t* BlockOf(int64_t label) {
    for (const Block& b : gBlocks)
        if (b.label == label) return b.base;
    return nullptr;
}

// A pointer as label * 0x1000 + offset when it lies in one of the blocks, a small number as
// it is, anything else -1.
int64_t Where(const void* pointer) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(pointer);
    if (p == 0) return 0;
    for (const Block& b : gBlocks) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(b.base);
        if (p >= base && p < base + b.size) return static_cast<int64_t>(b.label) * 0x1000 + static_cast<int64_t>(p - base);
    }
    if (p < 0x10000) return static_cast<int64_t>(p);
    return -1;
}

uint8_t* NewBlock(int32_t label, size_t size, int fill, bool inArena) {
    if (size < 16) size = 16;
    uint8_t* base = static_cast<uint8_t*>(inArena ? tpw::ArenaAlloc(size) : std::malloc(size));
    std::memset(base, fill, size);
    gBlocks.push_back({label, base, size, inArena});
    tpw_cl_label(label, base);
    return base;
}

void FreeBlocks() {
    for (const Block& b : gBlocks) {
        if (!b.owned) continue;
        if (b.arena) {
            tpw::ArenaFree(b.base);
        } else {
            std::free(b.base);
        }
    }
    gBlocks.clear();
}

// The two matrix events every fake that is given a matrix records.
void MatrixEvents(const void* matrix) {
    uint32_t w[16];
    std::memcpy(w, matrix, sizeof w);
    ev("Matrix", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("Matrix", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

}  // namespace

// ---- Fakes of what the unit calls and does not contain -----------------------------------
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }

void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}

uint32_t gRenderEntryFlags = 0;

namespace {
alignas(16) unsigned char gMapStore[sizeof(CMapWho)];
alignas(16) unsigned char gRenderMemoryStore[sizeof(CRenderMemory)];
alignas(16) unsigned char gCacheStore[sizeof(CTextureCache)];
alignas(16) unsigned char gLoadStore[sizeof(CTextureLoadManager)];
}  // namespace
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapStore);
CRenderMemory* gei_RenderMemory = reinterpret_cast<CRenderMemory*>(gRenderMemoryStore);
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
CTextureLoadManager* gei_TLoadManager = reinterpret_cast<CTextureLoadManager*>(gLoadStore);

uint8_t* CRenderMemory::GetVertexBufferPtr(int32_t count) {
    ev("GetVertexBufferPtr", {lab(this), count});
    return BlockOf(ret("VB", 0));
}
int32_t CRenderMemory::ExecuteUnrenderedPrimitives() {
    ev("ExecuteUnrendered", {lab(this)});
    return static_cast<int32_t>(ret("EU", 1));
}

void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("TCachePrepare", {lab(this), lab(entry), fbits(distance)});
}

CTextureEntry* CTextureLoadManager::LoadFile(char* name) {
    ev("LoadFile", {lab(this)}, {name});
    return reinterpret_cast<CTextureEntry*>(BlockOf(ret("LF", 0)));
}

CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() { ev("Entry.dtor"); }
int32_t CTextureEntry::Close() {
    ev("EntryClose", {lab(this)});
    return 1;
}
uint32_t CTextureEntry::GetBlendingMode() {
    ev("GetBlendingMode", {lab(this)});
    return static_cast<uint32_t>(ret("GBM", 0));
}

namespace {
// The engine allocator's free: records, then clears the caller's pointer.
void FakeFree(void*& block) {
    ev("Free", {lab(block)});
    block = nullptr;
}
}  // namespace
TbFreeFn gFree = FakeFree;

CDisplay gei_Display;
CDisplay::CDisplay() {}
CTextureSettings gei_TextureSettings;
CTextureSettings::CTextureSettings() {}
CMatrix* gei_FinalMatrix = nullptr;

// The draw primitive: the members the lists call record; everything virtual is unused.
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, Where(texture)});
    return reinterpret_cast<SPrimitive*>(BlockOf(ret("GNP", 0)));
}
int32_t CDrawPrimitive::ExecuteCurrentPrimitive(float depth) {
    ev("ExecuteCurrent", {lab(this), fbits(depth)});
    return static_cast<int32_t>(ret("EX", 1));
}
int32_t CDrawPrimitive::AddPoly_TLCTEX(SFVF_TLCTEX* vertices, int32_t count, int32_t stride, ClipCodeType* clip,
                                       int32_t flags) {
    ev("AddPoly", {lab(this), count, stride, lab(clip), flags});
    for (int32_t i = 0; i < count && i < 4; ++i) {
        const SFVF_TLCTEX& v = *reinterpret_cast<const SFVF_TLCTEX*>(reinterpret_cast<const uint8_t*>(vertices) + i * stride);
        // (the specular word is not written by the caller and not recorded)
        ev("Vertex", {fbits(v.x), fbits(v.y), fbits(v.z), fbits(v.rhw), v.colour, fbits(v.u), fbits(v.v)});
    }
    return static_cast<int32_t>(ret("AP", 1));
}

namespace {
class FakeDraw : public CDrawPrimitive {
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
FakeDraw gDraw;
}  // namespace
CDrawPrimitive* gei_DrawPrimitive = &gDraw;

// The render list entry's members.
int32_t SRenderListEntry::GetFaceSetBlendMode(sFaceSet* faceSet, CTextureEntry* texture) {
    ev("GetFaceSetBlendMode", {lab(this), lab(faceSet), lab(texture)});
    return static_cast<int32_t>(ret("GFB", 0x1234));
}
SPrimitive* SRenderListEntry::SetupPrimitive(int32_t faceSet, int32_t* extra) {
    ev("SetupPrimitive", {lab(this), faceSet});
    *extra = static_cast<int32_t>(ret("SPX", 0));
    return reinterpret_cast<SPrimitive*>(BlockOf(ret("SP", 0)));
}
int32_t SRenderListEntry::CalcRenderEntry(SPrimitive* primitive, int32_t faceSet, int32_t extra) {
    ev("CalcRenderEntry", {lab(this), Where(primitive), faceSet, extra});
    return static_cast<int32_t>(ret("CRE", 1));
}

// The sprites' own members.
int32_t SSpriteGameData::Prepare() {
    ev("SpritePrepare", {lab(this)});
    return 1;
}
int32_t SSpriteGameData::DrawRegularShadow() {
    ev("DrawRegularShadow", {lab(this)});
    return static_cast<int32_t>(ret("DRS", 1));
}
int32_t SSpriteGameData::AddRegularShadow(CTextureEntry* shadowTexture) {
    ev("AddRegularShadow", {lab(this), lab(shadowTexture)});
    return static_cast<int32_t>(ret("ARS", 1));
}
int32_t SSpriteGameData::DrawRegular() {
    ev("DrawRegular", {lab(this)});
    return static_cast<int32_t>(ret("DR", 1));
}
int32_t SSpriteGameData::AddRegular() {
    ev("AddRegular", {lab(this)});
    return static_cast<int32_t>(ret("AR", 1));
}
int32_t SSpriteGameData::DrawPartial() {
    ev("DrawPartial", {lab(this)});
    return static_cast<int32_t>(ret("DP", 1));
}

// CMatrix::TransformVertex: records the matrix and the vertex in, and writes four words from
// the queues TV0..TV3 (so the test chooses what comes out).
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    uint32_t w[3];
    std::memcpy(w, in, sizeof w);
    ev("TransformVertex", {w[0], w[1], w[2], type});
    MatrixEvents(this);
    uint32_t result[4];
    result[0] = static_cast<uint32_t>(ret("TV0", 0));
    result[1] = static_cast<uint32_t>(ret("TV1", 0));
    result[2] = static_cast<uint32_t>(ret("TV2", 0));
    result[3] = static_cast<uint32_t>(ret("TV3", 0));
    std::memcpy(out, result, sizeof result);
}

// The math unit: Transform records; MultMatrix records both matrices and writes
// a + 3 * b + index as words (so the product is exact on both sides).
CTransformBase::~CTransformBase() {}
namespace {
class FakeMath : public CTransformBase {
public:
    ~FakeMath() override {}
    void Transform(sMatrix* m, void* out, sVertexPosnSet* in, int32_t outStride, int32_t first, int32_t count) override {
        ev("Transform", {lab(this), Where(out), Where(in), outStride, first, count});
        MatrixEvents(m);
    }
    void Transform(sMatrix*, void*, void*, int32_t, int32_t, int32_t, int32_t) override { ev("Transform2"); }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float* a, const float* b, float* out) override {
        ev("MultMatrix", {lab(this), Where(out)});
        MatrixEvents(a);
        MatrixEvents(b);
        uint32_t wa[16], wb[16], wo[16];
        std::memcpy(wa, a, sizeof wa);
        std::memcpy(wb, b, sizeof wb);
        for (int i = 0; i < 16; ++i) wo[i] = wa[i] + 3 * wb[i] + static_cast<uint32_t>(i);
        std::memcpy(out, wo, sizeof wo);
    }
};
FakeMath gMath;
}  // namespace
CTransformBase* gei_MathUnit = &gMath;

// ---- Building the world --------------------------------------------------------------------
namespace {

constexpr int kPagesMax = 32;
uint8_t* gPageSlab = nullptr;
uint8_t* gEntrySlab = nullptr;
// Arrays made with new[] (texture entries): freed by the test at the end if still live.
std::vector<CTextureEntry*> gArrays;
// What a pointer field of an object under test holds before the code under test writes it.
const uintptr_t kDirty = static_cast<uintptr_t>(0xa5a5a5a5a5a5a5a5ull);

CRenderList* gList = nullptr;

// The size of the cookie new[] puts before an array of an object with a destructor (the
// library's operator delete[] is given the cookie's address). Measured with a type of the
// same alignment.
struct CookieProbe {
    ~CookieProbe() {}
    static void* operator new[](size_t size) {
        gProbeBlock = std::malloc(size);
        return gProbeBlock;
    }
    static void operator delete[](void* block) { std::free(block); }
    static void* gProbeBlock;
    double mPad;
};
void* CookieProbe::gProbeBlock = nullptr;
static_assert(alignof(CookieProbe) == alignof(CTextureEntry), "the probe needs the entry's alignment");
size_t gCookie = 0;
size_t CookieSize() {
    if (gCookie == 0) {
        CookieProbe* probe = new CookieProbe[2];
        gCookie = reinterpret_cast<uint8_t*>(probe) - static_cast<uint8_t*>(CookieProbe::gProbeBlock);
        delete[] probe;
    }
    return gCookie;
}

void FillEntryLabels(CRenderList* list) {
    for (int i = 0; i < 16; ++i) tpw_cl_label(300 + i, &list->mEntries[i]);
}

}  // namespace

extern "C" {

// Empties the world and makes the objects every case has: the math unit (label 2), the
// render memory (3), the draw primitive (4), the texture cache (5), the load manager (6) and
// the map (7). Call after tpw_cl_reset.
void tpw_fl_reset() {
    FreeBlocks();
    gArrays.clear();
    if (gPageSlab == nullptr) gPageSlab = static_cast<uint8_t*>(std::malloc(kPagesMax * 64));
    std::memset(gPageSlab, 0, kPagesMax * 64);
    if (gEntrySlab == nullptr) gEntrySlab = static_cast<uint8_t*>(std::malloc(kPagesMax * sizeof(CTextureEntry)));
    gList = nullptr;
    tpw_cl_label(2, &gMath);
    tpw_cl_label(3, gei_RenderMemory);
    tpw_cl_label(4, &gDraw);
    tpw_cl_label(5, gei_TCache);
    tpw_cl_label(6, gei_TLoadManager);
    tpw_cl_label(7, gei_MapWho);
    gei_MathUnit = &gMath;
    gei_DrawPrimitive = &gDraw;
    gei_FinalMatrix = nullptr;
    gei_Display.mFlags = 0;
    gei_Display.mF04 = 0.0f;
    gHotSprite = nullptr;
    gei_TextureSettings.mFlags = 0;
    gRenderEntryFlags = 0;
    gei_MapWho->mF30 = nullptr;
    gDraw.mFields[2] = 0;
    gDraw.mCurrent = nullptr;
    // The work arrays start out filled with a marker the dumps stop at.
    std::memset(gRenderFaces, 0xa5, sizeof(SRenderFace) * 256);
    std::memset(gRenderGroups, 0xa5, sizeof(SRenderGroup) * 64);
    std::memset(gSpriteGroups, 0xff, sizeof gSpriteGroups);
    // gFree is the one the test library installs.
    gFree = FakeFree;
}

// After the log has been read: frees what the case made and the code under test did not.
void tpw_fl_cleanup() {
    for (CTextureEntry* array : gArrays) {
        if (lab(reinterpret_cast<uint8_t*>(array) - CookieSize()) > 0) delete[] array;
    }
    gArrays.clear();
    FreeBlocks();
}

// State the code under test reads. what: 0 texture settings flags, 1 render entry flags,
// 2 the map's list being prepared (label of a block), 3 the draw primitive's instant mask,
// 4 display flags, 5 display mF04 (float bits), 6 display hot sprite (a label, 0 for none,
// -1 for a stale pointer), 7 the draw primitive's current primitive (the same coding),
// 8 the final matrix (a label of a matrix block, 0 for none).
void tpw_fl_set(int32_t what, int64_t value) {
    switch (what) {
    case 0:
        gei_TextureSettings.mFlags = static_cast<uint32_t>(value);
        break;
    case 1:
        gRenderEntryFlags = static_cast<uint32_t>(value);
        break;
    case 2:
        gei_MapWho->mF30 = reinterpret_cast<SMapLinkType*>(BlockOf(value));
        break;
    case 3:
        gDraw.mFields[2] = static_cast<uint32_t>(value);
        break;
    case 4:
        gei_Display.mFlags = static_cast<uint32_t>(value);
        break;
    case 5: {
        uint32_t bits = static_cast<uint32_t>(value);
        std::memcpy(&gei_Display.mF04, &bits, 4);
        break;
    }
    case 6:
        if (value < 0) {
            gHotSprite = reinterpret_cast<SSpriteGameData*>(kDirty);
        } else {
            gHotSprite = reinterpret_cast<SSpriteGameData*>(BlockOf(value));
        }
        break;
    case 7:
        if (value < 0) {
            gDraw.mCurrent = reinterpret_cast<SPrimitive*>(kDirty);
        } else {
            gDraw.mCurrent = reinterpret_cast<SPrimitive*>(BlockOf(value));
        }
        break;
    case 8:
        gei_FinalMatrix = reinterpret_cast<CMatrix*>(BlockOf(value));
        break;
    }
}

// Makes an object. kinds (v is the list of numbers after the label):
//   0 page            (order)                      an address that compares by `order`
//   1 texture entry   (flags, page label, place in address order)
//   2 texture object  (eight texture entry labels, 0 for none)
//   3 texture record  (frame, texture object label)
//   4 node            (flags, vertexCount, faceSets, vertexBlock, pointsBlock, wordsBlock,
//                      then per face set: texture record label, face set label)
//   5 block           (size, fill byte, arena); tpw_fl_words sets words
//   6 sprite          (depth bits, flags, sprite data label)
//   7 sprite data     (texture entry label)
//   8 sprite list     (count, then the instance labels)
//   9 sprite manager (blob shadow texture array label or 0)
//  10 starfield       (count, mF04, texture array label, star block label, size bits, mF14, matrix label)
//  11 texture array   (count, then per entry: flags, page label, u offset, v offset, u size,
//                      v size as float bits)
//  12 matrix          (sixteen words)
//  13 render list
void tpw_fl_make(int32_t kind, int32_t label, const int64_t* v) {
    switch (kind) {
    case 0: {
        uint8_t* page = gPageSlab + v[0] * 64;
        gBlocks.push_back({label, page, 64, false, false});
        tpw_cl_label(label, page);
        break;
    }
    case 1: {
        // (v[2]: the entry's place in address order, as for pages)
        uint8_t* block = gEntrySlab + v[2] * sizeof(CTextureEntry);
        std::memset(block, 0xa5, sizeof(CTextureEntry));
        gBlocks.push_back({label, block, sizeof(CTextureEntry), false, false});
        tpw_cl_label(label, block);
        CTextureEntry* entry = reinterpret_cast<CTextureEntry*>(block);
        entry->mFlags = static_cast<uint32_t>(v[0]);
        entry->mTexture = reinterpret_cast<CTextureBase*>(BlockOf(v[1]));
        break;
    }
    case 2: {
        CsTexture* texture = static_cast<CsTexture*>(tpw::ArenaAllocZeroed(sizeof(CsTexture)));
        gBlocks.push_back({label, reinterpret_cast<uint8_t*>(texture), sizeof(CsTexture), true});
        tpw_cl_label(label, texture);
        CTextureEntry** entries = reinterpret_cast<CTextureEntry**>(NewBlock(label + 200, 8 * sizeof(void*), 0, false));
        for (int i = 0; i < 8; ++i) entries[i] = reinterpret_cast<CTextureEntry*>(BlockOf(v[i]));
        texture->mEntries = entries;
        texture->mEntryCount = 8;
        break;
    }
    case 3: {
        sTexture* record = static_cast<sTexture*>(tpw::ArenaAllocZeroed(sizeof(sTexture)));
        gBlocks.push_back({label, reinterpret_cast<uint8_t*>(record), sizeof(sTexture), true});
        tpw_cl_label(label, record);
        record->mF00 = 0xbeef;
        record->mF02 = static_cast<uint16_t>(v[0]);
        record->mTexture = reinterpret_cast<CsTexture*>(BlockOf(v[1]));
        break;
    }
    case 4: {
        tag_sNode* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        gBlocks.push_back({label, reinterpret_cast<uint8_t*>(node), sizeof(tag_sNode), true});
        tpw_cl_label(label, node);
        node->mFlags = static_cast<uint32_t>(v[0]);
        node->mVertexCount = static_cast<uint16_t>(v[1]);
        const int sets = static_cast<int>(v[2]);
        node->mFaceSetCount = static_cast<uint16_t>(sets);
        node->mVertices = reinterpret_cast<sVertexPosnSet*>(BlockOf(v[3]));
        node->mPoints = reinterpret_cast<s3DPoint*>(BlockOf(v[4]));
        node->mMeshWords = reinterpret_cast<uint32_t*>(BlockOf(v[5]));
        sFaceSet* sets_ = static_cast<sFaceSet*>(tpw::ArenaAllocZeroed((sets > 0 ? sets : 1) * sizeof(sFaceSet)));
        gBlocks.push_back({label + 200, reinterpret_cast<uint8_t*>(sets_), (sets > 0 ? sets : 1) * sizeof(sFaceSet), true});
        for (int i = 0; i < sets; ++i) {
            sets_[i].mTexture = reinterpret_cast<sTexture*>(BlockOf(v[6 + 2 * i]));
            sets_[i].mFirstFace = 0x5a5a;
            tpw_cl_label(static_cast<int32_t>(v[7 + 2 * i]), &sets_[i]);
        }
        node->mFaceSets = sets_;
        break;
    }
    case 5:
        NewBlock(label, static_cast<size_t>(v[0]), static_cast<int>(v[1]), v[2] != 0);
        break;
    case 6: {
        SSpriteGameData* sprite = reinterpret_cast<SSpriteGameData*>(NewBlock(label, sizeof(SSpriteGameData), 0, false));
        const uint32_t bits = static_cast<uint32_t>(v[0]);
        std::memcpy(&sprite->mDepth, &bits, 4);
        sprite->mF10 = static_cast<int32_t>(v[1]);
        sprite->mSprite = reinterpret_cast<SSpriteEngineData*>(BlockOf(v[2]));
        sprite->mNext = reinterpret_cast<SSpriteGameData*>(kDirty);
        break;
    }
    case 7: {
        SSpriteEngineData* data = reinterpret_cast<SSpriteEngineData*>(NewBlock(label, sizeof(SSpriteEngineData), 0, false));
        data->mTexture = reinterpret_cast<CTextureEntry*>(BlockOf(v[0]));
        break;
    }
    case 8: {
        CSpriteList* list = reinterpret_cast<CSpriteList*>(NewBlock(label, sizeof(CSpriteList), 0, false));
        const int n = static_cast<int>(v[0]);
        list->mCount = n;
        SSpriteGameData** instances = reinterpret_cast<SSpriteGameData**>(NewBlock(label + 200, (n > 0 ? n : 1) * sizeof(void*), 0, false));
        for (int i = 0; i < n; ++i) instances[i] = reinterpret_cast<SSpriteGameData*>(BlockOf(v[1 + i]));
        list->mInstances = instances;
        break;
    }
    case 9: {
        CSpriteManager* manager = reinterpret_cast<CSpriteManager*>(NewBlock(label, sizeof(CSpriteManager), 0, false));
        manager->mTextures = reinterpret_cast<CTextureEntry*>(BlockOf(v[0]));
        break;
    }
    case 10: {
        CStarfield* star = reinterpret_cast<CStarfield*>(NewBlock(label, sizeof(CStarfield), 0, false));
        star->mCount = static_cast<int32_t>(v[0]);
        const uint32_t size = static_cast<uint32_t>(v[4]);
        std::memcpy(&star->mSize, &size, 4);
        star->mF04 = static_cast<int32_t>(v[1]);
        star->mTexture = reinterpret_cast<CTextureEntry*>(BlockOf(v[2]));
        star->mStars = reinterpret_cast<float*>(BlockOf(v[3]));
        star->mF14 = static_cast<int32_t>(v[5]);
        star->mMatrix = reinterpret_cast<CMatrix*>(BlockOf(v[6]));
        break;
    }
    case 11: {
        const int n = static_cast<int>(v[0]);
        CTextureEntry* array = new CTextureEntry[n];
        gArrays.push_back(array);
        // The array's label is its first entry's address; the block operator delete[] is given
        // (the library's array cookie, before the first entry) is label + 80.
        tpw_cl_label(label, array);
        tpw_cl_label(label + 80, reinterpret_cast<uint8_t*>(array) - CookieSize());
        gBlocks.push_back({label, reinterpret_cast<uint8_t*>(array), n * sizeof(CTextureEntry), false, false});
        for (int i = 0; i < n; ++i) {
            std::memset(static_cast<void*>(&array[i]), 0xa5, sizeof(CTextureEntry));
            array[i].mFlags = static_cast<uint32_t>(v[1 + 6 * i]);
            array[i].mTexture = reinterpret_cast<CTextureBase*>(BlockOf(v[2 + 6 * i]));
            const uint32_t uv[4] = {static_cast<uint32_t>(v[3 + 6 * i]), static_cast<uint32_t>(v[4 + 6 * i]),
                                    static_cast<uint32_t>(v[5 + 6 * i]), static_cast<uint32_t>(v[6 + 6 * i])};
            std::memcpy(&array[i].mUOffset, &uv[0], 4);   // u offset, v offset, u size, v size
            std::memcpy(&array[i].mVOffset, &uv[1], 4);
            std::memcpy(&array[i].mUSize, &uv[2], 4);
            std::memcpy(&array[i].mVSize, &uv[3], 4);
            tpw_cl_label(label + 1 + i, &array[i]);   // (arrays have at most four entries)
        }
        break;
    }
    case 12: {
        uint8_t* block = NewBlock(label, 64, 0, false);
        uint32_t words[16];
        for (int i = 0; i < 16; ++i) words[i] = static_cast<uint32_t>(v[i]);
        std::memcpy(block, words, sizeof words);
        break;
    }
    case 13: {
        CRenderList* list = reinterpret_cast<CRenderList*>(NewBlock(label, sizeof(CRenderList), 0xa5, false));
        list->mCount = 0;
        gList = list;
        FillEntryLabels(list);
        break;
    }
    }
}

// Sets a block's first words (stars, matrices). words: n 32-bit values.
void tpw_fl_words(int32_t label, const int64_t* words, int32_t n) {
    uint32_t* block = reinterpret_cast<uint32_t*>(BlockOf(label));
    for (int i = 0; i < n; ++i) block[i] = static_cast<uint32_t>(words[i]);
}

}  // extern "C"

// ---- Running and dumping ----------------------------------------------------------------
namespace {

int64_t* gOut = nullptr;
int32_t gCount = 0;
int32_t gCapacity = 0;
void put(int64_t value) {
    if (gCount < gCapacity) gOut[gCount] = value;
    ++gCount;
}
int64_t FaceIndex(const SRenderFace* face) { return face == nullptr ? 0 : (face - gRenderFaces) + 1; }
int64_t RenderGroupIndex(const SRenderGroup* group) { return group == nullptr ? 0 : (group - gRenderGroups) + 1; }
int64_t SpriteGroupIndex(const SSpriteGroup* group) {
    if (reinterpret_cast<uintptr_t>(group) == ~static_cast<uintptr_t>(0)) return -1;
    return group == nullptr ? 0 : (group - gSpriteGroups) + 1;
}

}  // namespace

extern "C" {

// functions: 0 CRenderList::Reset, 1 AddEntry(list, view, second, node, flags, lights),
// 2 RenderByTexture, 3 Render; 10 CSpriteManager::Reset, 11 Prepare, 12 Render,
// 13 AddSprite(manager, sprite, times), 14 AddPartialSprite, 15 AddList, 16 RenderSprites,
// 17 RenderPartialSprites, 18 LoadBlobShadow(manager, 0 for a NULL name, 1 for one);
// 20 CStarfield::Prepare, 21 Execute, 22 Close, 23 the destructor; 30 sets a sprite
// manager's counts, 31 a render list's count.
int64_t tpw_fl_run(int32_t function, const int64_t* a) {
    uint8_t* object = BlockOf(a[0]);
    if (function == 30) {   // the three counts of a sprite manager
        CSpriteManager* manager = reinterpret_cast<CSpriteManager*>(object);
        manager->mF2404 = static_cast<int32_t>(a[1]);
        manager->mF2408 = static_cast<int32_t>(a[2]);
        manager->mF240C = static_cast<int32_t>(a[3]);
        return 0;
    }
    if (function == 31) {   // the entry count of a render list
        reinterpret_cast<CRenderList*>(object)->mCount = static_cast<int32_t>(a[1]);
        return 0;
    }
    if (function < 10) {
        CRenderList* list = reinterpret_cast<CRenderList*>(object);
        switch (function) {
        case 0:
            list->Reset();
            return 0;
        case 1:
            return list->AddEntry(*reinterpret_cast<CMatrix*>(BlockOf(a[1])), *reinterpret_cast<CMatrix*>(BlockOf(a[2])),
                                  reinterpret_cast<CLight**>(BlockOf(a[5])), reinterpret_cast<tag_sNode*>(BlockOf(a[3])),
                                  static_cast<int32_t>(a[4]));
        case 2: return list->RenderByTexture();
        case 3: return list->Render();
        }
    } else if (function < 20) {
        CSpriteManager* manager = reinterpret_cast<CSpriteManager*>(object);
        int64_t successes = 0;
        switch (function) {
        case 10:
            manager->Reset();
            return 0;
        case 11: return manager->Prepare();
        case 12: return manager->Render();
        case 13:
            for (int64_t i = 0; i < a[2]; ++i) successes += manager->AddSprite(reinterpret_cast<SSpriteGameData*>(BlockOf(a[1])));
            return successes;
        case 14:
            for (int64_t i = 0; i < a[2]; ++i) successes += manager->AddPartialSprite(reinterpret_cast<SSpriteGameData*>(BlockOf(a[1])));
            return successes;
        case 15:
            for (int64_t i = 0; i < a[2]; ++i) successes += manager->AddList(reinterpret_cast<CSpriteList*>(BlockOf(a[1])));
            return successes;
        case 16: return manager->RenderSprites();
        case 17: return manager->RenderPartialSprites();
        case 18: {
            char path[] = "blob.tga";
            return manager->LoadBlobShadow(a[1] != 0 ? path : nullptr);
        }
        }
    } else {
        CStarfield* star = reinterpret_cast<CStarfield*>(object);
        switch (function) {
        case 20: return star->Prepare();
        case 21: return star->Execute();
        case 22: return star->Close();
        case 23:
            star->~CStarfield();
            return 0;
        }
    }
    return -1;
}

// What the code under test changed. what: 0 the render list (a[0] its label), 1 the sprite
// manager (a[0] its label, then the sprites' labels from a[1], n of them), 2 the starfield
// (a[0] its label, a[1] the label of the primitive it was given).
int32_t tpw_fl_dump(int32_t what, const int64_t* a, int32_t n, int64_t* out, int32_t capacity) {
    gOut = out;
    gCount = 0;
    gCapacity = capacity;
    uint8_t* object = BlockOf(a[0]);
    if (what == 0) {
        CRenderList* list = reinterpret_cast<CRenderList*>(object);
        put(lab(list));
        put(list->mCount);
        for (int i = 0; i < 12; ++i) {
            const SRenderListEntry& e = list->mEntries[i];
            put(lab(e.mLink));
            put(lab(e.mNode));
            put(e.mFlags);
            put(Where(e.mVertices));
            put(Where(e.mData));
        }
        for (int i = 0; i < 64; ++i) {
            const SRenderFace& f = gRenderFaces[i];
            if (reinterpret_cast<uintptr_t>(f.mEntry) == kDirty) break;
            put(1);
            put(lab(f.mEntry));
            put(lab(f.mTexture));
            put(f.mBlendMode);
            put(f.mSorted);
            put(f.mFaceSet);
            put(FaceIndex(f.mNext));
        }
        put(-100);
        for (int i = 0; i < 32; ++i) {
            const SRenderGroup& g = gRenderGroups[i];
            if (g.mPage == kDirty) break;
            put(lab(reinterpret_cast<void*>(g.mPage)));
            put(RenderGroupIndex(g.mAbove));
            put(RenderGroupIndex(g.mBelow));
            put(FaceIndex(g.mSorted));
            put(FaceIndex(g.mUnsorted));
        }
    } else if (what == 1) {
        CSpriteManager* m = reinterpret_cast<CSpriteManager*>(object);
        put(lab(m));
        put(m->mF2404);
        put(m->mF2408);
        put(m->mF240C);
        put(lab(m->mTextures));
        put(m->mTextures != nullptr ? m->mTextures->mFlags : 0);   // (the blob shadow's flags: LoadBlobShadow sets them)
        for (int i = 0; i < m->mF2404 && i < CSpriteManager::kMaxSprites; ++i) put(lab(m->mSprites[i]));
        put(-100);
        // (the Mac's partial sprite table has room for 0x80 only: see tb_starfield.h)
        for (int i = 0; i < m->mF2408 && i < 0x80; ++i) put(lab(m->mPartialSprites[i]));
        put(-100);
        for (int i = 0; i < m->mF240C && i < CSpriteManager::kMaxLists; ++i) put(lab(m->mLists[i]));
        put(-100);
        for (int i = 0; i < 64; ++i) {
            const SSpriteGroup& g = gSpriteGroups[i];
            if (g.mPage == ~static_cast<uintptr_t>(0)) break;
            put(lab(reinterpret_cast<void*>(g.mPage)));
            put(SpriteGroupIndex(g.mAbove));
            put(SpriteGroupIndex(g.mBelow));
            put(lab(g.mFirst));
            put(g.mBlend);
        }
        put(-100);
        for (int32_t i = 0; i < n; ++i) {
            const SSpriteGameData* s = reinterpret_cast<const SSpriteGameData*>(BlockOf(a[1 + i]));
            put(lab(s->mNext));
        }
        put(lab(gHotSprite));
        put(lab(gDraw.mCurrent));
    } else {
        CStarfield* s = reinterpret_cast<CStarfield*>(object);
        put(lab(s));
        put(s->mCount);
        put(s->mF04);
        put(lab(s->mTexture));
        put(lab(s->mStars));
        put(fbits(s->mSize));
        put(s->mF14);
        put(lab(s->mMatrix));
        const uint32_t* primitive = reinterpret_cast<const uint32_t*>(BlockOf(a[1]));
        put(primitive != nullptr ? *primitive : 0);
    }
    return gCount;
}

}  // extern "C"
