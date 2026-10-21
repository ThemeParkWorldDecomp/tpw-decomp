// Test library for eight advisor scorers (unit v5252; libtpw_unit_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS.dylib), main app:
//   src/game/advisor/AdvisorScoring.cpp
//
// The world, bank account and analyser callees are recording fakes; the objects are tokens (0x4000, 0x5000, 0x6000) that are never read.
// gGameWorld is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "ThingReceivers.h"
#include "TbTimeDiff.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CWorld* gGameWorld;

// ---- the fakes
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(0x5000);
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(const_cast<CBankAccount*>(this))});
    return static_cast<int32_t>(ret("GetBalance"));
}

bool CBankAccount::AnyLoanAvailable() {
    ev("AnyLoanAvailable", {lab(this)});
    return ret("AnyLoanAvailable") != 0;
}

TbTimeDiff CBankAccount::BeenInTheRedFor() {
    ev("BeenInTheRedFor", {lab(this)});
    TbTimeDiff d;
    uint64_t hi = static_cast<uint32_t>(ret("RedHi"));
    uint64_t lo = static_cast<uint32_t>(ret("RedLo"));
    d.mTicks = static_cast<int64_t>((hi << 32) | lo);
    return d;
}

int32_t CBankAccount::GetNumberOfLoansBought() {
    ev("GetNumberOfLoansBought", {lab(this)});
    return static_cast<int32_t>(ret("GetNumberOfLoansBought"));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(0x6000);
}

int32_t CParkAnalyser::MisbehavingVisitorsLastMonth(bool gotAway) const {
    ev("MisbehavingVisitorsLastMonth", {lab(const_cast<CParkAnalyser*>(this)), static_cast<int64_t>(gotAway ? 1 : 0)});
    return static_cast<int32_t>(ret("Misbehaving"));
}

int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(const_cast<CParkAnalyser*>(this)), static_cast<int64_t>(type), static_cast<int64_t>(level)});
    return static_cast<int32_t>(ret("ObjectTypes"));
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(const_cast<CParkAnalyser*>(this)), static_cast<int64_t>(model)});
    return static_cast<int32_t>(ret("StaffType"));
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    if (std::strcmp(name, "gGameWorld") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gGameWorld));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host address of the balance value at a Mac offset from the scoring object, relative to the object.
int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorScoring, mBalance) + offsetof(CAdvisorBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    return n;
}

#define WRAP(NAME) \
    int32_t tpw_adv_##NAME(uint64_t self) { return reinterpret_cast<CAdvisorScoring*>(self)->_ADVISOR_##NAME(); }
WRAP(IN_THE_RED_SIX_MONTHS)
WRAP(TAKE_LOAN)
WRAP(IN_THE_RED_THREE_MONTHS)
WRAP(IN_THE_RED_MONTH_LEFT)
WRAP(WEALTH_HIGH)
WRAP(GENERAL_LAX_SECURITY)
WRAP(LOTS_HANDYMAN)
WRAP(LOTS_ENTERTAINERS)

}  // extern "C"
