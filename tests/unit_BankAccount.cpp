// Test library for unit w173 (libtpw_unit_BankAccount.dylib, tests/test_BankAccount.py), main app: src/game/park/BankAccount.cpp (CBankAccount::Serialise 0x100f2d10,
// MonthlyUpdate 0x100f36a8, GetLoanInfo 0x100f3c44, AnyLoanAvailable 0x100f3ee0, PayOffLoan 0x100f40c8) and CBankAccount::MakeWithdrawal 0x100f33b0
// (src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/Weather.cpp, src/game/park/analyser.cpp, real). Every other callee is a fake that records the same event as the PowerPC side's stub.
//
// The worlds are not blocks of their Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and a world pointer is that block's
// address minus 0x1da6f0 on both sides (mParkTime's address is only passed on). The account lives in gObj over 0x40 guard bytes on each side; the two
// analysers are blocks that reach their words +0x1f5a0 and +0x1fc90 over guards, so a stray store shows.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Serialise.h"

// The uint32_t instances are specialised below; a header included later instantiates them inline, so they are declared first.
template <> int32_t LOADVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l);
template <> int32_t SAVEVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l);
template <> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l);
template <> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l);

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Clock.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TPSound.h"
#include "TbTimeDiff.h"
#include "ThingParts.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "tb_rain.h"

using namespace calllog;

// Only a place for the storage of the Mac's clock singleton (its symbol; the unit never constructs it).
alignas(16) unsigned char gClockBlock[sizeof(CClock)] __asm__("_gClock");

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kObjSize = 0x128;
constexpr int32_t kObjBlock = kGuard + kObjSize + kGuard;
constexpr int32_t kAnalyserBlock = kGuard + 0x1fc94 + kGuard;
constexpr int32_t kOutBlock = 0x20 + 0x20 + 0x20;

alignas(16) unsigned char gTail[2][kTailSize];
alignas(16) unsigned char gObj[kObjBlock];
alignas(16) unsigned char gAnalyser[2][kAnalyserBlock];
alignas(16) unsigned char gCentre[0x1d0];
alignas(16) unsigned char gOut[kOutBlock];
void* gNewCentre;
void* gAllocated[4];
int32_t gAllocatedCount;
const char* gBase;       // the account under test (offsets in the Serialise log are from it)
int32_t gCount;          // Serialise fakes called so far in this case
int32_t gFail = -1;      // the number of the Serialise fake that fails (-1: none)
int32_t gFailValue;      // what the failing fake answers

CWorld* WorldPtr(int32_t k) { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail[k]) - kTail); }
CParkAnalyser* AnalyserPtr(int32_t k) { return reinterpret_cast<CParkAnalyser*>(gAnalyser[k] + kGuard); }

// The answer of the next Serialise fake: the case's failValue for call number gFail, else 0.
int32_t Answer() {
    const int32_t number = gCount++;
    return number == gFail ? gFailValue : 0;
}

// One VAR call of the original (file, value, name, noLog).
int32_t Var(const char* kind, void* file, const void* value, const char* text, int32_t noLog) {
    ev(kind, {lab(file), static_cast<int64_t>(reinterpret_cast<const char*>(value) - gBase), noLog}, {text});
    return Answer();
}

}  // namespace

// ---- The unit's globals (the rest of Things2_small.cpp's, never reached) ----------------------------------------------------------------
CWorld* gWorld;
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
CRain* gei_Rain;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];

// ---- The Serialise fakes: CThing::Serialise 0x1012e010 and the six LOADVAR / SAVEVAR instances ---------------------------------------------
int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CThing::Serialise", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - gBase), lab(file), save, version, unused}, {});
    return Answer();
}
template <> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Var("LOADVAR<int>", f, v, n, l); }
template <> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Var("SAVEVAR<int>", f, v, n, l); }
template <> int32_t LOADVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Var("LOADVAR<unsigned int>", f, v, n, l); }
template <> int32_t SAVEVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Var("SAVEVAR<unsigned int>", f, v, n, l); }
template <> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Var("LOADVAR<unsigned long>", f, v, n, l); }
template <> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Var("SAVEVAR<unsigned long>", f, v, n, l); }

// ---- The asserts and traces ------------------------------------------------------------------------------------------------------------
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The trace records its level and as many ints as its text has "%d" (up to three).
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t values[3] = {0, 0, 0};
    int32_t count = 0;
    for (const char* p = format; p != nullptr && (p = std::strstr(p, "%d")) != nullptr; p += 2) ++count;
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < count && i < 3; ++i) values[i] = va_arg(args, int);
    va_end(args);
    switch (count) {
    case 0: ev("CTRACE", {static_cast<int64_t>(level)}, {format}); break;
    case 1: ev("CTRACE", {static_cast<int64_t>(level), values[0]}, {format}); break;
    case 2: ev("CTRACE", {static_cast<int64_t>(level), values[0], values[1]}, {format}); break;
    default: ev("CTRACE", {static_cast<int64_t>(level), values[0], values[1], values[2]}, {format}); break;
    }
}

// ---- The worlds, the analyser, the time ------------------------------------------------------------------------------------------------
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return AnalyserPtr(this == WorldPtr(0) ? 0 : 1);
}
int32_t CParkAnalyser::GetParkValue(uint32_t seconds) const {
    ev("GetParkValue", {lab(this), seconds});
    return static_cast<int32_t>(ret("pv", 0));
}
TbTimeDiff CParkTime::GetFunnyTimeSinceGameTurn(uint32_t turn) const {
    ev("GetFunnyTimeSinceGameTurn", {lab(this), turn});
    TbTimeDiff span;
    span.mTicks = ret("span", 0);
    return span;
}

// ---- The event message and the lazy message centre -------------------------------------------------------------------------------------
CMsgEvent::CMsgEvent(EventType type) : mEvent(type) { ev("CMsgEvent", {newlab(this), type}); }
CMsgEvent::~CMsgEvent() {}
int32_t CMsgEvent::GetType() const { return 0; }
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), lab(&message), static_cast<const CMsgEvent&>(message).mEvent});
    return 1;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    if (gAllocatedCount < 4) gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- Never reached: the other functions of Things2_small.cpp ---------------------------------------------------------------------------
CWorld& CWorldSavvy::GetWorld() const { std::abort(); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const { std::abort(); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing*) const { std::abort(); }
TbTimeStamp CParkTime::GetFunnyTime(int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*, int32_t*) const { std::abort(); }
int32_t CClock::ReadFixed() { std::abort(); }
void CRain::SetRainType(int32_t) { std::abort(); }
void CRain::SetRaindrops(int32_t) { std::abort(); }
int32_t SOUND_SendEvent(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) { std::abort(); }
void SOUND_FadeSoundEffect(int32_t) { std::abort(); }

extern "C" {

// The blocks the test fills and reads: 0 the account (over guards), 1 and 2 the world tails, 3 and 4 the analysers, 5 the message centre, 6 the
// hidden CLoan result (over guards).
void* tpw_bnk_addr(int32_t which) {
    switch (which) {
    case 0: return gObj;
    case 1: return gTail[0];
    case 2: return gTail[1];
    case 3: return gAnalyser[0];
    case 4: return gAnalyser[1];
    case 5: return gCentre;
    case 6: return gOut;
    default: return nullptr;
    }
}
void* tpw_bnk_world(int32_t k) { return WorldPtr(k); }

// The globals before a case: the worlds named by index, the centre (0 for none), and the Serialise fakes' failing call and answer.
void tpw_bnk_prepare(int32_t world, int32_t game, int32_t centre, int32_t fail, int32_t failValue) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gWorld = WorldPtr(world);
    gGameWorld = WorldPtr(game);
    gMessageCentre = centre != 0 ? reinterpret_cast<CMessageCentre*>(gCentre) : nullptr;
    gBase = reinterpret_cast<const char*>(gObj) + kGuard;
    gCount = 0;
    gFail = fail;
    gFailValue = failValue;
}
// The message centre pointer after the call (the test turns it into a label).
int64_t tpw_bnk_centre() { return lab(gMessageCentre); }

// The offsets and sizes the module relies on (HOST_LAYOUT): sizeof(CBankAccount), mLoans, mWithdrawalsEnabled, mProfitThisYear, sizeof(CLoan).
int32_t tpw_bnk_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CBankAccount));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mLoans));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mWithdrawalsEnabled));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mProfitThisYear));
    out[n++] = static_cast<int64_t>(sizeof(CLoan));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100) + 2 * sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100) + 5 * sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    return n;
}

static CBankAccount* Account() { return reinterpret_cast<CBankAccount*>(gObj + kGuard); }

// The functions under test; the file is the label address 0x1000.
int32_t tpw_bnk_serialise(int32_t save, int32_t version, uint32_t unused) {
    return Account()->Serialise(reinterpret_cast<void*>(0x1000), save, version, unused);
}
void tpw_bnk_monthly() { Account()->MonthlyUpdate(); }
// The CLoan comes back by value; it is copied to the middle of the out block.
void tpw_bnk_loaninfo(uint32_t loan) {
    const CLoan info = Account()->GetLoanInfo(loan);
    std::memcpy(gOut + 0x20, &info, sizeof info);
}
int32_t tpw_bnk_anyloan() { return Account()->AnyLoanAvailable() ? 1 : 0; }
int32_t tpw_bnk_payoff(uint32_t loan) { return Account()->PayOffLoan(loan) ? 1 : 0; }

}  // extern "C"
