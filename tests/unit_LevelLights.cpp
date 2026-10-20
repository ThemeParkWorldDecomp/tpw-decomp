// Test entry points for the park's lights and its second initialisation and teardown (library unit_LevelLights: LevelLights.cpp,
// LevelInit2.cpp, Light.cpp for the light's constructor, the arena for the height field header). Every callee outside those files
// is a fake that records what it is given: the engine's system object (UpdateLights), the map (Link), the record allocator, the
// error handler, the callees of LevelInit2 and LevelDeInit (translated or not: they tear down real structures), and the main
// balance, which is raw storage here (the accessors read four of its words).
#include <cstdio>
#include <cstring>
#include <new>

namespace {
void observe_allocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"

#include "Advisor.h"
#include "HLSprite.h"
#include "LevelInit.h"
#include "LevelInit2.h"
#include "Light.h"
#include "MainBalance.h"
#include "MapWho.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "World.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_hfield.h"
#include "tb_resource_system.h"

using namespace calllog;

namespace {

// Blocks the original's operator new makes get the same 0xa5 fill as the PowerPC side's, so a field nothing writes compares.
void observe_allocation(std::size_t size, bool, void* block) {
    if (on()) std::memset(block, 0xa5, size);
}

// The record block MReleaseAllocF answers (a sGlobalLight is larger on the host than the Mac's 0x14 bytes).
alignas(16) unsigned char gRecordBlock[64];
// What CMapWho::Link was last given.
CMapLinkBase* gLastLink = nullptr;
// The map is only an address here.
alignas(16) unsigned char gMapStorage[16];
// A world is only an address too (the accessors ignore it).
alignas(16) unsigned char gWorldStorage[16];

}  // namespace

// ---- The engine's system object, the map, the allocator, the error handler -------------------------------------------------
CSystem::CSystem() {}
CSystem gei_System;
void CSystem::UpdateLights() { ev("UpdateLights", {lab(this)}); }
float GlobalDiffuse[4];
CLight* gev_LightChain;
CMapLinkBase::~CMapLinkBase() {}
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("Link", {lab(this), link != nullptr, list});
    gLastLink = link;
    return 1;
}
// The record is 0x14 bytes on the Mac and a sGlobalLight on the host: the size is logged as the Mac's.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size == sizeof(sGlobalLight) ? 0x14 : size, flags});
    std::memset(gRecordBlock, 0xa5, sizeof gRecordBlock);
    return gRecordBlock;
}
bool DataErrorHandler(char* text) {
    ev("DataErrorHandler", {}, {text});
    return false;
}

// ---- The main balance (raw storage under the symbol's name; the accessors read four of its values) -------------------------
#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};
// The first of the four words: the Mac's +0x544 is value slot (0x544 - 8) / 4 (the PPC side writes the bytes there).
constexpr int kBalanceSlot = (0x544 - 8) / 4;

// ---- Globals of other files ------------------------------------------------------------------------------------------------
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
uint32_t gGameOptions;                      // src/TPMain.cpp's
tag_sM3D2Header* gHeightfieldHeader;        // the height field code's
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];   // src/RideTypes.cpp's
uint32_t gRideCountA;
uint32_t gRideCountB;

// ---- LevelInit2's callees ----------------------------------------------------------------------------------------------------
bool LoadLevelAttributeMap() {
    ev("LoadLevelAttributeMap");
    return true;
}
void DC_SetupDynamicCellMeshes() { ev("DC_SetupDynamicCellMeshes"); }
void SetupForPolySearch() { ev("SetupForPolySearch"); }
void DestroyHighlightColourBuffers() { ev("DestroyHighlightColourBuffers"); }
void CreateHighlightColourBuffers() { ev("CreateHighlightColourBuffers"); }
void SetupHeightmapSystem() { ev("SetupHeightmapSystem"); }
void InitialiseCoasterTypes() { ev("InitialiseCoasterTypes"); }
void LLCC_SystemInit(int32_t a, int32_t b) { ev("LLCC_SystemInit", {a, b}); }
void SCR_InitScriptSystem() { ev("SCR_InitScriptSystem"); }
void CreateScratchGeomBuffer(int32_t large) { ev("CreateScratchGeomBuffer", {large}); }
void DestroyScratchGeomBuffer() { ev("DestroyScratchGeomBuffer"); }
void PT_SetRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { ev("PT_SetRange", {x0, y0, x1, y1}); }

// ---- LevelDeInit's callees ---------------------------------------------------------------------------------------------------
void DC_FreeDynamicCellMeshes() { ev("DC_FreeDynamicCellMeshes"); }
void ADV_Close() { ev("ADV_Close"); }
void DestroyAdvisor(uint32_t which) { ev("DestroyAdvisor", {which}); }
void ClosedownRideHoardings() { ev("ClosedownRideHoardings"); }
void DestroyCoasterInstances() { ev("DestroyCoasterInstances"); }
void DestroyCoasterTypes() { ev("DestroyCoasterTypes"); }
void DestroyLevelLights() { ev("DestroyLevelLights"); }
void SetdownFromPolySearch() { ev("SetdownFromPolySearch"); }
void LLCC_SystemDeInit() { ev("LLCC_SystemDeInit"); }
void ESP_RemoveSprites() { ev("ESP_RemoveSprites"); }
// The real one frees an instance and may change the range of numbers: the case's queue gives the change to the free count (which
// never goes below 0).
void DestroyRideInstance(uint32_t instance) {
    ev("DestroyRideInstance", {instance});
    const int64_t changed = static_cast<int64_t>(gRideCountA) + ret("RideCountDelta", 0);
    gRideCountA = changed < 0 ? 0 : static_cast<uint32_t>(changed);
}
void DestroyLevelHeightfield() { ev("DestroyLevelHeightfield"); }
void DestroyAllRideTypes() { ev("DestroyAllRideTypes"); }
void DestroyPathAllocs() { ev("DestroyPathAllocs"); }

// ---- Entry points -------------------------------------------------------------------------------------------------------------
namespace {

// The 21 words of a light from +0x04 to +0x54 (colour floats, position, +0x20, normal, +0x38, the radius trio, flags, colour
// word).
void DumpLight(const CLight* light, uint32_t* out) { std::memcpy(out, light->mColor, 21 * sizeof(uint32_t)); }

// A pointer field of the record: 0 when it still holds the allocator's 0xa5 fill, 1 when it holds an object, 2 otherwise.
int32_t PointerState(const void* pointer) {
    if (pointer == nullptr) return 2;
    uintptr_t fill;
    std::memset(&fill, 0xa5, sizeof fill);
    return reinterpret_cast<uintptr_t>(pointer) == fill ? 0 : 1;
}

}  // namespace

extern "C" {

// Labels for the objects the log names.
void tpw_ll_prepare() {
    tpw_cl_label(1, gMapStorage);
    tpw_cl_label(2, &gei_System);
}

// CreateGlobalLight. `position` and `direction` are three float words each, or NULL. out: [0] record present, [1] type, [2] colour,
// [3] directional state, [4] point state, [5] mF08 state, [6] the link was the light's CMapLinkBase (1), another pointer (0) or none
// (2), [7] objects made by operator new, [8..28] the light's words, [29..32] GlobalDiffuse.
void tpw_ll_create(int32_t type, uint32_t colour, const uint32_t* position, const uint32_t* direction, const uint32_t* diffuse,
                   uint32_t* out) {
    s3DPoint positionValue;
    s3DPoint directionValue;
    if (position != nullptr) std::memcpy(&positionValue, position, sizeof positionValue);
    if (direction != nullptr) std::memcpy(&directionValue, direction, sizeof directionValue);
    std::memcpy(GlobalDiffuse, diffuse, sizeof GlobalDiffuse);
    gLastLink = nullptr;
    const int32_t newBefore = tpw_cl_heap[0];
    sGlobalLight* record = CreateGlobalLight(type, colour, position != nullptr ? &positionValue : nullptr,
                                             direction != nullptr ? &directionValue : nullptr);
    std::memset(out, 0, 40 * sizeof(uint32_t));
    out[7] = static_cast<uint32_t>(tpw_cl_heap[0] - newBefore);
    out[6] = 2;
    if (record != nullptr) {
        out[0] = 1;
        out[1] = static_cast<uint32_t>(record->mType);
        out[2] = record->mColour;
        out[3] = static_cast<uint32_t>(PointerState(record->mDirectional));
        out[4] = static_cast<uint32_t>(PointerState(record->mPoint));
        out[5] = static_cast<uint32_t>(PointerState(record->mF08));
        CLight* light = nullptr;
        if (out[3] == 1) light = record->mDirectional;
        if (out[4] == 1) light = record->mPoint;
        if (light != nullptr) {
            DumpLight(light, out + 8);
            if (gLastLink != nullptr) out[6] = gLastLink == static_cast<CMapLinkBase*>(light) ? 1 : 0;
        }
    }
    std::memcpy(out + 29, GlobalDiffuse, sizeof GlobalDiffuse);
}

// The three setters on a CLight in a block of 0xa5 (flags too); out: the 21 words after the call. op 0: SetColor(a, b, c, d) with
// the argument bit patterns, 1: SetNormal(a, b, c), 2: SetRadius(a).
void tpw_ll_setter(int32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t* out) {
    alignas(CLight) static unsigned char block[sizeof(CLight)];
    std::memset(block, 0xa5, sizeof block);
    CLight* light = new (block) CLight;
    light->mFlags = 0xa5a5a5a5u;
    std::memset(light->mColor, 0xa5, 21 * sizeof(uint32_t));
    float fa;
    float fb;
    float fc;
    float fd;
    std::memcpy(&fa, &a, 4);
    std::memcpy(&fb, &b, 4);
    std::memcpy(&fc, &c, 4);
    std::memcpy(&fd, &d, 4);
    if (op == 0) light->SetColor(fa, fb, fc, fd);
    if (op == 1) light->SetNormal(fa, fb, fc);
    if (op == 2) light->SetRadius(fa);
    DumpLight(light, out);
}

// The CMapLinkBase constructor on a block of 0xa5: out[0] the object is 8 bytes (the vtable pointer only) and the bytes after it
// are untouched, out[1] the constructor changed nothing outside the object.
void tpw_ll_linkbase(uint32_t* out) {
    struct Link : CMapLinkBase {
        int32_t Prepare() override { return 0; }
        int32_t GetLinkType() override { return 0; }
        void* GetData() override { return nullptr; }
    };
    alignas(16) unsigned char block[64];
    std::memset(block, 0xa5, sizeof block);
    new (block) Link;
    bool tail = true;
    for (size_t i = sizeof(Link); i < sizeof block; ++i)
        if (block[i] != 0xa5) tail = false;
    out[0] = sizeof(Link) == sizeof(void*) && sizeof(CMapLinkBase) == sizeof(void*) ? 1 : 0;
    out[1] = tail ? 1 : 0;
}

// LevelInit2's inputs: the four balance words (slots 334..337 as x start, y start, width, height), the options, the two extra words,
// the eight height field globals (as words) and the header: mode 0 none, 1 a header without a field, 2 a header with a field
// whose scale words are scaleA (mF08[2]) and scaleB (mF08[3]).
void tpw_ll_li2_set(const int32_t* balance, uint32_t options, const int32_t* extra, const uint32_t* globals, int32_t mode,
                    uint32_t scaleA, uint32_t scaleB) {
    CMainBalance& main = *reinterpret_cast<CMainBalance*>(gBalanceStorage);
    for (int i = 0; i < 4; ++i) main.mValues[kBalanceSlot + i].mInteger = balance[i];
    gGameOptions = options;
    gHeightfieldRangeExtraX = extra[0];
    gHeightfieldRangeExtraY = extra[1];
    std::memcpy(&gHeightfieldXStart, globals, 4);
    std::memcpy(&gHeightfieldYStart, globals + 1, 4);
    std::memcpy(&gHeightfieldWidth, globals + 2, 4);
    std::memcpy(&gHeightfieldHeight, globals + 3, 4);
    std::memcpy(&gHeightfieldXOrigin, globals + 4, 4);
    std::memcpy(&gHeightfieldYOrigin, globals + 5, 4);
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStorage);
    gHeightfieldHeader = nullptr;
    if (mode >= 1) {
        gHeightfieldHeader = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
        if (mode >= 2) {
            sHField* field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
            std::memcpy(&field->mF08[2], &scaleA, 4);
            std::memcpy(&field->mF08[3], &scaleB, 4);
            gHeightfieldHeader->mHField = field;
        }
    }
}

// Runs LevelInit2; out[0] its answer, out[1..8] the eight height field words (x start, y start, width, height, x origin, y origin,
// extra x, extra y), out[9] the options.
void tpw_ll_li2_run(uint32_t* out) {
    out[0] = static_cast<uint32_t>(LevelInit2());
    std::memcpy(out + 1, &gHeightfieldXStart, 4);
    std::memcpy(out + 2, &gHeightfieldYStart, 4);
    std::memcpy(out + 3, &gHeightfieldWidth, 4);
    std::memcpy(out + 4, &gHeightfieldHeight, 4);
    std::memcpy(out + 5, &gHeightfieldXOrigin, 4);
    std::memcpy(out + 6, &gHeightfieldYOrigin, 4);
    std::memcpy(out + 7, &gHeightfieldRangeExtraX, 4);
    std::memcpy(out + 8, &gHeightfieldRangeExtraY, 4);
    out[9] = gGameOptions;
    if (gHeightfieldHeader != nullptr) {
        if (gHeightfieldHeader->mHField != nullptr) tpw::ArenaFree(gHeightfieldHeader->mHField.get());
        tpw::ArenaFree(gHeightfieldHeader);
        gHeightfieldHeader = nullptr;
    }
}

// LevelDeInit's inputs: the two counts and the sets table (`present` entries from index 0, non-zero for a set).
void tpw_ll_di_set(uint32_t countA, uint32_t countB, int32_t n, const uint8_t* present) {
    gRideCountA = countA;
    gRideCountB = countB;
    for (uint32_t i = 0; i < kMaxRideInstances; ++i) gRideAnimSets[i] = nullptr;
    for (int32_t i = 0; i < n; ++i)
        if (present[i] != 0) gRideAnimSets[i] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(0x1000 + 16 * i));
}

// Runs LevelDeInit; out[0] the free count after, out[1] the instance count after.
void tpw_ll_di_run(int32_t keep, uint32_t* out) {
    LevelDeInit(keep != 0);
    out[0] = gRideCountA;
    out[1] = gRideCountB;
}

// The accessors: out[0..3] x start, y start, width, height (called on the world with the balance words set).
void tpw_ll_accessors(const int32_t* balance, int32_t* out) {
    CMainBalance& main = *reinterpret_cast<CMainBalance*>(gBalanceStorage);
    for (int i = 0; i < 4; ++i) main.mValues[kBalanceSlot + i].mInteger = balance[i];
    const CWorld* world = reinterpret_cast<CWorld*>(gWorldStorage);
    out[0] = world->GetHeightfieldXStart();
    out[1] = world->GetHeightfieldYStart();
    out[2] = world->GetHeightfieldWidth();
    out[3] = world->GetHeightfieldHeight();
}

}  // extern "C"
