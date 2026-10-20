// Test library for the park analyser's staff queries (unit w155; libtpw_unit_ParkStaff.dylib, tests/test_ParkStaff.py), main app:
//   src/ParkAnalyser_staff.cpp   the 13 functions; src/MainBalance.cpp is the real balance storage.
//
// The callees outside the unit are fakes that record the same event as the PowerPC stubs and answer from the test's queues: GAME_ASSERTMSG, the world
// accessor (answers world 1; the global gWorld is world 2), the used-thing walk, CStaff::TooTiredToWork and HasPatrolArea, CStaffDB::GetNumStaffOfType,
// CParkTime::GetFunnyTime and TbTimeStamp::operator-. The worlds are small blocks whose pointer is the block's address minus the offset the code reads
// (only the researcher head at +0x1da742 is real memory); the analyser and the things are arena blocks the test fills with Mac-offset words.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "StaffDB.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here (the launcher has the program's definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;

namespace {
CWorld* gWorld1;   // what the world accessor answers
}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld1;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head", 0));
    return result;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next", 0));
    return result;
}

int32_t CStaff::TooTiredToWork() const {
    ev("TooTiredToWork", {lab(this)});
    return static_cast<int32_t>(ret("tired"));
}

int32_t CStaff::HasPatrolArea() const {
    ev("HasPatrolArea", {lab(this)});
    return static_cast<int32_t>(ret("patrol"));
}

int32_t CStaffDB::GetNumStaffOfType(int32_t type) {
    ev("GetNumStaffOfType", {lab(this), type});
    return static_cast<int32_t>(ret("numstaff"));
}

TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g)}, {});
    ev("GetFunnyTime.8", {lab(h)});
    return TbTimeStamp{ret("ft")};
}

TbTimeDiff TbTimeStamp::operator-(const TbTimeStamp& other) const {
    ev("Stamp-", {lab(this), mTicks, lab(&other), other.mTicks});
    TbTimeDiff diff;
    diff.mTicks = ret("minus");
    return diff;
}

extern "C" {

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_pf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets the module relies on (HOST_LAYOUT).
int32_t tpw_pf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLastObjectBuilt));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLastStaffHired));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLongestStay) + sizeof(int32_t));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mStaffDB));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA742));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mDead));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry) >= 8 ? 1 : 0);
    return n;
}

// The two worlds: world 1 is the world accessor's answer, world 2 the global.
void tpw_pf_worlds(uint64_t world1, uint64_t world2) {
    gWorld1 = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world1));
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world2));
}

// The thing table's row (word 0 of the entry).
void tpw_pf_row(int32_t row, uint64_t thing) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }

// The balance's integer slots first .. first + count - 1.
void tpw_pf_balance(int32_t first, int32_t count, const int32_t* values) {
    for (int32_t i = 0; i < count; i++) gMainBalance.mValues[first + i].mInteger = values[i];
}

// ---- the functions, one wrapper each (the analyser is the first argument)
CParkAnalyser* An(uint64_t analyser) { return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser)); }

void tpw_pf_hired(uint64_t a) { An(a)->HiredSomeone(); }
uint32_t tpw_pf_happiness(uint64_t a, uint32_t arg) {
    float level = An(a)->GetStaffHappinessLevel(arg);
    uint32_t bits;
    std::memcpy(&bits, &level, sizeof(bits));
    return bits;
}
int32_t tpw_pf_days_obj(uint64_t a) { return An(a)->GetDaysSinceLastObjectBuilt(); }
int32_t tpw_pf_days_staff(uint64_t a) { return An(a)->GetDaysSinceLastStaffHired(); }
int32_t tpw_pf_nstaff(uint64_t a) { return An(a)->NumberOfStaff(); }
int32_t tpw_pf_tired(uint64_t a) { return An(a)->NumberOfStaffTooTiredToWork(); }
int32_t tpw_pf_patrol(uint64_t a, int32_t model) { return An(a)->NumberOfStaffTypeWithPatrolArea(static_cast<CThing::ThingModel>(model)); }
int32_t tpw_pf_quality(uint64_t a, int32_t model) { return An(a)->AverageQualityOfStaffType(static_cast<CThing::ThingModel>(model)); }
int32_t tpw_pf_hap_type(uint64_t a, int32_t model) { return An(a)->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model)); }
int32_t tpw_pf_hap_staff(uint64_t a) { return An(a)->AverageHappinessOfStaff(); }
int32_t tpw_pf_hire(uint64_t a, int32_t type) { return An(a)->CurrentNumberOfStaffTypeForHire(static_cast<StaffType>(type)); }
int32_t tpw_pf_research(uint64_t a) { return An(a)->CurrentResearcherAbility(); }
int32_t tpw_pf_maxres(uint64_t a) { return An(a)->MaxResearcherAbility(); }

}  // extern "C"

// The balance reader is not under test: the static initialiser and destructor of gMainBalance call these (nothing is read from a file).
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}
