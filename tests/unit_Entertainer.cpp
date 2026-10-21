// Test library for the entertainer and the world's unregister (unit w178; libtpw_unit_Entertainer.dylib, tests/test_Entertainer.py), main app:
//   src/game/people/Entertainer.cpp      CEntertainer (both constructors, Init, ReceiveMessage, Serialise, FinalDestroy, UpdateTurn, ChooseNextAction, SetState,
//                            ModelState), CStaff::~CStaff and the globals (the constructor's two statics, __sinit_Entertainer_cpp's two words)
//   src/game/World.cpp CWorld::Unregister for the six kinds
//
// The callees outside them are fakes that record the same events as the PowerPC stubs: the CStaff and CPeep members the entertainer calls, the delayed
// script, CWorld::Register / GetRand / GetParkAnalyser, the map's effectors, the park analyser, the sound and script calls, the string table's
// FormatString, the four serialise instances, CThing::GetThingId, Vehicle::~Vehicle, the trace and the assertion. The globals of other files are
// defined here: gWorld (pointing into a shifted tail block), gWorldMap, gText (raw storage), gSoundBanks, gMainBalance (raw storage) and gThingArray.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "Debug.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "Serialise.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- the main balance and the text object (raw storage under the symbols' names: their constructors are not wanted)
#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
extern unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
extern unsigned char gTextStorage[sizeof(CText)] __asm__("gText");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};
alignas(16) unsigned char gTextStorage[sizeof(CText)] = {};

// ---- globals of other files
CWorld* gWorld;
CMap* gWorldMap;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the entertainer under test (offsets in the logs are from it)
alignas(16) unsigned char gTable[64];      // the string table object (label 4)

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }

}  // namespace

// ---- the fakes: CStaff (the constructors, the untranslated members)
CStaff::CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model) {
    ev("CStaff::CStaff", {lab(this), cell.mIndex, static_cast<int32_t>(model)});
}
CStaff::CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model, PayGrade grade, uint8_t costume) {
    ev("CStaff::CStaff4", {lab(this), cell.mIndex, static_cast<int32_t>(model), static_cast<int32_t>(grade), costume});
}
void CStaff::FinalDestroy() { ev("CStaff::FinalDestroy", {lab(this)}); }
void CStaff::ReceiveMessage(const CMsg& message) { ev("CStaff::ReceiveMessage", {lab(this), lab(&message)}); }
int32_t CStaff::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CStaff::Serialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CStaff::Serialise", 0));
}
void CStaff::UpdateTurn() { ev("CStaff::UpdateTurn", {lab(this)}); }
void CStaff::SetState(StaffState state) { ev("CStaff::SetState", {lab(this), static_cast<int32_t>(state)}); }
void CStaff::ModelState() { ev("CStaff::ModelState", {lab(this)}); }
int32_t CStaff::DoGeneralStaffThing() {
    ev("CStaff::DoGeneralStaffThing", {lab(this)});
    return static_cast<int32_t>(ret("CStaff::DoGeneralStaffThing", 0));
}
int32_t CStaff::TooTiredToWork() const {
    ev("CStaff::TooTiredToWork", {lab(this)});
    return static_cast<int32_t>(ret("CStaff::TooTiredToWork", 0));
}
void CStaff::GetTiredWorking() { ev("CStaff::GetTiredWorking", {lab(this)}); }
void CStaff::GetTiredPatrolling() { ev("CStaff::GetTiredPatrolling", {lab(this)}); }
void CStaff::ProcessWalkingToRestArea() { ev("CStaff::ProcessWalkingToRestArea", {lab(this)}); }
void CStaff::ProcessResting() { ev("CStaff::ProcessResting", {lab(this)}); }

// ---- the CPeep members
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CPeep::LastCellVisited() {
    ev("CPeep::LastCellVisited", {lab(this)});
    return {static_cast<uint16_t>(ret("CPeep::LastCellVisited", 0))};
}
int32_t CPeep::PeepWalks() {
    ev("CPeep::PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::PeepWalks", 0));
}
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("CPeep::PlaySound", {lab(this), category, sound}); }
int32_t CPeep::SetRandomDestination() {
    ev("CPeep::SetRandomDestination", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::SetRandomDestination", 0));
}

// ---- the delayed script (reached at the entertainer + 0xc)
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("CDelayedScript::SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }

// ---- the world and the map
void CWorld::Register(CEntertainer* staff) { ev("CWorld::Register", {lab(staff)}); }
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {});
    return static_cast<uint32_t>(ret("CWorld::GetRand", 0));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x4000));
}
int32_t CParkAnalyser::IsNearbyThingOfType(CThing::ThingModel model, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id, int32_t a) {
    ev("CParkAnalyser::IsNearbyThingOfType", {static_cast<int32_t>(model), id.mIndex, a});
    return static_cast<int32_t>(ret("CParkAnalyser::IsNearbyThingOfType", 0));
}
void CMap::AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::AddRegionalEffector", {static_cast<int32_t>(effector), cell.mIndex});
}
void CMap::RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::RemoveRegionalEffector", {static_cast<int32_t>(effector), cell.mIndex});
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}

// ---- sprites, scripts, sound
int32_t ESP_GetNumberOfCustomAnims(ESP_Sprites set, int32_t index) {
    ev("ESP_GetNumberOfCustomAnims", {static_cast<int32_t>(set), index});
    return static_cast<int32_t>(ret("ESP_GetNumberOfCustomAnims", 0));
}
void SCR_GetScriptPosition(int32_t handle, float* x, float* y, float* z, int32_t* mode) {
    ev("SCR_GetScriptPosition", {handle});
    uint32_t bits[3] = {static_cast<uint32_t>(ret("SCR_x", 0)), static_cast<uint32_t>(ret("SCR_y", 0)), static_cast<uint32_t>(ret("SCR_z", 0))};
    std::memcpy(x, &bits[0], 4);
    std::memcpy(y, &bits[1], 4);
    std::memcpy(z, &bits[2], 4);
    *mode = static_cast<int32_t>(ret("SCR_mode", 0));
}
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return static_cast<int32_t>(ret("SOUND_SendEvent", 0));
}

// ---- the string table (a block of bytes; FormatString writes the case's text, cut to the size) and the Vehicle
namespace {
char16_t gFormatText[0x40];   // the text FormatString writes (the case's)
}  // namespace

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("TbITable::FormatString", {lab(this), size, id});
    if (size == 0) return 0;
    uint32_t count = 0;
    while (gFormatText[count] != 0 && count < size - 1) {
        out[count] = gFormatText[count];
        ++count;
    }
    out[count] = 0;
    return static_cast<int32_t>(count);
}
Vehicle::~Vehicle() { ev("Vehicle::~Vehicle", {Off(this)}); }

// ---- the serialise instances (the four the entertainer uses): the file, the field's offset from the entertainer, noLog and the name
template <> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) {
    ev("LOADVAR<unsigned long>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) {
    ev("SAVEVAR<unsigned long>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(void* f, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* v, const char* n, int32_t l) {
    ev("LOADVARSTRUCT<ThingId>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(void* f, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* v, const char* n, int32_t l) {
    ev("SAVEVARSTRUCT<ThingId>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}

// ---- the runtime
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ent_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"guard", &gEntertainerCostumeGuard},
        {"counter", &gEntertainerCostumeCounter},
        {"sinit_low", &gEntertainerSinitLow},
        {"sinit_high", &gEntertainerSinitHigh},
        {"banks", gSoundBanks},
        {"balance_values", &reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[0]},
        {"table", gTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_ent_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// A block from the global operator new (for the deleting destructor).
uint64_t tpw_ent_new(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(::operator new(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ent_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CEntertainer));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73C));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry) == 0x14 ? 0x14 : sizeof(CThingArrayEntry));
    return n;
}

// The state a case sets before the call: the entertainer under test, the world's tail block (the world pointer is shifted back to it), the table,
// the text FormatString writes, a row of the thing table and the heap counts.
void tpw_ent_setup(uint64_t entertainer, uint64_t tail, const char* text) {
    gBase = reinterpret_cast<const char*>(entertainer);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(0x5000));
    reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[13] = gTable;
    std::memset(gFormatText, 0, sizeof(gFormatText));
    for (int32_t i = 0; i < 0x3f && text[i] != 0; ++i) gFormatText[i] = static_cast<unsigned char>(text[i]);
}
void tpw_ent_setrow(int32_t row, uint64_t block) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(block)); }
void tpw_ent_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = tpw_cl_heap[i];
}

// ---- the functions, one wrapper each with C types only
uint64_t tpw_ent_ctor1(uint64_t block, uint32_t cell) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}));
}
uint64_t tpw_ent_ctor2(uint64_t block, uint32_t cell, int32_t grade, uint32_t costume, int32_t name) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CEntertainer(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}, grade, static_cast<uint8_t>(costume), name));
}
// form 0: the destructor in place; 1: delete; 2: delete of NULL
void tpw_ent_dtor(uint64_t block, int32_t form) {
    CStaff* staff = reinterpret_cast<CStaff*>(static_cast<uintptr_t>(block));
    if (form == 0) {
        staff->~CStaff();
    } else if (form == 1) {
        delete staff;
    } else {
        CStaff* none = nullptr;
        delete none;
    }
}
void tpw_ent_init(uint64_t block, uint32_t costume) { reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->Init(static_cast<uint8_t>(costume)); }
void tpw_ent_receive(uint64_t block, uint64_t message) {
    reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message)));
}
int32_t tpw_ent_serialise(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
void tpw_ent_final(uint64_t block) { reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->FinalDestroy(); }
void tpw_ent_update(uint64_t block) { reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->UpdateTurn(); }
void tpw_ent_choose(uint64_t block) { reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->ChooseNextAction(); }
void tpw_ent_setstate(uint64_t block, int32_t state) {
    reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->SetState(static_cast<CStaff::StaffState>(state));
}
void tpw_ent_model(uint64_t block) { reinterpret_cast<CEntertainer*>(static_cast<uintptr_t>(block))->ModelState(); }
// __sinit_Entertainer_cpp: the host has no function; the two words are constant initialised, so the wrapper only reads them back.
void tpw_ent_sinit(uint32_t* out) {
    out[0] = gEntertainerSinitLow;
    out[1] = gEntertainerSinitHigh;
}
// CWorld::Unregister for kind 0 handyman, 1 mechanic, 2 entertainer, 3 guard, 4 researcher, 5 object.
void tpw_ent_unregister(int32_t kind, uint64_t thing) {
    void* const p = reinterpret_cast<void*>(static_cast<uintptr_t>(thing));
    switch (kind) {
        case 0: gWorld->Unregister(static_cast<CHandyman*>(p)); break;
        case 1: gWorld->Unregister(static_cast<CMechanic*>(p)); break;
        case 2: gWorld->Unregister(static_cast<CEntertainer*>(p)); break;
        case 3: gWorld->Unregister(static_cast<CGuard*>(p)); break;
        case 4: gWorld->Unregister(static_cast<CResearcher*>(p)); break;
        default: gWorld->Unregister(static_cast<CObject*>(p)); break;
    }
}

}  // extern "C"
