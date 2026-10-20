// Differential library for unit w133: the park loop's small ticks (TickLevelLights, TickFlashObjects, TickRideHighlights, SetBaseBlendFlags,
// LLCC_DrawInitialHoarding, CIntrface::DoPointedThingComputations, FLY_SetFlyZone, CActionRecorder::DoTick and ResetActionRecording).
// Every callee outside those is a recording fake (tests/calllog.h). The meshes the flash and highlight ticks write into are built here
// from a flat description (their Ptr32 fields come from the arena); the sun is a real CLightParallel from src/Light.cpp; the world is only an
// address (the code adds 0x2d8 to it and reads the turn word at 0x1da70c, which is a static block).
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "ActionRecorder.h"
#include "Debug.h"
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "LevelInit.h"
#include "Light.h"
#include "MapCell.h"
#include "MapWho.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "World.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "intrface.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "tb_resource_system.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read: Format (the only member src/TbString.cpp leaves to the test) does nothing
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files that the unit owns here
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CSystem::CSystem() {}
CSystem gei_System;
void CSystem::UpdateLights() { ev("UpdateLights", {lab(this)}); }
float GlobalDiffuse[4];
CLight* gev_LightChain;
CMapLinkBase::~CMapLinkBase() {}
sGlobalLight* gGlobalLight;
s3DPoint gGlobalLightDirection;
uint32_t gGlobalLightColour;
uint32_t gGlobalAmbientColour;
tag_sRideAnimSet* gHighlightedRide;
uint32_t gTPDialogOptions;
uint8_t gLLCC4818;
CIntrface gIntrface;
CActionRecorder gActionRecorder;
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;
alignas(16) unsigned char gHoardingsBalance434C[256];   // (the real object is a CHoardingsBalance; only its address matters here)

namespace {
CLightParallel gLightObject;
sGlobalLight gRecord = {2, &gLightObject, nullptr, nullptr, 0};
alignas(16) unsigned char gTurnBlock[16];   // the world's turn word, 0x1da70c bytes into the world
CThing gThings[8];
}  // namespace

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t argument = 0;
    bool hasArgument = std::strchr(format, '%') != nullptr;
    if (hasArgument) {
        va_list list;
        va_start(list, format);
        argument = va_arg(list, int);
        va_end(list);
        ev("CTRACE", {static_cast<int64_t>(level), argument}, {format});
    } else {
        ev("CTRACE", {static_cast<int64_t>(level)}, {format});
    }
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void LLCM_SetIllegalPlacement(int32_t placement) { ev("LLCM_SetIllegalPlacement", {placement}); }
uint64_t SamsUtilities::UTimer::GetAbsolute() {
    ev("GetAbsolute");
    return 0;
}

static int64_t CellIndex(const CMapCell* cell) {
    return (reinterpret_cast<const char*>(cell) - reinterpret_cast<const char*>(gGameWorld->GetMap())) / 0x44;
}
void CMapCell::UnSetFlag(uint16_t flag) { ev("UnSetFlag", {CellIndex(this), flag}); }
void CMapCell::SetHFieldTile() { ev("SetHFieldTile", {CellIndex(this)}); }
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetParkEntrancePos(int32_t which) const {
    ev("GetParkEntrancePos", {reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gGameWorld), which});
    return {static_cast<uint16_t>(ret("Entrance"))};
}
void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag ? 1 : 0}); }
void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag ? 1 : 0}); }
void LLCC_DrawHoardingsFromBalanceFile(CHoardingsBalance* balance) { ev("LLCC_DrawHoardingsFromBalanceFile", {lab(balance)}); }
void LLCC_DrawDefaultHoardings() { ev("LLCC_DrawDefaultHoardings"); }
void LLCC_LockHoarding(bool lock) { ev("LLCC_LockHoarding", {lock ? 1 : 0}); }
void LLCC_FillInsideOfPark() { ev("LLCC_FillInsideOfPark"); }
int32_t LLCC_BuildHoardingDB(int32_t right, int32_t bottom, int32_t left, int32_t top, char mode, bool flag) {
    ev("LLCC_BuildHoardingDB", {right, bottom, left, top, static_cast<uint8_t>(mode), flag ? 1 : 0});
    return 0;
}
void RecalculateParkDarkening() { ev("RecalculateParkDarkening"); }
void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    ev("LLCR_OverRide", {static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
    *out = static_cast<uint16_t>(ret("OverRide"));
}
void LLCR_GetHoardingExtents(int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    ev("LLCR_GetHoardingExtents", {lab(a), lab(b), lab(c), lab(d)});
    *a = static_cast<int32_t>(ret("Extents"));
    *b = static_cast<int32_t>(ret("Extents"));
    *c = static_cast<int32_t>(ret("Extents"));
    *d = static_cast<int32_t>(ret("Extents"));
}
const uint16_t gLLCRNoCell = 0;

void CActionRecorder::EndReplay() { ev("EndReplay", {lab(this)}); }
void CActionRecorder::GetNextTimingFileEntry() {
    ev("GetNextTimingFileEntry", {lab(this)});
    mTurnsToNext = static_cast<int32_t>(ret("Timing"));
}
void CActionRecorder::GetNextCameraFileEntry() { ev("GetNextCameraFileEntry", {lab(this)}); }
int32_t CActionRecorder::ReadAndProcessNextAction() {
    ev("ReadAndProcessNextAction", {lab(this)});
    return static_cast<int32_t>(ret("ReadAction"));
}
void CActionRecorder::RecordAction(int32_t action, uint8_t* data) {
    int64_t any = 0;
    for (int i = 0; i < 128; ++i) any |= data[i];
    ev("RecordAction", {lab(this), action, any});
}

namespace bfl::FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::CFile(const CFile&) : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() { ev("CFile::Close", {lab(this)}); }
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return -1; }
TbError CMemFile::Open(uint32_t size) {
    ev("CMemFile::Open", {lab(this), size});
    return TbError(0);
}
}  // namespace bfl::FileSystem

// ---- the meshes
namespace {
struct NodeRecord {
    uint32_t* points;
    int nwords;
    std::vector<sTexture*> textures;
};
std::vector<NodeRecord> gNodes;
std::vector<void*> gArenaBlocks, gHeapBlocks;
tag_sNode* gFirstNode;

void Release() {
    for (void* p : gArenaBlocks) tpw::ArenaFree(p);
    for (void* p : gHeapBlocks) std::free(p);
    gArenaBlocks.clear();
    gHeapBlocks.clear();
    gNodes.clear();
    gFirstNode = nullptr;
}
void* Arena(std::size_t bytes) {
    void* p = tpw::ArenaAllocZeroed(bytes);
    gArenaBlocks.push_back(p);
    return p;
}
}  // namespace

extern "C" {

// Named globals the test reads and writes directly.
void* tpw_pt_addr(const char* name) {
    struct Entry { const char* name; void* address; };
    static const Entry table[] = {
        {"direction", &gGlobalLightDirection}, {"colour", &gGlobalLightColour}, {"ambient", &gGlobalAmbientColour},
        {"scale", &gGlobalLightScale}, {"flashPhase", &gFlashPhase}, {"frameTime", &gTPGlobals.mF4014},
        {"dialogOptions", &gTPDialogOptions}, {"llcc4818", &gLLCC4818}, {"left", &gHoardingLeft}, {"top", &gHoardingTop},
        {"right", &gHoardingRight}, {"bottom", &gHoardingBottom}, {"llccE0CC", &gLLCCE0CC}, {"flyB", &gFlyZoneB},
        {"flyC", &gFlyZoneC}, {"flyD", &gFlyZoneD}, {"flyA", &gFlyZoneA}, {"interfaceF24", &gIntrface.mF24},
        {"system", &gei_System}, {"diffuse", GlobalDiffuse}, {"balance", gHoardingsBalance434C}, {"recorder", &gActionRecorder},
        {"recState", &gActionRecorder.mState}, {"recMode", &gActionRecorder.mMode}, {"recReplaying", &gActionRecorder.mRecording},
        {"recType", &gActionRecorder.mReplayType}, {"recTurns", &gActionRecorder.mTurnsToNext}, {"actionFile", gActionRecorder.mActionFile},
        {"timingFile", gActionRecorder.mTimingFile}, {"cameraFile", gActionRecorder.mCameraFile}, {"turn", gTurnBlock + 12}, {"recF08", &gActionRecorder.mF08}, {"recF2C", &gActionRecorder.mF2C},
    };
    for (const Entry& entry : table)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// The world is only an address: its turn word (0x1da70c in) is the static block's word 3.
void tpw_pt_prepare() {
    gGameWorld = reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTurnBlock) + 12 - 0x1da70c);
    std::memset(gTurnBlock, 0, sizeof gTurnBlock);
    std::memset(&gActionRecorder, 0, sizeof gActionRecorder);
}
void tpw_pt_set_turn(uint32_t turn) { std::memcpy(gTurnBlock + 12, &turn, 4); }

// The sun: the fields TickLevelLights writes are filled with 0xa5; out is the light's normal (3), colour word, colour (4).
void tpw_pt_light_prepare() {
    gGlobalLight = &gRecord;
    CLight& light = *gRecord.mDirectional;
    std::memset(&light.mNormal, 0xa5, sizeof light.mNormal);
    std::memset(light.mColor, 0xa5, sizeof light.mColor);
    light.mColorWord = 0xa5a5a5a5;
}
void tpw_pt_light_dump(uint32_t* out) {
    CLight& light = *gRecord.mDirectional;
    std::memcpy(out, &light.mNormal, 12);
    out[3] = light.mColorWord;
    std::memcpy(out + 4, light.mColor, 16);
}

// spec: nsets, then per set: nnodes, then per node: flags, nwords, nfacesets, ntextures, the textures' halfwords, the face sets' texture
// indices. Builds the list gFlashObjects and (when `highlight`) gHighlightedRide.
void tpw_pt_build(const uint32_t* spec, int32_t highlight) {
    Release();
    int k = 0;
    const uint32_t nsets = spec[k++];
    tag_sRideAnimSet* previous = nullptr;
    tag_sRideAnimSet* first = nullptr;
    std::vector<tag_sRideAnimSet*> sets;
    for (uint32_t s = 0; s < nsets; ++s) {
        const uint32_t nnodes = spec[k++];
        tag_sNode* nodes = static_cast<tag_sNode*>(Arena(sizeof(tag_sNode) * (nnodes ? nnodes : 1)));
        for (uint32_t n = 0; n < nnodes; ++n) {
            tag_sNode& node = nodes[n];
            node.mFlags = spec[k++];
            const uint32_t nwords = spec[k++];
            const uint32_t nfacesets = spec[k++];
            const uint32_t ntextures = spec[k++];
            uint32_t* points = static_cast<uint32_t*>(Arena(4 * (nwords + 2)));
            for (uint32_t i = 0; i < nwords + 2; ++i) points[i] = 0xa5a5a5a5;
            sTexture* textures = static_cast<sTexture*>(Arena(sizeof(sTexture) * (ntextures ? ntextures : 1)));
            NodeRecord record{points, static_cast<int>(nwords), {}};
            for (uint32_t t = 0; t < ntextures; ++t) {
                textures[t].mF00 = static_cast<uint16_t>(spec[k++]);
                textures[t].mF02 = 0x5a5a;
                record.textures.push_back(&textures[t]);
            }
            sFaceSet* faceSets = static_cast<sFaceSet*>(Arena(sizeof(sFaceSet) * (nfacesets ? nfacesets : 1)));
            for (uint32_t f = 0; f < nfacesets; ++f) faceSets[f].mTexture.set(&textures[spec[k++]]);
            node.mF5E = static_cast<uint16_t>(nwords);
            node.mFaceSetCount = static_cast<uint16_t>(nfacesets);
            node.mPoints.set(reinterpret_cast<s3DPoint*>(points));
            node.mFaceSets.set(faceSets);
            gNodes.push_back(record);
        }
        tag_sM3D2Header* header = static_cast<tag_sM3D2Header*>(Arena(sizeof(tag_sM3D2Header)));
        header->mNodeCount = static_cast<uint16_t>(nnodes);
        header->mNodes.set(nodes);
        tag_sMesh* mesh = static_cast<tag_sMesh*>(std::calloc(1, sizeof(tag_sMesh)));
        gHeapBlocks.push_back(mesh);
        mesh->mHeader = header;
        tag_sRideAnimSet* set = static_cast<tag_sRideAnimSet*>(std::calloc(1, sizeof(tag_sRideAnimSet)));
        gHeapBlocks.push_back(set);
        set->mHeads = mesh;
        if (previous != nullptr) previous->mFD0 = set;
        else {
            first = set;
            gFirstNode = nodes;
        }
        previous = set;
    }
    gFlashObjects = first;
    gHighlightedRide = highlight != 0 ? first : nullptr;
}
// Everything the three mesh functions could have written: per node its points (nwords + 2 words, the last two guards), then per texture
// its two halfwords. Answers the number of words.
int32_t tpw_pt_dump(uint32_t* out) {
    int32_t n = 0;
    for (const NodeRecord& record : gNodes) {
        for (int i = 0; i < record.nwords + 2; ++i) out[n++] = record.points[i];
        for (const sTexture* texture : record.textures) {
            out[n++] = texture->mF00;
            out[n++] = texture->mF02;
        }
    }
    return n;
}

void tpw_pt_set_thing(int32_t index, int32_t kind) { gThings[index].mKind = static_cast<uint8_t>(kind); }
void tpw_pt_set_row(int32_t row, int32_t thingPlusOne) { gThingArray[row].mThing = thingPlusOne != 0 ? &gThings[thingPlusOne - 1] : nullptr; }
void tpw_pt_set_interface(int32_t pointed, int32_t staff, int32_t under) {
    gIntrface.mPointedThing = pointed != 0 ? &gThings[pointed - 1] : nullptr;
    gIntrface.mPointedStaffOrGuest = staff != 0 ? &gThings[staff - 1] : nullptr;
    gIntrface.mThingUnderPointer = under != 0 ? &gThings[under - 1] : nullptr;
}
static int32_t IdOf(const CThing* thing) {
    if (thing == nullptr) return 0;
    for (int i = 0; i < 8; ++i)
        if (thing == &gThings[i]) return i + 1;
    return 99;
}
void tpw_pt_get_interface(int32_t* out) {
    out[0] = IdOf(gIntrface.mPointedThing);
    out[1] = IdOf(gIntrface.mPointedStaffOrGuest);
    out[2] = IdOf(gIntrface.mThingUnderPointer);
}

// op 0: TickLevelLights(arg); 1: TickFlashObjects; 2: TickRideHighlights; 3: SetBaseBlendFlags(NULL, first node, arg); 4: LLCC_DrawInitialHoarding;
// 5: DoPointedThingComputations; 6: FLY_SetFlyZone; 7: DoTick; 8: ResetActionRecording.
void tpw_pt_call(int32_t op, int32_t arg) {
    switch (op) {
    case 0: TickLevelLights(arg); break;
    case 1: TickFlashObjects(); break;
    case 2: TickRideHighlights(); break;
    case 3: SetBaseBlendFlags(nullptr, gFirstNode, arg); break;
    case 4: LLCC_DrawInitialHoarding(); break;
    case 5: gIntrface.DoPointedThingComputations(); break;
    case 6: FLY_SetFlyZone(); break;
    case 7: gActionRecorder.DoTick(); break;
    case 8: gActionRecorder.ResetActionRecording(); break;
    }
}

}  // extern "C"
