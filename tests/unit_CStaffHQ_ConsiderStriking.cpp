// Test library for CStaffHQ::ConsiderStriking (unit v5681; libtpw_unit_CStaffHQ_ConsiderStriking.dylib, tests/test_CStaffHQ_ConsiderStriking.py), main app:
//   src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_ConsiderStriking.cpp
//
// The callees outside the unit are recording fakes: CTRACE, CParkTime::GetFunnyTimeSinceSessionStart, StaffTypeHasAGrievance,
// StopStrikeInProgress, the CMsgStaffInfo constructor, CMessageCentre::PostMessage and the two allocation helpers.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files
CMessageCentre* gMessageCentre = nullptr;
CWorld* gWorld = nullptr;
CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;
static void* gNewBlock = nullptr;

// ---- the fakes
// Only so the src file's weak program version of ConstructSubscriptions links; never run here.
CSubscriptionHeader::CSubscriptionHeader() {}
CSubscriptionHeader::~CSubscriptionHeader() {}
void CTRACE(uint32_t level, const char* format, ...) {
    if (level == 2) {
        va_list args;
        va_start(args, format);
        int turn = va_arg(args, int);
        va_end(args);
        ev("CTRACE2", {static_cast<int64_t>(level), static_cast<int64_t>(static_cast<uint32_t>(turn))}, {format});
    } else {
        ev("CTRACE", {static_cast<int64_t>(level)}, {format});
    }
}

TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("Funny", {lab(this)});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(ret("hi"))) << 32) | static_cast<uint32_t>(ret("lo")));
    return d;
}

int32_t CStaffHQ::StaffTypeHasAGrievance(StaffType type) {
    ev("Grievance", {lab(this), static_cast<int64_t>(type)});
    return static_cast<int32_t>(ret("Grievance"));
}

void CStaffHQ::StopStrikeInProgress(StaffType type) { ev("Stop", {lab(this), static_cast<int64_t>(type)}); }

CMsgStaffInfo::CMsgStaffInfo(StaffType staff, StaffSituation situation) {
    ev("MsgStaffInfo", {static_cast<int64_t>(staff), static_cast<int64_t>(situation)});
    mFlags = 0;
    mStaff = staff;
    mSituation = situation;
}
int32_t CMsgStaffInfo::GetType() const { return 0x17; }
CMsgStaffInfo::~CMsgStaffInfo() {}

int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const CMsgStaffInfo& m = static_cast<const CMsgStaffInfo&>(message);
    ev("Post", {lab(this), static_cast<int64_t>(m.mFlags), static_cast<int64_t>(static_cast<uint32_t>(m.mStaff)),
                static_cast<int64_t>(static_cast<uint32_t>(m.mSituation))});
    return 1;
}

void* staffhq_callee::OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return gNewBlock;
}
void staffhq_callee::ConstructSubscriptions(void* mem) { ev("construct", {lab(mem), 0x10, 0x1d}); }

extern "C" {

uint64_t tpw_hq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMessageCentre", &gMessageCentre}, {"gWorld", &gWorld}, {"gGameWorld", &gGameWorld}, {"gNewBlock", &gNewBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mStrikes));
    out[n++] = static_cast<int64_t>(sizeof(CStaffHQ));
    return n;
}

void tpw_hq_consider(uint64_t hq, int32_t type) {
    reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq))->ConsiderStriking(static_cast<StaffType>(type));
}

}  // extern "C"
