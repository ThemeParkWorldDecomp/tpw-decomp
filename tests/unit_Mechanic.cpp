// Test library for the mechanic (unit w187; libtpw_unit_Mechanic.dylib, tests/test_Mechanic.py), main app:
//   src/game/people/mechanic.cpp   CMechanic (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, SetState, ModelState, UpdateTurn, ChooseNextAction,
//                      ProcessPatrolling, ProcessWalkingToBreakdown, ProcessRepairing, ProcessIdle) and its four globals
//
// The callees outside it are fakes that record the same events as the PowerPC stubs: the CStaff, CPeep and CObject members, the delayed script,
// CWorld::Register / Unregister / GetRand / GetMechanicHQ / GetAdvisor, CMechanicHQ::GetNewJob, the advisor request and its queue, the string table's
// FormatString, the serialise instances, CThing::GetThingId, the message centre (Subscribe, the headers and the nothrow operator new), the trace and the
// assertion. The globals of other files are defined here: gWorld (pointing into a shifted tail block), gText and gMainBalance (raw storage), gSoundBanks,
// gThingArray and gMessageCentre. __sinit_mechanic_cpp has no host function: the two words are constant initialised, so the wrapper reads them back.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Serialise.h"

// the headers reach TimeSig.h, which uses these two: declare the fakes before it.
template <> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l);
template <> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l);

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MainBalance.h"
#include "RideDB.h"
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
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];
CMessageCentre* gMessageCentre;

namespace {

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the mechanic under test (offsets in the logs are from it)
void* gNewCentre = nullptr;                // the block the fake operator new made last (its first header logs the event)
const void* gLastRequest = nullptr;        // the last advisor request constructed
RideInfo* gInfo = nullptr;                 // the ride record GetDBInfo answers
alignas(16) unsigned char gTable[64];      // the string table object (label 4)
char16_t gFormatText[0x40];                // the text FormatString writes (the case's)

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }
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

// ---- the CObject members (the rides)
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetAssignedStaffMember() {
    ev("CObject::GetAssignedStaffMember", {lab(this)});
    return {static_cast<uint16_t>(ret("CObject::GetAssignedStaffMember", 0))};
}
void CObject::AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("CObject::AssignStaffMember", {lab(this), id.mIndex}); }
void CObject::UnassignStaffMember() { ev("CObject::UnassignStaffMember", {lab(this)}); }
int32_t CObject::IsFlaggedForMaintenance() const {
    ev("CObject::IsFlaggedForMaintenance", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsFlaggedForMaintenance", 0));
}
bool CObject::IsBrokenDown() const {
    ev("CObject::IsBrokenDown", {lab(this)});
    return ret("CObject::IsBrokenDown", 0) != 0;
}
int32_t CObject::RequiresUpgrade() const {
    ev("CObject::RequiresUpgrade", {lab(this)});
    return static_cast<int32_t>(ret("CObject::RequiresUpgrade", 0));
}
void CObject::StartFixing() { ev("CObject::StartFixing", {lab(this)}); }
int32_t CObject::IsPlayingFixAnim() {
    ev("CObject::IsPlayingFixAnim", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsPlayingFixAnim", 0));
}
void CObject::RepairFully(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) { ev("CObject::RepairFully", {lab(this), staff.mIndex}); }
RideInfo* CObject::GetDBInfo() const {
    ev("CObject::GetDBInfo", {lab(this)});
    return gInfo;
}

// ---- the delayed script (reached at the mechanic + 0xc)
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("CDelayedScript::SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }

// ---- the world, the mechanic HQ and the advisor
void CWorld::Register(CMechanic* staff) { ev("CWorld::Register", {lab(staff)}); }
void CWorld::Unregister(CMechanic* staff) { ev("CWorld::Unregister", {lab(staff)}); }
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {});
    return static_cast<uint32_t>(ret("CWorld::GetRand", 0));
}
CMechanicHQ* CWorld::GetMechanicHQ() {
    ev("CWorld::GetMechanicHQ", {});
    return reinterpret_cast<CMechanicHQ*>(static_cast<uintptr_t>(0x4000));
}
CAdvisor* CWorld::GetAdvisor() {
    ev("CWorld::GetAdvisor", {});
    return reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(0x5000));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMechanicHQ::GetNewJob(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> mechanic) {
    ev("CMechanicHQ::GetNewJob", {lab(this), mechanic.mIndex});
    return {static_cast<uint16_t>(ret("CMechanicHQ::GetNewJob", 0))};
}
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    gLastRequest = this;
    ev("CAdvisorRequest", {static_cast<int32_t>(message), thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
}
int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {lab(this), &request == gLastRequest ? 1 : 0});
    return 0;
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

// ---- the serialise instances (the four the mechanic uses): the file, the field's offset from the mechanic, noLog and the name
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

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_mech_addr(const char* name) {
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
uint64_t tpw_mech_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// A ride-deleted message whose GetType answers the queue "GetType" and whose deleted id is `id`.
uint64_t tpw_mech_makemsg(uint32_t id) {
    TestMessage* m = new TestMessage();
    m->mThingId = static_cast<uint16_t>(id);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}

// The offsets and sizes the module relies on (HOST_LAYOUT); the last is the offset of a ride record's slot 0 (RideInfo::Value(0)).
int32_t tpw_mech_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMechanic));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry) == 0x14 ? 0x14 : sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    alignas(16) static unsigned char probe[sizeof(RideInfo)];
    RideInfo* info = reinterpret_cast<RideInfo*>(probe);
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&info->Value(static_cast<RideBalanceSlot>(0))) - probe);
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    return n;
}

// The state a case sets before the call: the mechanic under test, the world's tail block (the world pointer is shifted back to it), the table, the text
// FormatString writes, the message centre (0 or a block) and the ride record GetDBInfo answers.
void tpw_mech_setup(uint64_t mechanic, uint64_t tail, const char* text, uint64_t centre, uint64_t info) {
    gBase = reinterpret_cast<const char*>(mechanic);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[12] = gTable;
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gInfo = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info));
    gNewCentre = nullptr;
    gLastRequest = nullptr;
    std::memset(gFormatText, 0, sizeof(gFormatText));
    for (int32_t i = 0; i < 0x3f && text[i] != 0; ++i) gFormatText[i] = static_cast<unsigned char>(text[i]);
}
void tpw_mech_setrow(int32_t row, uint64_t block) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(block)); }
int64_t tpw_mech_centre() { return Canon(gMessageCentre); }
// The constructor's function static (the flag byte and the sprite index): set before a case, read back after it.
void tpw_mech_setglobals(uint32_t guard, uint32_t sprite) {
    gMechanicSpriteGuard = static_cast<uint8_t>(guard);
    gMechanicSprite = static_cast<int32_t>(sprite);
}
void tpw_mech_globals(uint32_t* out) {
    out[0] = gMechanicSpriteGuard;
    out[1] = static_cast<uint32_t>(gMechanicSprite);
}
// __sinit_mechanic_cpp: the host has no function; the two words are constant initialised, so the wrapper only reads them back.
void tpw_mech_sinit(uint32_t* out) {
    out[0] = gMechanicSinitLow;
    out[1] = gMechanicSinitHigh;
}

// ---- the functions, one wrapper each with C types only
static CMechanic* M(uint64_t block) { return reinterpret_cast<CMechanic*>(static_cast<uintptr_t>(block)); }
uint64_t tpw_mech_ctor1(uint64_t block, uint32_t cell) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}));
}
uint64_t tpw_mech_ctor2(uint64_t block, uint32_t cell, int32_t grade, uint32_t flag, int32_t name) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CMechanic(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}, grade, static_cast<uint8_t>(flag), name));
}
void tpw_mech_init(uint64_t block, uint32_t sprite) { M(block)->Init(static_cast<uint8_t>(sprite)); }
void tpw_mech_receive(uint64_t block, uint64_t message) { M(block)->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message))); }
int32_t tpw_mech_serialise(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return M(block)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
void tpw_mech_final(uint64_t block) { M(block)->FinalDestroy(); }
void tpw_mech_update(uint64_t block) { M(block)->UpdateTurn(); }
void tpw_mech_choose(uint64_t block) { M(block)->ChooseNextAction(); }
void tpw_mech_model(uint64_t block) { M(block)->ModelState(); }
void tpw_mech_setstate(uint64_t block, int32_t state) { M(block)->SetState(static_cast<CStaff::StaffState>(state)); }
void tpw_mech_idle(uint64_t block) { M(block)->ProcessIdle(); }
void tpw_mech_patrol(uint64_t block) { M(block)->ProcessPatrolling(); }
void tpw_mech_walk(uint64_t block) { M(block)->ProcessWalkingToBreakdown(); }
void tpw_mech_repair(uint64_t block) { M(block)->ProcessRepairing(); }

}  // extern "C"
