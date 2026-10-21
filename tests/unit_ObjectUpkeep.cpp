// Test library for a ride's wear and upkeep (unit w199; libtpw_unit_ObjectUpkeep.dylib, tests/test_ObjectUpkeep.py), main app:
//   src/game/Object.cpp   the 18 CObject members GetProspectiveDamagePerTurn, GetProspectiveReliability, AddWearAndTear, RepairFully, RequestMaintenance,
//                           IsBeingWorkedUpon, AssignStaffMember, UnassignStaffMember, GetAssignedStaffMember, IsCondemned, IsDirty, IsFlaggedForMaintenance,
//                           RequiresUpgrade, StartFixing, IsPlayingFixAnim, CleanFully, RequestBreakdown and SetWornFlag
//   src/game/Object.cpp  CObject::IsBrokenDown (another unit), run as translated
//
// Every other callee is a fake that records the same event as the PowerPC stub: the ride database's GetRideInfo (it answers the case's record), IsRunning,
// IsCoaster, IsCoasterUseable, IsBOQConnected, GetFrontOfPhysicalQueue, SetState (it stores the state), the script calls (GetScriptPointer hands out the
// next block of a pool), the ride effect, the sprite spawn (it logs the six words of the data block it is given), the hoardings, the world's advisor and
// bank, the map's regional effectors, the two staff queries, the advisor request and its queue, the game type's constructor, the trace and the assertion.
// The globals of other files are defined here: gWorld (pointing into a shifted tail block), gWorldMap, gThingArray, gRideDatabase, gRideAnimSets.
// CThing::GetThingId (src/game/Thing.cpp, another unit) is a copy here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "AdvisorQueue.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
CRideDatabase gRideDatabase;
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

constexpr uintptr_t kTail = 0x1da70c;       // the world byte the tail block starts at
constexpr int32_t kPool = 32;               // the blocks GetScriptPointer hands out, in turn
RideInfo* gInfo = nullptr;                   // the ride record GetRideInfo answers
int32_t gCtorType = 0;                       // what the game type's constructor sets
const void* gLastRequest = nullptr;          // the last advisor request constructed
alignas(16) unsigned char gPool[kPool][16];
int32_t gPoolN = 0;

// The number of "%d" in a format: the trace and the assertion log that many integer arguments (two texts have a %d the original passes nothing for: they log none).
int32_t CountInts(const char* format) {
    if (std::strstr(format, "repairing fully") != nullptr || std::strstr(format, "requested breakdown") != nullptr) return 0;
    int32_t n = 0;
    for (const char* p = format; (p = std::strstr(p, "%d")) != nullptr; p += 2) ++n;
    return n;
}

CObject* Obj(uint64_t block) { return reinterpret_cast<CObject*>(static_cast<uintptr_t>(block)); }

}  // namespace

// ---- the fakes
CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(gCtorType);
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return gInfo;
}

int32_t CObject::IsRunning() const {
    ev("CObject::IsRunning", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsRunning", 0));
}
int32_t CObject::IsCoaster() const {
    ev("CObject::IsCoaster", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsCoaster", 0));
}
int32_t IsCoasterUseable(CObject* object) {
    ev("IsCoasterUseable", {lab(object)});
    return static_cast<int32_t>(ret("IsCoasterUseable", 0));
}
int32_t CObject::IsBOQConnected() {
    ev("CObject::IsBOQConnected", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsBOQConnected", 0));
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetFrontOfPhysicalQueue() const {
    ev("CObject::GetFrontOfPhysicalQueue", {lab(this)});
    return {static_cast<uint16_t>(ret("CObject::GetFrontOfPhysicalQueue", 0))};
}
void CObject::SetState(ObjectState state) {
    ev("CObject::SetState", {lab(this), static_cast<int32_t>(state)});
    mState = state;
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    return reinterpret_cast<RSSE_SCRIPT*>(gPool[(gPoolN++) % kPool]);
}
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), index});
    char key[16];
    std::snprintf(key, sizeof(key), "var%d", index);
    return static_cast<int32_t>(ret(key, 0));
}
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) { ev("RSSE_Setvar", {lab(script), index, value}); }
int32_t RSSE_GetRideSpeed(RSSE_SCRIPT* script) {
    ev("RSSE_GetRideSpeed", {lab(script)});
    return static_cast<int32_t>(ret("RSSE_GetRideSpeed", 0));
}
void RSSE_SetRideSpeed(RSSE_SCRIPT* script, int32_t speed) { ev("RSSE_SetRideSpeed", {lab(script), speed}); }
void RSSE_UpgradeRideEffect(int32_t handle) { ev("RSSE_UpgradeRideEffect", {handle}); }
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    const uint32_t* w = static_cast<const uint32_t*>(block);
    ev("ESP_SpawnScript", {w[0], w[1], w[2], w[3], w[4], w[5], static_cast<int32_t>(sprite), a, static_cast<int32_t>(acts), fbits(x), fbits(y), fbits(z)});
    return static_cast<int32_t>(ret("ESP_SpawnScript", 0));
}
void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag) { ev("ActivateHoardings", {lab(set), flag}); }
void DeActivateHoardings(tag_sRideAnimSet* set) { ev("DeActivateHoardings", {lab(set)}); }

CAdvisor* CWorld::GetAdvisor() {
    ev("CWorld::GetAdvisor", {});
    return reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(0x5000));
}
CBankAccount* CWorld::GetBankAccount() {
    ev("CWorld::GetBankAccount", {});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x6000));
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("CBankAccount::MakeWithdrawal", {lab(this), amount}); }
void CMap::AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::AddRegionalEffector", {lab(this), static_cast<int32_t>(effector), cell.mIndex});
}
void CMap::RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::RemoveRegionalEffector", {lab(this), static_cast<int32_t>(effector), cell.mIndex});
}
int32_t CStaff::IsAllocatedToRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) {
    ev("CStaff::IsAllocatedToRide", {lab(this), ride.mIndex});
    return static_cast<int32_t>(ret("CStaff::IsAllocatedToRide", 0));
}
int32_t CStaff::IsWorkingOnAllocatedObject(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> object) {
    ev("CStaff::IsWorkingOnAllocatedObject", {lab(this), object.mIndex});
    return static_cast<int32_t>(ret("CStaff::IsWorkingOnAllocatedObject", 0));
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
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const { return {mThingId}; }

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

// The host address of a block by the module's name for it.
uint64_t tpw_oup_addr(const char* name) {
    if (std::strcmp(name, "pool") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gPool));
    if (std::strcmp(name, "db") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gRideDatabase));
    return 0;
}

// A zeroed block the module fills with Mac-offset words.
uint64_t tpw_oup_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT); the fifth is the offset of a ride record's slot 0 (RideInfo::Value(0)).
int32_t tpw_oup_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    alignas(16) static unsigned char probe[sizeof(RideInfo)];
    RideInfo* info = reinterpret_cast<RideInfo*>(probe);
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&info->Value(static_cast<RideBalanceSlot>(0))) - probe);
    return n;
}

// The state a case sets before the call: the world's tail block (the world pointer is shifted back to it), the ride record, the game type constructor's
// answer, the map; the pool starts again.
void tpw_oup_setup(uint64_t tail, uint64_t info, int32_t ctortype) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(0x7000));
    gInfo = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(info));
    gCtorType = ctortype;
    gLastRequest = nullptr;
    gPoolN = 0;
}
void tpw_oup_setrow(int32_t row, uint64_t block) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(block)); }
void tpw_oup_setset(int32_t index, uint64_t block) { gRideAnimSets[index] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(block)); }
void tpw_oup_mode(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_oup_type() { return TheGameType().mType; }

// ---- the functions, one wrapper each with C types only
double tpw_oup_damage(uint64_t block, float speed, float capacity, float duration, uint32_t use) {
    return Obj(block)->GetProspectiveDamagePerTurn(speed, capacity, duration, (use & 0xff) != 0);
}
int32_t tpw_oup_reliab(uint64_t block, float speed, float capacity, float duration) { return Obj(block)->GetProspectiveReliability(speed, capacity, duration); }
void tpw_oup_wear(uint64_t block) { Obj(block)->AddWearAndTear(); }
void tpw_oup_repair(uint64_t block, uint32_t staff) { Obj(block)->RepairFully(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(staff)}); }
int32_t tpw_oup_maint(uint64_t block, int32_t on) { return Obj(block)->RequestMaintenance(on); }
int32_t tpw_oup_working(uint64_t block) { return Obj(block)->IsBeingWorkedUpon(); }
void tpw_oup_assign(uint64_t block, uint32_t staff) { Obj(block)->AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(staff)}); }
void tpw_oup_unassign(uint64_t block) { Obj(block)->UnassignStaffMember(); }
uint32_t tpw_oup_assigned(uint64_t block) { return Obj(block)->GetAssignedStaffMember().mIndex; }
int32_t tpw_oup_condemned(uint64_t block) { return Obj(block)->IsCondemned(); }
int32_t tpw_oup_dirty(uint64_t block) { return Obj(block)->IsDirty(); }
int32_t tpw_oup_flagged(uint64_t block) { return Obj(block)->IsFlaggedForMaintenance(); }
int32_t tpw_oup_upgrade(uint64_t block) { return Obj(block)->RequiresUpgrade(); }
void tpw_oup_fixing(uint64_t block) { Obj(block)->StartFixing(); }
int32_t tpw_oup_playfix(uint64_t block) { return Obj(block)->IsPlayingFixAnim(); }
void tpw_oup_clean(uint64_t block) { Obj(block)->CleanFully(); }
void tpw_oup_breakdown(uint64_t block) { Obj(block)->RequestBreakdown(); }
void tpw_oup_worn(uint64_t block) { Obj(block)->SetWornFlag(); }

}  // extern "C"
