// Test library for the handyman (unit w184; libtpw_unit_Handyman.dylib, tests/test_Handyman.py), main app:
//   src/game/people/Handyman.cpp   CHandyman (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, UpdateTurn, ChooseNextAction, SetState, ModelState,
//                      ProcessPatrolling, ProcessWalkingToLitter, ProcessCleaningLitter, ProcessWalkingToCleanToilet, ProcessCleaningToilet,
//                      ProcessIdle, FindNearestDirtyToilet) and its four globals
//
// The callees outside it are fakes that record the same events as the PowerPC stubs: the CStaff, CPeep and CObject members, the map cell calls, the
// thoughts, the delayed script, CWorld::Register / Unregister / GetRand / SeedRand / GetParkAnalyser, the park analyser's GetNearbyLitteredCell, the
// string table's FormatString, the serialise instances, CThing::GetThingId, the message centre (Subscribe, the headers and the nothrow operator new),
// the trace and the assertion. The globals of other files are defined here: gWorld (pointing into a shifted tail block), gWorldMap (a block of cells),
// gText and gMainBalance (raw storage), gSoundBanks, gThingArray and gMessageCentre. __sinit_Handyman_cpp has no host function: the two words are
// constant initialised, so the wrapper reads them back.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "Serialise.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

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
CMessageCentre* gMessageCentre;

namespace {

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the handyman under test (offsets in the logs are from it)
const char* gMapBase;                      // the map (cell offsets in the logs are from it)
void* gNewCentre = nullptr;                // the block the fake operator new made last (its first header logs the event)
alignas(16) unsigned char gTable[64];      // the string table object (label 4)
char16_t gFormatText[0x40];                // the text FormatString writes (the case's)

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }
int32_t MapOff(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gMapBase); }
int64_t Canon(const void* pointer) {
    const int32_t label = lab(pointer);
    if (label >= 0) return label;
    return (1ll << 33) | static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu);
}

class TestMessage : public CMsgRideDeleted {
public:
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return static_cast<int32_t>(ret("GetType", 0));
    }
};

// The number of "%d" in a format: the trace and the assertion log that many integer arguments.
int32_t CountInts(const char* format) {
    int32_t n = 0;
    for (const char* p = format; (p = std::strstr(p, "%d")) != nullptr; p += 2) ++n;
    return n;
}

}  // namespace

// Test-only key function of CMsgRideDeleted (the Mac's is 0x10110f34); TestMessage overrides GetType.
int32_t CMsgRideDeleted::GetType() const { return 10; }
// The destructor the vtable names (the Mac's is 0x10107b28, in src/game/Object.cpp).
CMsgRideDeleted::~CMsgRideDeleted() {}

// ---- the message centre: Subscribe, the 29 headers (the first stands for __construct_array's call) and the nothrow operator new
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), type});
    return true;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = ::operator new(size);
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- the fakes: CStaff
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
int32_t CPeep::PeepWalks() {
    ev("CPeep::PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::PeepWalks", 0));
}
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("CPeep::PlaySound", {lab(this), category, sound}); }
int32_t CPeep::SetRandomDestination() {
    ev("CPeep::SetRandomDestination", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::SetRandomDestination", 0));
}
int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CPeep::SetDest", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("CPeep::SetDest", 0));
}
int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("CPeep::SetDestPos", {lab(this), x.mCell, x.mOffset, y.mCell, y.mOffset});
    return static_cast<int32_t>(ret("CPeep::SetDestPos", 0));
}
void CPeep::Teleport(MapPos x, MapPos y) { ev("CPeep::Teleport", {lab(this), x.mCell, x.mOffset, y.mCell, y.mOffset}); }

// ---- the CObject members (the toilets)
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetAssignedStaffMember() {
    ev("CObject::GetAssignedStaffMember", {lab(this)});
    return {static_cast<uint16_t>(ret("CObject::GetAssignedStaffMember", 0))};
}
void CObject::AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("CObject::AssignStaffMember", {lab(this), id.mIndex}); }
void CObject::UnassignStaffMember() { ev("CObject::UnassignStaffMember", {lab(this)}); }
int32_t CObject::IsDirty() const {
    ev("CObject::IsDirty", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsDirty", 0));
}
int32_t CObject::IsFlaggedForMaintenance() const {
    ev("CObject::IsFlaggedForMaintenance", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsFlaggedForMaintenance", 0));
}
void CObject::CleanFully() { ev("CObject::CleanFully", {lab(this)}); }
void CObject::Reopen() { ev("CObject::Reopen", {lab(this)}); }

// ---- the map cell calls (the cell's offset from the map)
void CMapCell::MarkForLitterCollection(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> collector) {
    ev("CMapCell::MarkForLitterCollection", {MapOff(this), collector.mIndex});
}
void CMapCell::RemoveLitter() { ev("CMapCell::RemoveLitter", {MapOff(this)}); }

// ---- the thoughts and the delayed script (reached at the handyman + 0x30 and + 0xc)
void CThoughts::ProcessThoughtBubbles() { ev("CThoughts::ProcessThoughtBubbles", {Off(this)}); }
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("CDelayedScript::SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }

// ---- the world and the analyser
void CWorld::Register(CHandyman* staff) { ev("CWorld::Register", {lab(staff)}); }
void CWorld::Unregister(CHandyman* staff) { ev("CWorld::Unregister", {lab(staff)}); }
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {});
    return static_cast<uint32_t>(ret("CWorld::GetRand", 0));
}
void CWorld::SeedRand(uint32_t seed) { ev("CWorld::SeedRand", {seed}); }
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x4000));
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CParkAnalyser::GetNearbyLitteredCell(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> who,
                                                                                    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id, int32_t radius) {
    ev("CParkAnalyser::GetNearbyLitteredCell", {who.mIndex, id.mIndex, radius});
    return {static_cast<uint16_t>(ret("CParkAnalyser::GetNearbyLitteredCell", 0))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}

// ---- the string table (FormatString writes the case's text, cut to the size)
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

// ---- the serialise instances (the six the handyman uses): the file, the field's offset from the handyman, noLog and the name
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
template <> int32_t LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>>(void* f, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* v, const char* n, int32_t l) {
    ev("LOADVARSTRUCT<MapId>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>>(void* f, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>* v, const char* n, int32_t l) {
    ev("SAVEVARSTRUCT<MapId>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}

// ---- the runtime: the "%d" arguments are logged after the level (CTRACE) or the condition (GAME_ASSERTMSG)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t v[4] = {condition, 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("GAME_ASSERTMSG", {v[0]}, {format}); break;
    case 1: ev("GAME_ASSERTMSG", {v[0], v[1]}, {format}); break;
    case 2: ev("GAME_ASSERTMSG", {v[0], v[1], v[2]}, {format}); break;
    default: ev("GAME_ASSERTMSG", {v[0], v[1], v[2], v[3]}, {format}); break;
    }
}
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t v[4] = {static_cast<int64_t>(level), 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("CTRACE", {v[0]}, {format}); break;
    case 1: ev("CTRACE", {v[0], v[1]}, {format}); break;
    case 2: ev("CTRACE", {v[0], v[1], v[2]}, {format}); break;
    default: ev("CTRACE", {v[0], v[1], v[2], v[3]}, {format}); break;
    }
}
void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_hm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"banks", gSoundBanks},
        {"balance_values", &reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[0]},
        {"table", gTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_hm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// A ride-deleted message whose GetType answers the queue "GetType" and whose deleted id is `id`.
uint64_t tpw_hm_makemsg(uint32_t id) {
    TestMessage* m = new TestMessage();
    m->mThingId = static_cast<uint16_t>(id);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_hm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CHandyman));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry) == 0x14 ? 0x14 : sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    return n;
}

// The state a case sets before the call: the handyman under test, the world's tail block (the world pointer is shifted back to it), the map block, the
// table, the text FormatString writes and the message centre (0 or a block).
void tpw_hm_setup(uint64_t handyman, uint64_t tail, const char* text, uint64_t centre, uint64_t map) {
    gBase = reinterpret_cast<const char*>(handyman);
    gMapBase = reinterpret_cast<const char*>(map);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
    reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[11] = gTable;
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gNewCentre = nullptr;
    std::memset(gFormatText, 0, sizeof(gFormatText));
    for (int32_t i = 0; i < 0x3f && text[i] != 0; ++i) gFormatText[i] = static_cast<unsigned char>(text[i]);
}
void tpw_hm_setrow(int32_t row, uint64_t block) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(block)); }
int64_t tpw_hm_centre() { return Canon(gMessageCentre); }
// The constructor's function static (the flag byte and the sprite index): set before a case, read back after it.
void tpw_hm_setglobals(uint32_t guard, uint32_t sprite) {
    gHandymanSpriteGuard = static_cast<uint8_t>(guard);
    gHandymanSprite = static_cast<int32_t>(sprite);
}
void tpw_hm_globals(uint32_t* out) {
    out[0] = gHandymanSpriteGuard;
    out[1] = static_cast<uint32_t>(gHandymanSprite);
}
// __sinit_Handyman_cpp: the host has no function; the two words are constant initialised, so the wrapper only reads them back.
void tpw_hm_sinit(uint32_t* out) {
    out[0] = gHandymanSinitLow;
    out[1] = gHandymanSinitHigh;
}

// ---- the functions, one wrapper each with C types only
static CHandyman* H(uint64_t block) { return reinterpret_cast<CHandyman*>(static_cast<uintptr_t>(block)); }
uint64_t tpw_hm_ctor1(uint64_t block, uint32_t cell) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}));
}
uint64_t tpw_hm_ctor2(uint64_t block, uint32_t cell, int32_t grade, uint32_t flag, int32_t name) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CHandyman(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}, grade, static_cast<uint8_t>(flag), name));
}
void tpw_hm_init(uint64_t block, uint32_t sprite) { H(block)->Init(static_cast<uint8_t>(sprite)); }
void tpw_hm_receive(uint64_t block, uint64_t message) { H(block)->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message))); }
int32_t tpw_hm_serialise(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return H(block)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
void tpw_hm_final(uint64_t block) { H(block)->FinalDestroy(); }
void tpw_hm_update(uint64_t block) { H(block)->UpdateTurn(); }
void tpw_hm_choose(uint64_t block) { H(block)->ChooseNextAction(); }
void tpw_hm_model(uint64_t block) { H(block)->ModelState(); }
void tpw_hm_setstate(uint64_t block, int32_t state) { H(block)->SetState(static_cast<CStaff::StaffState>(state)); }
void tpw_hm_idle(uint64_t block) { H(block)->ProcessIdle(); }
void tpw_hm_patrol(uint64_t block) { H(block)->ProcessPatrolling(); }
void tpw_hm_tolitter(uint64_t block) { H(block)->ProcessWalkingToLitter(); }
void tpw_hm_cleanlitter(uint64_t block) { H(block)->ProcessCleaningLitter(); }
void tpw_hm_totoilet(uint64_t block) { H(block)->ProcessWalkingToCleanToilet(); }
void tpw_hm_cleantoilet(uint64_t block) { H(block)->ProcessCleaningToilet(); }
uint32_t tpw_hm_find(uint64_t block) { return H(block)->FindNearestDirtyToilet().mIndex; }

}  // extern "C"
