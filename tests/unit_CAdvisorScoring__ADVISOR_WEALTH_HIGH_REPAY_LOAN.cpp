// Test library for twelve advisor scorers (unit v5896; libtpw_unit_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN.dylib,
// tests/test_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN.py), main app:
//   src/game/advisor/AdvisorScoring.cpp   the twelve scorers and gAdvisorNoRideQueue
//
// The callees outside the unit (the world, the bank account, the park analyser, the park time, the history sum) are fakes that record the
// arguments the real ones read and answer from the test's queues. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "AdvisorScoring.h"
#include "History.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CMainBalance gMainBalance;

void CBalanceTable::Init() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance::~CMainBalance() {}

namespace {

alignas(16) char gObjBlock[sizeof(CAdvisorScoring)];
alignas(16) char gWorldBlock[0x1000];
alignas(16) char gAnalyserBlock[0x30000];
alignas(16) char gBankBlock[0x1000];

CAdvisorScoring* Obj() { return reinterpret_cast<CAdvisorScoring*>(gObjBlock); }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gAnalyserBlock); }

float FromBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

constexpr int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return Analyser();
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gBankBlock);
}

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("closed"));
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("balance"));
}

uint32_t CBankAccount::TotalOutstandingOnLoans() const {
    ev("TotalOutstandingOnLoans", {lab(this)});
    return static_cast<uint32_t>(ret("total"));
}

uint32_t CBankAccount::GetCheapestLoanTaken() const {
    ev("GetCheapestLoanTaken", {lab(this)});
    return static_cast<uint32_t>(ret("cheapest"));
}

uint32_t CBankAccount::OutstandingOnLoan(uint32_t loan) const {
    ev("OutstandingOnLoan", {lab(this), loan});
    return static_cast<uint32_t>(ret("owed"));
}

bool CParkAnalyser::HaveCompleteSetOfOpinions(int32_t opinion) const {
    ev("HaveCompleteSetOfOpinions", {lab(this), opinion});
    return ret("complete") != 0;
}

uint8_t CParkAnalyser::GetAverageOpinionOn(int32_t opinion) const {
    ev("GetAverageOpinionOn", {lab(this), opinion});
    return static_cast<uint8_t>(ret("avg"));
}

int32_t CParkAnalyser::NumberOfObjectTypeInPark(UIObjType type, int32_t level) const {
    ev("NumberOfObjectTypeInPark", {lab(this), static_cast<int32_t>(type), level});
    return static_cast<int32_t>(ret("nobj"));
}

int32_t CParkAnalyser::NumberOfStaffType(CThing::ThingModel model) const {
    ev("NumberOfStaffType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("nstaff"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CParkAnalyser::FindRideWithUnconnectedQueue(bool any) {
    ev("FindRideWithUnconnectedQueue", {lab(this), any});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("unconn"));
    return id;
}

float CParkAnalyser::GetAverageToiletQueueLenght() const {
    ev("GetAverageToiletQueueLenght", {lab(this)});
    return FromBits(static_cast<uint32_t>(ret("queue")));
}

bool CParkAnalyser::GatesOpen() const {
    ev("GatesOpen", {lab(this)});
    return ret("gates") != 0;
}

int32_t CParkAnalyser::NumberOfToiletsInPark() const {
    ev("NumberOfToiletsInPark", {lab(this)});
    return static_cast<int32_t>(ret("toilets"));
}

int32_t CParkAnalyser::NumberOfVisitorsNeedingToilet(uint8_t level) const {
    ev("NumberOfVisitorsNeedingToilet", {lab(this), level});
    return static_cast<int32_t>(ret("needing"));
}

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(this)});
    return static_cast<int32_t>(ret("inpark"));
}

int32_t CParkAnalyser::TypesOfParticularObjectTypeInPark(UIObjType type, bool includeGoldenTickets) const {
    ev("TypesOfParticularObjectTypeInPark", {lab(this), static_cast<int32_t>(type), includeGoldenTickets});
    return static_cast<int32_t>(ret("types"));
}

int32_t CParkAnalyser::GetDaysSinceLastObjectBuilt() const {
    ev("GetDaysSinceLastObjectBuilt", {lab(this)});
    return static_cast<int32_t>(ret("days"));
}

TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("GetFunnyTimeSinceSessionStart", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - gWorldBlock)});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(ret("thi"))) << 32) | static_cast<uint32_t>(ret("tlo")));
    return d;
}

// The callee CHistory<int,144>::GetAverageOfLast (0x1001c064) answers the count it summed; the translation takes that count and the sum from
// GetSumOfLast, so this one fake stands for it (the PowerPC stub of 0x1001c064 records the same event and makes the same mean).
template <>
int32_t CHistory<int32_t, 144>::GetSumOfLast(double& sum, int32_t count) const {
    const int32_t index = static_cast<int32_t>((reinterpret_cast<const char*>(this) - gAnalyserBlock - 0x1f100) / 0x250);
    ev("GetAverageOfLast", {index, count});
    const bool three = index == 3;
    const int64_t n = static_cast<int32_t>(ret(three ? "n3" : "n5"));
    sum = static_cast<double>(static_cast<int64_t>(ret(three ? "sum3" : "sum5")) - 1000000);
    return static_cast<int32_t>(n);
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gWorldBlock},
        {"analyser", gAnalyserBlock},
        {"bank", gBankBlock},
        {"word4", &Obj()->mWord4},
        {"mb", &gMainBalance.mValues[(0x2a8 - 0x8) / 4]},
        {"noride", &gAdvisorNoRideQueue},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The host address of the balance value at a Mac offset of the scoring object (4 is the halfword mWord4).
uint64_t tpw_adv_slot(int32_t macOffset) {
    if (macOffset == 4) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&Obj()->mWord4));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&Obj()->mBalance.mValues[SlotOf(macOffset)]));
}

// Clears the scoring object and points gGameWorld at the world block.
void tpw_adv_reset() {
    std::memset(gObjBlock, 0, sizeof gObjBlock);
    gMainBalance.mValues[(0x2a8 - 0x8) / 4].mBits = 0;
    gAdvisorNoRideQueue = 0;
    gGameWorld = reinterpret_cast<CWorld*>(gWorldBlock);
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&Obj()->mWord4) - gObjBlock);
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100));
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int32_t, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    return n;
}

// ---- the functions, one wrapper each
#define TPW_ADV_WRAP(NAME) \
    uint32_t tpw_adv_##NAME() { return static_cast<uint32_t>(Obj()->_ADVISOR_##NAME()); }
TPW_ADV_WRAP(WEALTH_HIGH_REPAY_LOAN)
TPW_ADV_WRAP(PRICING_TICKET_TOO_CHEAP)
TPW_ADV_WRAP(WAGES_HIGH)
TPW_ADV_WRAP(PRICING_TICKET_TOO_EXPENSIVE)
TPW_ADV_WRAP(LOTS_MECHANICS)
TPW_ADV_WRAP(LOTS_SECURITY)
TPW_ADV_WRAP(LOTS_RESEARCHERS)
TPW_ADV_WRAP(QUEUE_UNCONNECTED)
TPW_ADV_WRAP(MAX_HANDYMEN)
TPW_ADV_WRAP(FEATURES_NOT_ENOUGH_TOILETS)
TPW_ADV_WRAP(FEATURES_NO_TOILETS)
TPW_ADV_WRAP(FEATURES_BUILD_FEATURES)

}  // extern "C"
