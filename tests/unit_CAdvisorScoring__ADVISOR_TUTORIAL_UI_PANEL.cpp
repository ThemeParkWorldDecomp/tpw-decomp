// Test library for the advisor scoring unit v5767 (libtpw_unit_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.dylib,
// tests/test_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.py), main app:
//   src/game/advisor/AdvisorScoring.cpp   Load, Save, GetFirstResponseFor and three scorers
//
// The callees outside the unit are recording fakes (the same events as the module's PowerPC stubs). The globals of other files the
// unit reads (the world pointers, gMasterSave, gMainBalance, the message table) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "AdvisorScoring.h"
#include "FS_MasterSave.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/samsutils.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files
CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;
CFSGlobalMasterSave* gMasterSave = nullptr;
CMainBalance gMainBalance;
AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2];

// CMainBalance needs its table functions and the base Init (not compiled here).
void CBalanceTable::Init() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance::~CMainBalance() {}

// The fake constructor of CFSGlobalMasterSave builds and (on an exception) destroys its string members.
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}

namespace {

alignas(16) unsigned char gBankBlock[64];
alignas(16) unsigned char gAnalyserBlock[64];
alignas(16) unsigned char gMasterBlock[64];
void* gCurObj = nullptr;

}  // namespace

// ---- the fakes
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    const char* world = reinterpret_cast<const char*>(this) - offsetof(CWorld, mParkTime);
    ev("FunnyTime", {lab(world)});
    TbTimeDiff d;
    d.mTicks = ret("ticks");
    return d;
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gBankBlock);
}

int32_t CBankAccount::GetProfitThisYear() const {
    ev("GetProfit", {lab(this)});
    return static_cast<int32_t>(ret("profit"));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserBlock);
}

int32_t CParkAnalyser::GetShopWithWorstValue(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("WorstValue", {lab(this), static_cast<int64_t>(type)});
    id.mIndex = static_cast<uint16_t>(ret("worstid"));
    return static_cast<int32_t>(ret("worst"));
}

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("Ctor", {newlab(this)}); }

bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeLocal type) const {
    ev("HaveTicket", {lab(this), static_cast<int64_t>(type)});
    return ret("have") != 0;
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int64_t>(level), value}, {format});
}

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("Read", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu), length, buffer == gCurObj ? 1 : 0});
    uint32_t word = static_cast<uint32_t>(ret("readdata"));
    unsigned char bytes[4] = {static_cast<unsigned char>(word >> 24), static_cast<unsigned char>(word >> 16),
                              static_cast<unsigned char>(word >> 8), static_cast<unsigned char>(word)};
    std::memcpy(buffer, bytes, 4);
    *transferred = static_cast<uint32_t>(ret("xfer"));
    return TbError(0);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const unsigned char* b = static_cast<const unsigned char*>(buffer);
    ev("Write", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu), length, b[0], b[1], b[2], b[3]});
    *transferred = static_cast<uint32_t>(ret("xfer"));
    return TbError(0);
}

extern "C" {

uint64_t tpw_v5767_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"bank", gBankBlock},
        {"analyser", gAnalyserBlock},
        {"master", gMasterBlock},
        {"mbal", &gMainBalance},
        {"table", &gAdvisorMessageBlock[0]},
        {"none", &gAdvisorNoneDrinksPoorValue},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5767_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v5767_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorScoring, mWord4));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorScoring, mWord8));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorScoring, mBalance) + offsetof(CAdvisorBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorScoring));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(sizeof(AdvisorMessageRecord));
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageRecord, mMessage));
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageRecord, mFirstResponse));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    return n;
}

void tpw_v5767_set_world(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }
void tpw_v5767_set_master(uint64_t master) { gMasterSave = reinterpret_cast<CFSGlobalMasterSave*>(static_cast<uintptr_t>(master)); }
uint64_t tpw_v5767_get_master() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMasterSave)); }

uint32_t tpw_v5767_tutorial(uint64_t obj) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(obj)->_ADVISOR_TUTORIAL_UI_PANEL()); }
uint32_t tpw_v5767_gold(uint64_t obj) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(obj)->_ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT()); }
uint32_t tpw_v5767_drinks(uint64_t obj) { return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(obj)->_ADVISOR_PRICING_DRINKS_POOR_VALUE()); }

uint32_t tpw_v5767_load(uint64_t obj, uint64_t cell) {
    gCurObj = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    return static_cast<uint32_t>(reinterpret_cast<CAdvisorScoring*>(obj)->Load(*reinterpret_cast<void**>(static_cast<uintptr_t>(cell))));
}
uint32_t tpw_v5767_save(uint64_t obj, uint64_t cell) {
    gCurObj = reinterpret_cast<void*>(static_cast<uintptr_t>(obj));
    return static_cast<uint32_t>(
        reinterpret_cast<const CAdvisorScoring*>(obj)->Save(*reinterpret_cast<void**>(static_cast<uintptr_t>(cell))));
}
uint32_t tpw_v5767_first(uint64_t obj, int32_t message) {
    return static_cast<uint32_t>(reinterpret_cast<const CAdvisorScoring*>(obj)->GetFirstResponseFor(static_cast<AdvisorMessage>(message)));
}

}  // extern "C"
