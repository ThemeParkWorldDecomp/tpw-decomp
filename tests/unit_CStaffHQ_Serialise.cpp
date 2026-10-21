// Test library for CStaffHQ::Serialise, ReceiveMessage, UpdateMonthly and FindNearestAvailableGuard (unit v5918; libtpw_unit_CStaffHQ_Serialise.dylib,
// tests/test_CStaffHQ_Serialise.py), main app:
//   src/game/buildings/StaffHQ.cpp   the four functions and the halfwords 0x102a11bc.. they compare with
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The globals of other
// files (gWorld, gWorldMap, gThingArray) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {
uintptr_t gHqBase;                       // the object under test, so the fakes can name a field by its offset
int16_t gFakeCell[8];                    // what GetRegionalEffectCell answers
CWorld* gWorldBlock;                     // one zeroed block of sizeof(CWorld), made on the first request
const uintptr_t kAnalyser = 0x50000200;  // a fake analyser: never dereferenced

struct FakeMsg : CMsgStaffHQPrank {
    int32_t GetType() const override {
        ev("GetType", {1});
        return static_cast<int32_t>(ret("msgtype"));
    }
};
FakeMsg gMsg;

int32_t VarFake(const char* key, void* file, void* value, const char* name, int32_t noLog) {
    ev((std::string("off:") + key).c_str(), {static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gHqBase)});
    ev(key, {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), noLog}, {name});
    return static_cast<int32_t>(ret(key));
}
}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return VarFake("SAVEVAR<int>", file, value, name, noLog);
}
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return VarFake("LOADVAR<int>", file, value, name, noLog);
}
typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdTag;
template<> int32_t SAVEVARSTRUCT<ThingIdTag>(void* file, ThingIdTag* value, const char* name, int32_t noLog) {
    return VarFake("SAVEVARSTRUCT<tag>", file, value, name, noLog);
}
template<> int32_t LOADVARSTRUCT<ThingIdTag>(void* file, ThingIdTag* value, const char* name, int32_t noLog) {
    return VarFake("LOADVARSTRUCT<tag>", file, value, name, noLog);
}
template<> int32_t SAVEVARSTRUCT<CStaffHQ::StrikeType>(void* file, CStaffHQ::StrikeType* value, const char* name, int32_t noLog) {
    return VarFake("SAVEVARSTRUCT<Strike>", file, value, name, noLog);
}
template<> int32_t LOADVARSTRUCT<CStaffHQ::StrikeType>(void* file, CStaffHQ::StrikeType* value, const char* name, int32_t noLog) {
    return VarFake("LOADVARSTRUCT<Strike>", file, value, name, noLog);
}

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CThing::Serialise", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), save, version, static_cast<int64_t>(unused)});
    return static_cast<int32_t>(ret("CThing::Serialise"));
}
int32_t CTrainingCentre::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CTrainingCentre::Serialise", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), save, version, static_cast<int64_t>(unused)});
    return static_cast<int32_t>(ret("CTrainingCentre::Serialise"));
}
void CTrainingCentre::UpdateMonthly() { ev("CTrainingCentre::UpdateMonthly", {lab(this)}); }

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(kAnalyser);
}
int32_t CParkAnalyser::GetVisitorsInPark(uint32_t arg) const {
    ev("GetVisitorsInPark", {lab(this), static_cast<int64_t>(arg)});
    return static_cast<int32_t>(ret("GetVisitorsInPark"));
}
CRegionEffectCell* CMap::GetRegionalEffectCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("GetRegionalEffectCell", {lab(this), cell.mIndex});
    return reinterpret_cast<CRegionEffectCell*>(gFakeCell);
}
void CGuard::ProsecutePerp(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> perp) { ev("ProsecutePerp", {lab(this), perp.mIndex}); }
bool CGuard::IsAvailable() const {
    ev("IsAvailable", {lab(this)});
    return ret("IsAvailable") != 0;
}
int32_t CStaffHQ::IsStriking(StaffType type) {
    ev("IsStriking", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("IsStriking"));
}
void CStaffHQ::StopStrikeInProgress(StaffType type) { ev("StopStrikeInProgress", {lab(this), static_cast<int32_t>(type)}); }
void CStaffHQ::ConsiderStriking(StaffType type) { ev("ConsiderStriking", {lab(this), static_cast<int32_t>(type)}); }

extern "C" {

uint64_t tpw_sh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gWorldMap", &gWorldMap},
        {"noguard", &gStaffHQNoGuard},
        {"s0", &gStaffHQNoStaff[0]},
        {"s1", &gStaffHQNoStaff[1]},
        {"s2", &gStaffHQNoStaff[2]},
        {"s3", &gStaffHQNoStaff[3]},
        {"s4", &gStaffHQNoStaff[4]},
        {"gend", &gStaffHQGuardListEnd},
        {"regcell", gFakeCell},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The world block (sizeof(CWorld) bytes), the same one every case.
uint64_t tpw_sh_world() {
    if (gWorldBlock == nullptr) gWorldBlock = static_cast<CWorld*>(tpw::ArenaAllocZeroed(sizeof(CWorld)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWorldBlock));
}

void tpw_sh_setthing(uint32_t row, uint64_t thing) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }

// The message: a prank carrying the perp's id; GetType answers from the queue "msgtype".
uint64_t tpw_sh_msg(uint32_t perp) {
    gMsg.mPerp = static_cast<uint16_t>(perp);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(static_cast<CMsg*>(&gMsg)));
}

int32_t tpw_sh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73C));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA744));
    out[n++] = static_cast<int64_t>(sizeof(CStaffHQ));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mStrikes));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF60));
    out[n++] = static_cast<int64_t>(offsetof(CGuard, mNext));
    out[n++] = static_cast<int64_t>(sizeof(CGuard) <= 0x300 ? 1 : 0);
    out[n++] = static_cast<int64_t>(offsetof(CMsgStaffHQPrank, mPerp) > 0 ? 1 : 0);
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    return n;
}

uint32_t tpw_sh_ser(uint64_t hq, uint32_t file, int32_t save, int32_t version, uint32_t unused) {
    gHqBase = static_cast<uintptr_t>(hq);
    return static_cast<uint32_t>(reinterpret_cast<CStaffHQ*>(gHqBase)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused));
}
void tpw_sh_recv(uint64_t hq, uint64_t msg) {
    reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq))->ReceiveMessage(*reinterpret_cast<CMsg*>(static_cast<uintptr_t>(msg)));
}
void tpw_sh_month(uint64_t hq) { reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq))->UpdateMonthly(); }
uint32_t tpw_sh_find(uint64_t hq, uint32_t cell) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(cell);
    return reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq))->FindNearestAvailableGuard(tag).mIndex;
}

}  // extern "C"
