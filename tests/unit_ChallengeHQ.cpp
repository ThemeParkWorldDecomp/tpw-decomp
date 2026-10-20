// Test library for the challenge hall's progress (unit w175; libtpw_unit_ChallengeHQ.dylib, tests/test_ChallengeHQ.py), main app:
//   src/ChallengeHQ_progress.cpp   CChallengeHQ::FinalDestroy, Serialise, AddToCounter, GetCurrentChallenge, ReceiveMessage, OfferFollowup,
//                                  TellPlayerChallengeOffered, ChallengeAccepted, ChallengeDeclined, EndCurrentChallenge, WonCurrentChallenge and
//                                  LostCurrentChallenge
//   src/ThingMessages.cpp          the CMsgChallenge constructor (built for real by the hall)
//
// Every callee outside those is a fake that records the same event as the module's PowerPC stub, in the same order: CThing::Serialise, CChallenge::Serialise,
// DidThisWinIt, GetOfferId / GetWinId / GetLoseId, LOADVAR / SAVEVAR of int and bool and the VARSTRUCT of the time stamp and the thing id, GetFunnyTime, the
// time stamp's operator+, CWorld::GetParkAnalyser and GetBankAccount, the analyser's three queries, MakeDeposit, CMessageCentre::PostMessage and its nothrow new
// and header construction, the trace and the assert. The worlds are only addresses (never dereferenced); the hall, the file, the analyser, the bank and the
// message are blocks the test allocates. The globals of other files are defined here.
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
#include "ParkMessages.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (zero at start like the Mac's bss)
CWorld* gWorld;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CMessageCentre* gMessageCentre;

namespace {

using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

const char* gHall;            // the hall under test: the log's offsets and challenge indices are from it
void* gAnalyser;              // CWorld::GetParkAnalyser answers it
void* gBank;                  // CWorld::GetBankAccount answers it
void* gNewCentre;             // the block the fake nothrow new made
void* gAllocated[8];
int32_t gAllocatedCount;
alignas(16) unsigned char gMsgBuf[0x80];

int64_t Off(const void* inside) { return static_cast<int64_t>(reinterpret_cast<const char*>(inside) - gHall); }
// The index of a challenge row of the hall (or the raw offset when the pointer is no row).
int64_t Row(const void* challenge) { return (reinterpret_cast<const char*>(challenge) - gHall - 0xc) / 0x30; }

// A message of a chosen type over a real message layout: the research and add-to-challenge messages are the unit's own classes, anything else has the base
// and a word.
template <class M>
struct Typed : M {
    int32_t mWantedType = 0;
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return mWantedType;
    }
};
struct EventShape : CMsg {
    int32_t mEvent = 0;
    int32_t mWantedType = 0;
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return mWantedType;
    }
};

template <class V>
int32_t Fake(const char* kind, bool load, void* file, V* value, const char* text, int32_t noLog) {
    ev(kind, {lab(file), Off(value), noLog}, {text});
    const int64_t answer = ret("ans", 0);
    const int64_t write = ret("wr", 1);
    const int64_t stored = ret("val", 0);
    if (load && write != 0) {
        const uint64_t bits = static_cast<uint64_t>(stored);
        std::memcpy(static_cast<void*>(value), &bits, sizeof(V));
    }
    return static_cast<int32_t>(answer);
}

}  // namespace

// ---- the balance table's construction and destruction (the values are written by the test)
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}

// ---- the base thing and the challenges
int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("ThingSerialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("thing", 0));
}
int32_t CChallenge::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("ChalSerialise", {Row(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("chal", 0));
}
bool CChallenge::DidThisWinIt(int32_t& value, bool atEnd) {
    ev("DidWin", {Row(this), Off(&value), value, atEnd});
    if (ret("winmut", 0) != 0) value = static_cast<int32_t>(ret("winval"));
    return (ret("won") & 0xff) != 0;
}
int32_t CChallenge::GetOfferId() const {
    ev("GetOfferId", {Row(this)});
    return static_cast<int32_t>(ret("oid"));
}
int32_t CChallenge::GetWinId() const {
    ev("GetWinId", {Row(this)});
    return static_cast<int32_t>(ret("wid"));
}
int32_t CChallenge::GetLoseId() const {
    ev("GetLoseId", {Row(this)});
    return static_cast<int32_t>(ret("lid"));
}

// ---- the save-file templates
template <> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Fake("LOADVAR<int>", true, f, v, n, l); }
template <> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Fake("SAVEVAR<int>", false, f, v, n, l); }
template <> int32_t LOADVAR<bool>(void* f, bool* v, const char* n, int32_t l) { return Fake("LOADVAR<bool>", true, f, v, n, l); }
template <> int32_t SAVEVAR<bool>(void* f, bool* v, const char* n, int32_t l) { return Fake("SAVEVAR<bool>", false, f, v, n, l); }
template <> int32_t LOADVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) { return Fake("LOADVARSTRUCT<TbTimeStamp>", true, f, v, n, l); }
template <> int32_t SAVEVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) { return Fake("SAVEVARSTRUCT<TbTimeStamp>", false, f, v, n, l); }
template <> int32_t LOADVARSTRUCT<ThingId>(void* f, ThingId* v, const char* n, int32_t l) { return Fake("LOADVARSTRUCT<ThingId>", true, f, v, n, l); }
template <> int32_t SAVEVARSTRUCT<ThingId>(void* f, ThingId* v, const char* n, int32_t l) { return Fake("SAVEVARSTRUCT<ThingId>", false, f, v, n, l); }

// ---- the clock
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), lab(a), lab(b), lab(c), lab(d), lab(e), lab(f), lab(g)}, {});
    ev("GetFunnyTime.8", {lab(h)});
    return TbTimeStamp{ret("ft")};
}
TbTimeStamp TbTimeStamp::operator+(const TbTimeDiff& diff) const {
    ev("Stamp+", {lab(this), mTicks, diff.mTicks});
    return TbTimeStamp{ret("plus")};
}

// ---- the world, the analyser, the bank
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(gAnalyser);
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBank", {lab(this)});
    return static_cast<CBankAccount*>(gBank);
}
int32_t CParkAnalyser::AvgFilthLevelOfToilets() const {
    ev("Filth", {lab(this)});
    return static_cast<int32_t>(ret("filth"));
}
int32_t CParkAnalyser::AverageHappinessOfStaff() const {
    ev("Happy", {lab(this)});
    return static_cast<int32_t>(ret("happy"));
}
int32_t CParkAnalyser::AverageHappinessOfStaffType(CThing::ThingModel model) const {
    ev("HappyType", {lab(this), static_cast<int32_t>(model)});
    return static_cast<int32_t>(ret("happyt"));
}
void CBankAccount::MakeDeposit(uint32_t amount) { ev("MakeDeposit", {lab(this), amount}); }

// ---- the trace and the assert
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t arg = 0;
    if (std::strstr(format, "%d") != nullptr) {
        va_list list;
        va_start(list, format);
        arg = va_arg(list, int);
        va_end(list);
    }
    ev("CTRACE", {level, arg}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// ---- the message centre: PostMessage logs the challenge message; the nothrow new and the CSubscriptionHeader constructor stand for the Mac's operator new and
// __construct_array (the first header of the new block reports the construction).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    const auto& m = static_cast<const CMsgChallenge&>(message);
    ev("PostMessage", {lab(this), type, message.mFlags, m.mTagId, m.mMessage, m.mFlag, m.mPrize, m.mTargetTime, m.mTargetVal, m.mDays, m.mValue, m.mObj});
    ev("PostMessage.2", {m.mObj2, m.mStaff});
    return static_cast<int32_t>(ret("post"));
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys): only the message buffer.
uint64_t tpw_chq_msg_addr() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMsgBuf)); }

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_chq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_chq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CChallengeHQ));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mChallenges));
    out[n++] = static_cast<int64_t>(sizeof(CChallenge));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mCurrentChallenge));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mNextChallengeEventTime));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mSystemSwitchedOn));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mChallengeOn));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mWaitingForReply));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mValue));
    out[n++] = static_cast<int64_t>(offsetof(CChallengeHQ, mActualThing));
    out[n++] = static_cast<int64_t>(sizeof(TbTimeStamp));
    out[n++] = static_cast<int64_t>(sizeof(ThingId));
    return n;
}

// The globals before a case: the hall and the worlds (addresses), the analyser, the bank and the centre block (0 for none); the allocations of the last
// case are freed.
void tpw_chq_prepare(uint64_t hall, uint64_t world, uint64_t game, uint64_t analyser, uint64_t bank, uint64_t centre) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gHall = reinterpret_cast<const char*>(hall);
    gWorld = reinterpret_cast<CWorld*>(world);
    gGameWorld = reinterpret_cast<CWorld*>(game);
    gAnalyser = reinterpret_cast<void*>(analyser);
    gBank = reinterpret_cast<void*>(bank);
    gMessageCentre = reinterpret_cast<CMessageCentre*>(centre);
}
void tpw_chq_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
// What is left after a call: the label of the centre global and the allocations.
void tpw_chq_globals(int64_t* out) {
    out[0] = lab(gMessageCentre);
    out[1] = gAllocatedCount;
}

// ---- the functions, one wrapper each (every argument a word, the casts inside)
static CChallengeHQ* Hq(uint64_t hall) { return reinterpret_cast<CChallengeHQ*>(hall); }

uint64_t tpw_chq_final(uint64_t hall) {
    Hq(hall)->FinalDestroy();
    return 0;
}
uint64_t tpw_chq_serialise(uint64_t hall, uint64_t file, uint64_t save, uint64_t version, uint64_t unused) {
    return static_cast<uint32_t>(Hq(hall)->Serialise(reinterpret_cast<void*>(file), static_cast<int32_t>(save), static_cast<int32_t>(version),
                                                     static_cast<uint32_t>(unused)));
}
uint64_t tpw_chq_add(uint64_t hall, uint64_t type, uint64_t amount, uint64_t obj, uint64_t thing) {
    Hq(hall)->AddToCounter(static_cast<ChallengeType>(static_cast<int32_t>(type)), static_cast<int32_t>(amount), ObjectId{static_cast<uint16_t>(obj)},
                           ThingId{static_cast<uint16_t>(thing)});
    return 0;
}
uint64_t tpw_chq_current(uint64_t hall) { return reinterpret_cast<uint64_t>(Hq(hall)->GetCurrentChallenge()); }
// f1 .. f4 are the message's words: the research message's id and category, the add message's type, value, id and thing.
uint64_t tpw_chq_receive(uint64_t hall, uint64_t wanted, uint64_t flags, uint64_t f1, uint64_t f2, uint64_t f3, uint64_t f4) {
    std::memset(gMsgBuf, 0xa5, sizeof gMsgBuf);
    if (wanted == 0x11) {
        auto* m = new (gMsgBuf) Typed<CMsgResearchCompleted>;
        m->mWantedType = 0x11;
        m->mFlags = static_cast<uint32_t>(flags);
        m->mObj = static_cast<uint16_t>(f1);
        m->mCategory = static_cast<ResearchCat>(static_cast<int32_t>(f2));
        Hq(hall)->ReceiveMessage(*m);
    } else if (wanted == 0x1a) {
        auto* m = new (gMsgBuf) Typed<CMsgAddToChallenge>;
        m->mWantedType = 0x1a;
        m->mFlags = static_cast<uint32_t>(flags);
        m->mType = static_cast<ChallengeType>(static_cast<int32_t>(f1));
        m->mValue = static_cast<int32_t>(f2);
        m->mObj = static_cast<uint16_t>(f3);
        m->mThing = static_cast<uint16_t>(f4);
        Hq(hall)->ReceiveMessage(*m);
    } else {
        auto* m = new (gMsgBuf) EventShape;
        m->mWantedType = static_cast<int32_t>(wanted);
        m->mFlags = static_cast<uint32_t>(flags);
        Hq(hall)->ReceiveMessage(*m);
    }
    return 0;
}
uint64_t tpw_chq_offer(uint64_t hall) { return Hq(hall)->OfferFollowup() ? 1 : 0; }
uint64_t tpw_chq_tell(uint64_t hall, uint64_t challenge, uint64_t force) {
    return Hq(hall)->TellPlayerChallengeOffered(reinterpret_cast<CChallenge*>(challenge), static_cast<uint8_t>(force) != 0) ? 1 : 0;
}
uint64_t tpw_chq_accept(uint64_t hall) {
    Hq(hall)->ChallengeAccepted();
    return 0;
}
uint64_t tpw_chq_decline(uint64_t hall) {
    Hq(hall)->ChallengeDeclined();
    return 0;
}
uint64_t tpw_chq_end(uint64_t hall) {
    Hq(hall)->EndCurrentChallenge();
    return 0;
}
uint64_t tpw_chq_won(uint64_t hall) {
    Hq(hall)->WonCurrentChallenge();
    return 0;
}
uint64_t tpw_chq_lost(uint64_t hall) {
    Hq(hall)->LostCurrentChallenge();
    return 0;
}

}  // extern "C"
