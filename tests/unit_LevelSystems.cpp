// Test entry points for the park's second-stage systems (library unit_LevelSystems: LevelSystems.cpp, LLCellSystem.cpp,
// AnalyserSystems.cpp, tb_hfield_range.cpp, Light.cpp for the lights DestroyLevelLights deletes). Every callee outside those
// files is a fake that records what it is given: the allocator and its free, the texture and mesh freers, the dynamic mesh's
// constructor and destructor, the file lookup, the map loader, the cell system's callees, the map's Unlink, the error box's
// replacement (UDebug), rand and the height field's range setter. The globals are the unit's own, reached by name through
// tpw_ls_addr; the objects the functions free or delete are built here (coaster types, lights, meshes) with fixed labels.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"

#include "DynamicMesh.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "analyser.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "tb_hfield.h"

using namespace calllog;

namespace {

// Blocks the allocator fake answers (labels 20 to 23 in the order they are handed out) and the script table's block.
alignas(16) unsigned char gBlocks[4][64];
uint32_t gBlockNext = 0;
alignas(16) unsigned char gScriptBlock[416];
// The texts the file lookup fakes answer: the theme path and the path FindFilePath finds (NULL when gFoundNull).
char gThemeText[1100];
char gFoundText[1100];
bool gFoundNull = false;
// The map, the world and the height field are only addresses.
alignas(16) unsigned char gMapStorage[16];
alignas(16) unsigned char gWorldStorage[16];
alignas(16) unsigned char gHFieldStorage[16];
// The ok buffer address the dynamic mesh constructor fake saw last (to log whether a call used the same one).
const uint32_t* gLastOk = nullptr;

}  // namespace

// ---- Globals of other files the functions read ------------------------------------------------------------------------------
uint32_t gPolySearchCount;      // src/RideInstance.cpp's
uint32_t gMaxMeshFaces4;        // TPEngine.cpp's
uint32_t gMaxMeshF3E;
uint32_t gMaxMeshNodes;
uint32_t gObjectTableCount;     // src/CoasterDestroy.cpp's
uint32_t gObjectTableFirstFree;
uint32_t gObjectTableLowest;
sGlobalLight* gGlobalLight;     // src/LevelInit.cpp's
float GlobalDiffuse[4];         // the engine's
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);
CWorld* gGameWorld;
CHField* gei_HField;
CLight* gev_LightChain;
CMapLinkBase::~CMapLinkBase() {}
sDTextureSet gRideHoardingTextures;     // initialised data of the Mac
sDTextureSet gDynamicCellTextures;

// ---- The allocator, the freers and the mesh and texture callees -----------------------------------------------------------------
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    return gBlocks[gBlockNext++ & 3];
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
int32_t DMFreeMesh(tag_sDMeshSet* set) {
    ev("DMFreeMesh", {lab(set)});
    return static_cast<int32_t>(ret("DMFreeMesh", 1));
}
int32_t DMLoadTextures(sDTextureSet* set) {
    ev("DMLoadTextures", {lab(set)});
    return static_cast<int32_t>(ret("DMLoadTextures", 1));
}
void DMFreeTextures(sDTextureSet* set) { ev("DMFreeTextures", {lab(set)}); }
void* LbMem_Alloc(uint32_t size) {
    ev("LbMem_Alloc", {size});
    return gScriptBlock;
}
int32_t msl_rand() {
    const int64_t value = ret("rand", 4242);
    ev("rand", {value});
    return static_cast<int32_t>(value);
}

// ---- Coaster callees --------------------------------------------------------------------------------------------------------------
void CreateExtrudeFaceStorage(uint32_t a, uint32_t b) { ev("CreateExtrudeFaceStorage", {a, b}); }
void SetupObjectInstanceTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest) {
    ev("SetupObjectInstanceTable", {lab(&count), lab(&firstFree), lab(&lowest), count, firstFree, lowest});
}

// ---- The attribute map's callees ----------------------------------------------------------------------------------------------
char* get_theme_path() {
    ev("get_theme_path");
    return gThemeText;
}
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    return gFoundNull ? nullptr : gFoundText;
}
int32_t CMap::LoadAttribMap(const char* path) {
    ev("LoadAttribMap", {lab(this)}, {path});
    return static_cast<int32_t>(ret("LoadAttribMap", 0));
}

// ---- The cell system's callees: each records a hash of the system's words so far (so the order of the stores shows) ----------
namespace {
uint64_t Fold(uint64_t hash, uint64_t value) { return ((hash ^ value) * 0x100000001b3ull) & 0x7fffffffffffffffull; }
int64_t CellHash() {
    uint64_t h = 0xcbf29ce484222325ull & 0x7fffffffffffffffull;
    h = Fold(h, gLLCC4818);
    h = Fold(h, gLLCC47C4);
    h = Fold(h, static_cast<uint32_t>(gLLCCWidth));
    h = Fold(h, static_cast<uint32_t>(gLLCCHeight));
    h = Fold(h, static_cast<uint32_t>(gLLCC47CC));
    h = Fold(h, static_cast<uint32_t>(gLLCC4814));
    h = Fold(h, static_cast<uint32_t>(gLLCC4810));
    h = Fold(h, static_cast<uint32_t>(gLLCC480C));
    h = Fold(h, static_cast<uint32_t>(gLLCC4808));
    h = Fold(h, static_cast<uint32_t>(gLLCC4804));
    h = Fold(h, static_cast<uint32_t>(gLLCC4800));
    h = Fold(h, gLLCCDE0E2);
    h = Fold(h, static_cast<uint32_t>(gLLCCC214));
    h = Fold(h, static_cast<uint32_t>(gLLCCC210));
    h = Fold(h, static_cast<uint32_t>(gLLCCC208));
    h = Fold(h, static_cast<uint32_t>(gLLCCC204));
    h = Fold(h, static_cast<uint32_t>(gLLCCC200));
    h = Fold(h, static_cast<uint32_t>(gLLCC47EC));
    h = Fold(h, static_cast<uint32_t>(gLLCC47E8));
    h = Fold(h, static_cast<uint32_t>(gLLCC47E4));
    h = Fold(h, static_cast<uint32_t>(gLLCC47E0));
    h = Fold(h, static_cast<uint32_t>(gLLCCDB6C));
    h = Fold(h, gLLCCDB68);
    h = Fold(h, static_cast<uint32_t>(gLLCC47C8));
    return static_cast<int64_t>(h);
}
}  // namespace
void LLCC_ResetMap(int32_t width, int32_t height) { ev("LLCC_ResetMap", {width, height, CellHash()}); }
void LLCM_SetIllegalPlacement(int32_t placement) { ev("LLCM_SetIllegalPlacement", {placement, CellHash()}); }
void LLCM_LoadQMeshes() { ev("LLCM_LoadQMeshes", {CellHash()}); }
void LLCM_LoadKTrackMeshes() { ev("LLCM_LoadKTrackMeshes", {CellHash()}); }
void LLCM_LoadWTrackMeshes() { ev("LLCM_LoadWTrackMeshes", {CellHash()}); }
void LLCM_LoadHoardingMeshes() { ev("LLCM_LoadHoardingMeshes", {CellHash()}); }
void LLCC_SystemResetClickStack() { ev("LLCC_SystemResetClickStack", {CellHash()}); }
void LLCC_ResetBuyLandList() { ev("LLCC_ResetBuyLandList", {CellHash()}); }
void LLCC_DrawInitialHoarding() { ev("LLCC_DrawInitialHoarding", {CellHash()}); }
void LLCM_DestroyQMeshes() { ev("LLCM_DestroyQMeshes"); }
void LLCM_DestroyKTrackMeshes() { ev("LLCM_DestroyKTrackMeshes"); }
void LLCM_DestroyWTrackMeshes() { ev("LLCM_DestroyWTrackMeshes"); }
void LLCM_DestroyHoardingMeshes() { ev("LLCM_DestroyHoardingMeshes"); }

// ---- The script system's and the dynamic meshes' callees ----------------------------------------------------------------------
void SCR_InitCheckTable() { ev("SCR_InitCheckTable"); }
// operator new with a null answer: the case's queue "newnull" says which calls fail (the host's ordinary new never does).
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    // The size is recorded as the Mac's: CDynamicMesh is 0xa4 bytes there (operator new(0xa4) in DC_SetupDynamicCellMeshes' listing) and
    // larger on the host since w112b made the class real (its pointers are eight bytes).
    const int64_t mac = size == sizeof(CDynamicMesh) ? 0xa4 : static_cast<int64_t>(size);
    ev("new", {mac, fail});
    return fail ? nullptr : ::operator new(size);
}
CDynamicMesh::CDynamicMesh(uint16_t sizeA, uint16_t sizeB, uint32_t* ok, const char* name, DYN_Type type, float minX, float minY,
                           float minZ, float maxX, float maxY, float maxZ) {
    if (on()) {
        ev("CDynamicMesh", {newlab(this), sizeA, sizeB, ok == gLastOk, type, fbits(minX), fbits(minY), fbits(minZ), fbits(maxX),
                            fbits(maxY), fbits(maxZ)},
           {name});
        gLastOk = ok;
    }
    *ok = on() ? static_cast<uint32_t>(ret("ctorOk", 1)) : 1;
}
CDynamicMesh::~CDynamicMesh() { ev("CDynamicMesh::~CDynamicMesh", {lab(this)}); }

// ---- The map, the lights' list, the height field, the error box's replacement --------------------------------------------------
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    ev("Unlink", {lab(this), lab(link), list});
    return 1;
}
void CHField::ProcTextureRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    ev("ProcTextureRange", {lab(this), x0, y0, x1, y1});
}
void SamsUtilities::UDebug(const char* format, ...) {
    char text[512];
    va_list ap;
    va_start(ap, format);
    std::vsnprintf(text, sizeof text, format, ap);
    va_end(ap);
    ev("UDebug", {}, {text});
}

// ---- Entry points ---------------------------------------------------------------------------------------------------------------
namespace {

struct Entry {
    const char* name;
    void* address;
};
#define G(x) {#x, &x}
const Entry kState[] = {
    G(gPolySearchCount), G(gPolySearchTable), G(gHeightmapScaleA), G(gHeightmapScaleB), G(gHighlightFaceColours),
    G(gHighlightNodeColours), G(gMaxMeshFaces4), G(gMaxMeshF3E), G(gMaxMeshNodes), G(gObjectTableCount),
    G(gObjectTableFirstFree), G(gObjectTableLowest), G(gExtrudeFaceCountA), G(gExtrudeFaceCountB), G(gCoasterTypeCount),
    G(gCoasterTextureBase), G(gGlobalLight), G(gGameWorld), G(gei_HField), G(gScriptInitialised), G(gScriptF7C2C),
    G(gScriptTableSize), G(gScriptTable), G(gDynamicCellF9BD4), G(gDynamicCellsReady), G(gDynamicCellF9D94),
    G(gDynamicCellF9D98), G(gDynamicCellF9D9C), G(gDynamicCellF9DA0), G(gDynamicCellFBDBC), G(gDynamicCellFlag),
    G(gBlueprintMesh), G(gWaterMesh), G(gFlagsMesh), G(gLLCC4818), G(gLLCC47C4), G(gLLCCWidth), G(gLLCCHeight),
    G(gLLCC47CC), G(gLLCC4814), G(gLLCC4810), G(gLLCC480C), G(gLLCC4808), G(gLLCC4804), G(gLLCC4800), G(gLLCCDE0E2),
    G(gLLCCC214), G(gLLCCC210), G(gLLCCC208), G(gLLCCC204), G(gLLCCC200), G(gLLCC47EC), G(gLLCC47E8), G(gLLCC47E4),
    G(gLLCC47E0), G(gLLCCDB6C), G(gLLCCDB68), G(gLLCC47C8), G(gDynamicCellRandom), G(gCoasterTypes), G(GlobalDiffuse),
    G(gRideHoardingTextures), G(gDynamicCellTextures), G(gMapStorage), G(gWorldStorage),
    G(gHFieldStorage), G(gBlocks), G(gScriptBlock),
};
#undef G

// The coaster types the builder made, in creation order, for the dump.
struct BuiltType {
    tag_sCoasterType* type;
    sCarType* sets[3];
    void* setBlocks[3];
    sDTextureSet* textures;
    sDTextureEntry* entries;
    uint32_t entryCount;
    sCoasterRecord* records;
    uint32_t recordCount;
};
BuiltType gBuilt[3];
uint32_t gBuiltCount = 0;
CLightParallel* gDirectional = nullptr;
CLightPoint* gPoint = nullptr;
sGlobalLight gRecord;

// A pointer's label, or the label 0 for NULL (the dump of a freed or cleared field).
int64_t Lab(const void* p) { return lab(p); }

}  // namespace

extern "C" {

// The address of a named global (the test reads and writes it directly); NULL for an unknown name.
void* tpw_ls_addr(const char* name) {
    for (const Entry& entry : kState)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// Per-case reset after the call log has begun: the allocator's blocks, the ok-buffer memory, the texts.
void tpw_ls_begin(const char* themeText, const char* foundText) {
    gBlockNext = 0;
    gLastOk = nullptr;
    std::memset(gBlocks, 0xa5, sizeof gBlocks);
    std::memset(gScriptBlock, 0xa5, sizeof gScriptBlock);
    std::strncpy(gThemeText, themeText, sizeof gThemeText - 1);
    gFoundNull = foundText == nullptr;
    if (foundText != nullptr) std::strncpy(gFoundText, foundText, sizeof gFoundText - 1);
    gBuiltCount = 0;
    gDirectional = nullptr;
    gPoint = nullptr;
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStorage);
    gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);
}

// The functions without arguments or objects (op numbers in the test module); answers the function's own answer where it has one.
int64_t tpw_ls_call(int32_t op, int32_t a, int32_t b, int32_t c, int32_t d, const char* text) {
    switch (op) {
        case 0: return LoadLevelAttributeMap();
        case 1: SetupForPolySearch(); return 0;
        case 2: SetdownFromPolySearch(); return 0;
        case 3: SetupHeightmapSystem(); return 0;
        case 4: ClosedownRideHoardings(); return 0;
        case 5: DestroyLevelLights(); return 0;
        case 6: CreateHighlightColourBuffers(); return 0;
        case 7: DestroyHighlightColourBuffers(); return 0;
        case 8: InitialiseCoasterTypes(); return 0;
        case 9: DestroyCoasterTypes(); return 0;
        case 10: LLCC_SystemInit(a, b); return 0;
        case 11: LLCC_SystemDeInit(); return 0;
        case 12: return DataErrorHandler(const_cast<char*>(text));
        case 13: SCR_InitScriptSystem(); return 0;
        case 14: DC_SetupDynamicCellMeshes(); return 0;
        case 15: DC_FreeDynamicCellMeshes(); return 0;
        case 16: PT_SetRange(a, b, c, d); return 0;
    }
    return -1;
}

// The meshes DC_FreeDynamicCellMeshes deletes: bit n of `mask` makes mesh n (blueprint, water, flags) with the label `labels[n]`;
// the others are NULL. Made with the log off (the constructor fake would record).
void tpw_ls_make_meshes(int32_t mask, const int32_t* labels) {
    const int32_t was = tpw_cl_on;
    tpw_cl_on = 0;
    CDynamicMesh** slots[3] = {&gBlueprintMesh, &gWaterMesh, &gFlagsMesh};
    uint32_t ok[4] = {};
    for (int32_t n = 0; n < 3; ++n) {
        *slots[n] = nullptr;
        if ((mask >> n) & 1) {
            *slots[n] = new CDynamicMesh(0, 0, ok, "", kDynBlueprint, 0, 0, 0, 0, 0, 0);
            tpw_cl_label(labels[n], *slots[n]);
        }
    }
    tpw_cl_on = was;
}

// The park's light record (type, which lights exist), stored in gGlobalLight: labels 31 and 32 for the directional and the point
// light's objects. The link base of a host light is at the object's own address (the test checks it through the Unlink events,
// which name the link by its label), so it has no label of its own. GlobalDiffuse is the caller's.
void tpw_ls_make_lights(int32_t type, int32_t directional, int32_t point) {
    gRecord.mType = type;
    gRecord.mDirectional = nullptr;
    gRecord.mF08 = nullptr;
    gRecord.mPoint = nullptr;
    gRecord.mColour = 0;
    if (directional != 0) {
        gDirectional = new CLightParallel;
        gRecord.mDirectional = gDirectional;
        tpw_cl_label(31, gDirectional);
    }
    if (point != 0) {
        gPoint = new CLightPoint;
        gRecord.mPoint = gPoint;
        tpw_cl_label(32, gPoint);
    }
    gGlobalLight = &gRecord;
}

// What is left of the lights: out[0] the record's directional pointer, out[1] its point pointer (labels, 0 for NULL).
void tpw_ls_dump_lights(int64_t* out) {
    out[0] = Lab(gRecord.mDirectional);
    out[1] = Lab(gRecord.mPoint);
}

// Frees what the light builder made and the function did not delete.
void tpw_ls_free_lights(const int32_t* deleted) {
    const int32_t was = tpw_cl_on;
    tpw_cl_on = 0;
    if (deleted[0] == 0) delete gDirectional;
    if (deleted[1] == 0) delete gPoint;
    gDirectional = nullptr;
    gPoint = nullptr;
    tpw_cl_on = was;
}

// The coaster types. spec: 7 words per type: [track set of +0x2fc, of +0x300, of +0x304 (each 0 to 2), the texture set's count,
// the record index, the record count, a mask of blocks that are NULL]. The bits of the mask: 0..2 the three track sets' blocks,
// 3 +0x308, 4 +0x14, 5 +0x78, 6 +0x90, 7 +0x94, 8 +0x98, 9 the indexed record's block. `labels` are handed out in creation order:
// per type the record, the three track sets, their blocks (not NULL), +0x308, the texture set, the entry array, one block per
// entry, the mesh set, +0x14, the record array, one block per record, +0x90, +0x94, +0x98, +0x78.
void tpw_ls_make_coasters(int32_t n, const int32_t* spec, const int32_t* labels) {
    const int32_t was = tpw_cl_on;
    tpw_cl_on = 0;
    int32_t next = 0;
    auto make = [&](void* p) {
        if (p != nullptr) tpw_cl_label(labels[next], p);
        ++next;
        return p;
    };
    auto block = [&](int32_t mask, int32_t bit) { return ((mask >> bit) & 1) != 0 ? nullptr : new unsigned char[16]; };
    gBuiltCount = static_cast<uint32_t>(n);
    gCoasterTypeCount = static_cast<uint32_t>(n);
    for (int32_t t = 0; t < n; ++t) {
        const int32_t* s = spec + 7 * t;
        BuiltType& b = gBuilt[t];
        b.type = static_cast<tag_sCoasterType*>(make(new tag_sCoasterType));
        std::memset(b.type, 0xa5, sizeof *b.type);
        for (int32_t k = 0; k < 3; ++k) b.sets[k] = static_cast<sCarType*>(make(new sCarType));
        for (int32_t k = 0; k < 3; ++k) {
            b.setBlocks[k] = block(s[6], k);
            b.sets[k]->mBlock = make(b.setBlocks[k]);
        }
        b.type->mCarTypeA = b.sets[s[0]];
        b.type->mCarTypeB = b.sets[s[1]];
        b.type->mCarTypeC = b.sets[s[2]];
        b.type->mCarTypes = static_cast<sCarType*>(make(block(s[6], 3)));
        b.textures = static_cast<sDTextureSet*>(make(new sDTextureSet));
        b.entryCount = static_cast<uint32_t>(s[3]);
        b.textures->mCount = static_cast<uint16_t>(s[3]);
        b.entries = new sDTextureEntry[b.entryCount + 1];
        make(b.entries);
        b.textures->mEntries = b.entries;
        for (uint32_t e = 0; e < b.entryCount; ++e) b.entries[e].mBlock = make(new unsigned char[16]);
        b.type->mTextures = b.textures;
        b.type->mMeshSet = static_cast<tag_sDMeshSet*>(make(new unsigned char[16]));
        b.type->mF14 = make(block(s[6], 4));
        b.recordCount = static_cast<uint32_t>(s[5]);
        b.records = new sCoasterRecord[b.recordCount];
        make(b.records);
        for (uint32_t r = 0; r < b.recordCount; ++r) {
            const bool target = r == static_cast<uint32_t>(s[4]);
            b.records[r].mBlock = make(target ? block(s[6], 9) : new unsigned char[16]);
        }
        b.type->mRecords = b.records;
        b.type->mRecordIndex = static_cast<uint32_t>(s[4]);
        b.type->mF90 = make(block(s[6], 6));
        b.type->mF94 = make(block(s[6], 7));
        b.type->mF98 = make(block(s[6], 8));
        b.type->mF78 = make(block(s[6], 5));
        gCoasterTypes[t] = b.type;
    }
    tpw_cl_on = was;
}

// What the types hold after DestroyCoasterTypes, as labels, per type: the 12 fields of the record, the three track sets' blocks,
// the texture set's entry array, the entries' blocks, the records' blocks. Returns the count of words written.
int32_t tpw_ls_dump_coasters(int64_t* out) {
    int32_t n = 0;
    for (uint32_t t = 0; t < gBuiltCount; ++t) {
        const BuiltType& b = gBuilt[t];
        const tag_sCoasterType* y = b.type;
        const void* fields[12] = {y->mF14, y->mMeshSet, y->mTextures, y->mF78, y->mRecords, y->mF90, y->mF94, y->mF98,
                                  y->mCarTypeA, y->mCarTypeB, y->mCarTypeC, y->mCarTypes};
        for (const void* f : fields) out[n++] = Lab(f);
        for (int32_t k = 0; k < 3; ++k) out[n++] = Lab(b.sets[k]->mBlock);
        out[n++] = Lab(b.textures->mEntries);
        for (uint32_t e = 0; e < b.entryCount; ++e) out[n++] = Lab(b.entries[e].mBlock);
        for (uint32_t r = 0; r < b.recordCount; ++r) out[n++] = Lab(b.records[r].mBlock);
    }
    return n;
}

// Frees what the coaster builder made (the blocks are never freed by the fake free).
void tpw_ls_free_coasters() {
    for (uint32_t t = 0; t < gBuiltCount; ++t) {
        BuiltType& b = gBuilt[t];
        delete b.type;
        for (int32_t k = 0; k < 3; ++k) delete b.sets[k];
        delete b.textures;
        delete[] b.entries;
        delete[] b.records;
    }
    gBuiltCount = 0;
}

// The script table's block (416 bytes, filled with 0xa5 before the call) as it is after.
void tpw_ls_dump_script(uint8_t* out) { std::memcpy(out, gScriptBlock, sizeof gScriptBlock); }

}  // extern "C"
