// Test library for the staff member, part 3 (unit w191; libtpw_unit_StaffBehaviour.dylib, tests/test_StaffBehaviour.py), main app:
//   src/game/people/staff.cpp   CStaff: ProcessWalkingToRestArea, ProcessResting, ProcessWalkingToStrike, ProcessStriking, ProcessBeingStunned, GetTiredPatrolling,
//                             GetTiredWorking, FindNearestRestingPlace, DoGeneralStaffThing, GoToRestArea, StopResting, SetPatrolArea, GetPatrolArea,
//                             CancelPatrolArea, HasPatrolArea, InsidePatrolArea, FindDestInPatrolArea
//
// The callees outside it are fakes that record the same events as the PowerPC stubs. The globals of other files are defined here: gWorld (pointing into a
// shifted tail block), gWorldMap, gMainBalance (raw storage), gThingArray and gMessageCentre.
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
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};

// ---- globals of other files
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
CMessageCentre* gMessageCentre;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the staff member under test (offsets in the logs are from it)
const char* gMapBase;                      // the map (cell offsets in the logs are from it)
void* gNewCentre = nullptr;                // the block the fake operator new made last (its first header logs the event)

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }
int32_t MapOff(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gMapBase); }
int64_t Canon(const void* pointer) {
    const int32_t label = lab(pointer);
    if (label >= 0) return label;
    return (1ll << 33) | static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu);
}

// The number of "%d" in a format: the trace and the assertion log that many integer arguments.
int32_t CountInts(const char* format) {
    int32_t n = 0;
    for (const char* p = format; (p = std::strstr(p, "%d")) != nullptr; p += 2) ++n;
    return n;
}

CStaffHQ* const kStaffHQ = reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(0x6000));
RSSE_SCRIPT* const kScript = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(0x7000));
CParkAnalyser* const kAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x4000));

}  // namespace

// ---- the message centre: PostMessage (the staff-room event's fields), the 29 headers (the first stands for __construct_array's call) and the nothrow operator new
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const CMsgStaffRoomEvent& event = static_cast<const CMsgStaffRoomEvent&>(message);
    ev("PostMessage", {lab(this), static_cast<int64_t>(event.mFlags), event.mThingId, static_cast<int64_t>(static_cast<uint32_t>(event.mArriving))});
    return static_cast<int32_t>(ret("PostMessage", 0));
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

// The staff-room event's destructor and GetType belong to src/game/people/staff.cpp (unit w189): silent here.
CMsgStaffRoomEvent::~CMsgStaffRoomEvent() {}
int32_t CMsgStaffRoomEvent::GetType() const { return 15; }

// ---- the CPeep members
int32_t CPeep::PeepWalks() {
    ev("CPeep::PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::PeepWalks", 0));
}
int32_t CPeep::SetDest(MapId cell) {
    ev("CPeep::SetDest", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("CPeep::SetDest", 0));
}
int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("CPeep::SetDestPos", {lab(this), x.mCell, x.mOffset, y.mCell, y.mOffset});
    return static_cast<int32_t>(ret("CPeep::SetDestPos", 0));
}

// ---- the other CStaff members and the five ChooseNextAction
void CStaff::SetState(StaffState state) { ev("CStaff::SetState", {lab(this), static_cast<int32_t>(state)}); }
StaffType CStaff::GetStaffType() {
    ev("CStaff::GetStaffType", {lab(this)});
    return static_cast<StaffType>(ret("CStaff::GetStaffType", 0));
}
int32_t CStaff::IsStriking() {
    ev("CStaff::IsStriking", {lab(this)});
    return static_cast<int32_t>(ret("CStaff::IsStriking", 0));
}
void CEntertainer::ChooseNextAction() { ev("CEntertainer::ChooseNextAction", {lab(this)}); }
void CGuard::ChooseNextAction() { ev("CGuard::ChooseNextAction", {lab(this)}); }
void CHandyman::ChooseNextAction() { ev("CHandyman::ChooseNextAction", {lab(this)}); }
void CMechanic::ChooseNextAction() { ev("CMechanic::ChooseNextAction", {lab(this)}); }
void CResearcher::ChooseNextAction() { ev("CResearcher::ChooseNextAction", {lab(this)}); }

// ---- the staff headquarters
int32_t CStaffHQ::IsStriking(StaffType type) {
    ev("CStaffHQ::IsStriking", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("CStaffHQ::IsStriking", 0));
}
void CStaffHQ::StopStrikeInProgress(StaffType type) { ev("CStaffHQ::StopStrikeInProgress", {lab(this), static_cast<int32_t>(type)}); }

// ---- the thoughts and the delayed script (reached at the staff member + 0x30 and + 0xc)
void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) { ev("CThoughts::ShowThoughtBubble", {Off(this), static_cast<int32_t>(type), always}); }
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}

// ---- the world and the analyser
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {});
    return static_cast<uint32_t>(ret("CWorld::GetRand", 0));
}
int32_t CWorld::IsParkClosed() const {
    ev("CWorld::IsParkClosed", {});
    return static_cast<int32_t>(ret("CWorld::IsParkClosed", 0));
}
int32_t CWorld::ParkGates_GetStatus() const {
    ev("CWorld::ParkGates_GetStatus", {});
    return static_cast<int32_t>(ret("CWorld::ParkGates_GetStatus", 0));
}
CStaffHQ* CWorld::GetStaffHQ() {
    ev("CWorld::GetStaffHQ", {});
    return kStaffHQ;
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {});
    return kAnalyser;
}
int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("CParkAnalyser::NumberOfVisitorsInPark", {lab(this)});
    return static_cast<int32_t>(ret("CParkAnalyser::NumberOfVisitorsInPark", 0));
}
ThingId CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}

// ---- the map
int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("CMap::IsXYValid", {x, y});
    return static_cast<int32_t>(ret("CMap::IsXYValid", 0));
}
int32_t CMapCell::IsPath() const {
    ev("CMapCell::IsPath", {MapOff(this)});
    return static_cast<int32_t>(ret("CMapCell::IsPath", 0));
}

// ---- the script variables
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    return kScript;
}
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), index});
    return static_cast<int32_t>(ret("RSSE_Getvar", 0));
}
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) { ev("RSSE_Setvar", {lab(script), index, value}); }

// ---- the runtime: the "%d" arguments are logged after the level (CTRACE) or the condition (GAME_ASSERTMSG)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t v[4] = {condition, 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n && i < 3; ++i) v[1 + i] = va_arg(args, uint32_t);
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
    for (int32_t i = 0; i < n && i < 3; ++i) v[1 + i] = va_arg(args, uint32_t);
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

// The host address of a global by the module's name for it.
uint64_t tpw_stb_addr(const char* name) {
    if (std::strcmp(name, "balance_values") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[0]));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_stb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_stb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CStaff));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    return n;
}

// The state a case sets before the call: the staff member under test, the world's tail block (the world pointer is shifted back to it), the map block and the
// message centre (0 or a block).
void tpw_stb_setup(uint64_t staff, uint64_t tail, uint64_t centre, uint64_t map) {
    gBase = reinterpret_cast<const char*>(staff);
    gMapBase = reinterpret_cast<const char*>(map);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gNewCentre = nullptr;
}
void tpw_stb_setrow(int32_t row, uint64_t block) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(block)); }
int64_t tpw_stb_centre() { return Canon(gMessageCentre); }

// ---- the functions, one wrapper each with C types only
static CStaff* S(uint64_t block) { return reinterpret_cast<CStaff*>(static_cast<uintptr_t>(block)); }
void tpw_stb_walkrest(uint64_t block) { S(block)->ProcessWalkingToRestArea(); }
void tpw_stb_resting(uint64_t block) { S(block)->ProcessResting(); }
void tpw_stb_walkstrike(uint64_t block) { S(block)->ProcessWalkingToStrike(); }
void tpw_stb_striking(uint64_t block) { S(block)->ProcessStriking(); }
void tpw_stb_stunned(uint64_t block) { S(block)->ProcessBeingStunned(); }
void tpw_stb_tiredpatrol(uint64_t block) { S(block)->GetTiredPatrolling(); }
void tpw_stb_tiredwork(uint64_t block) { S(block)->GetTiredWorking(); }
uint32_t tpw_stb_findrest(uint64_t block) { return S(block)->FindNearestRestingPlace().mIndex; }
int32_t tpw_stb_general(uint64_t block) { return S(block)->DoGeneralStaffThing(); }
uint32_t tpw_stb_gorest(uint64_t block) { return S(block)->GoToRestArea().mIndex; }
void tpw_stb_stop(uint64_t block) { S(block)->StopResting(); }
void tpw_stb_setpatrol(uint64_t block, uint32_t bl, uint32_t tr) { S(block)->SetPatrolArea(MapId{static_cast<uint16_t>(bl)}, MapId{static_cast<uint16_t>(tr)}); }
// GetPatrolArea: the two out words start as the case's, the answer packs them (start low, end high).
uint32_t tpw_stb_getpatrol(uint64_t block, uint32_t start, uint32_t end) {
    MapId a{static_cast<uint16_t>(start)};
    MapId b{static_cast<uint16_t>(end)};
    const_cast<const CStaff*>(S(block))->GetPatrolArea(&a, &b);
    return static_cast<uint32_t>(a.mIndex) | (static_cast<uint32_t>(b.mIndex) << 16);
}
void tpw_stb_cancel(uint64_t block) { S(block)->CancelPatrolArea(); }
int32_t tpw_stb_has(uint64_t block) { return const_cast<const CStaff*>(S(block))->HasPatrolArea(); }
int32_t tpw_stb_inside(uint64_t block, uint32_t cell) { return const_cast<const CStaff*>(S(block))->InsidePatrolArea(MapId{static_cast<uint16_t>(cell)}); }
int32_t tpw_stb_finddest(uint64_t block) { return S(block)->FindDestInPatrolArea(); }

}  // extern "C"
