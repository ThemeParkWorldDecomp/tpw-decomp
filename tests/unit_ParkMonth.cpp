// Test library for the park analyser's messages, month end, admissions, thoughts and cell save (unit w161; libtpw_unit_ParkMonth.dylib,
// tests/test_ParkMonth.py), main app:
//   src/game/park/analyser.cpp   CParkAnalyser::ReceiveMessage, PersonPaidAdmission, UpdateEndOfMonth, GetThreeMostDominantThoughts, FinalDestroy,
//                                CParkAnalyserCell::Serialise and the two start-up words of __sinit_analyser_cpp
//   src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/ui/MessageCentre.cpp         the constructors, GetType and destructors of CMsgAddToChallenge, CMsgResearchCompleted and CMsgStaffInfo, and
//                                CBankAccount::GetBalance
//
// Every callee outside those is a fake that records the same event as the module's PowerPC stub, in the same order: CThing::FinalDestroy, the asserts, the
// world accessor and the used-thing walk, CPeep::IsInsidePark, CParkTime::GetFunnyTime, CRideDatabase::GetRideInfo, the analyser's queries and
// SetLastOfObjectTypeResearched, the histories (SetAtEnd, SetFromTemp, GetAverageOfLast), CWorld::GetBankAccount, CMessageCentre::PostMessage and its
// constructor pieces, LOADVAR / SAVEVAR of int and the Serialise of the analyser value. The world is only an address (never dereferenced); the analyser,
// the bank, the ride record, the message and the things are blocks the test allocates. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "History.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

extern uint32_t gAnalyserWord1034C19C;
extern uint32_t gAnalyserWord1034C1A0;

namespace {

constexpr int32_t kRideBytes = 0x800;
constexpr int32_t kMacCentreBytes = 0x1d0;

const char* gBase;            // the object under test: the log's offsets are from it
CBankAccount* gBank;          // CWorld::GetBankAccount answers it
unsigned char* gRide;         // CRideDatabase::GetRideInfo answers it (mUIType at +0x7a8)
void* gNewCentre;             // the block the fake nothrow new made
int32_t gNewCount;
alignas(16) unsigned char gMsgBuf[0x80];
alignas(16) unsigned char gCentreBlock[kMacCentreBytes];
alignas(16) unsigned char gMsg[0x80];

int64_t Off(const void* inside) { return static_cast<int64_t>(reinterpret_cast<const char*>(inside) - gBase); }

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

// A message of a chosen type over a real message layout: the research and staff messages are the unit's own classes, anything else has the event
// message's layout (the base and the word at +8).
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
static_assert(offsetof(EventShape, mEvent) == offsetof(CMsgEvent, mEvent), "the event message's layout");

template <class V>
int32_t Fake(const char* kind, bool load, void* file, V* value, const char* text, int32_t noLog) {
    ev(kind, {lab(file), static_cast<int64_t>(reinterpret_cast<const char*>(value) - gBase), noLog}, {text});
    const int64_t answer = ret("ans", 0);
    const int64_t stored = ret("val", -1);
    if (load && stored != -1) {
        const uint64_t bits = static_cast<uint64_t>(stored);
        std::memcpy(static_cast<void*>(value), &bits, sizeof(V));
    }
    return static_cast<int32_t>(answer);
}

}  // namespace

// ---- the globals of other files (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
CMessageCentre* gMessageCentre;
CRideDatabase gRideDatabase;

// ---- the fakes
void CThing::FinalDestroy() { ev("FinalDestroy", {lab(this)}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld;
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

int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("inside", 0));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return gBank;
}

TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(this), reinterpret_cast<intptr_t>(a), reinterpret_cast<intptr_t>(b), reinterpret_cast<intptr_t>(c), reinterpret_cast<intptr_t>(d),
                        reinterpret_cast<intptr_t>(e), reinterpret_cast<intptr_t>(f), reinterpret_cast<intptr_t>(g), reinterpret_cast<intptr_t>(h)});
    TbTimeStamp stamp;
    stamp.mTicks = ret("ft", 0);
    return stamp;
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(gRide);
}

void CParkAnalyser::SetLastOfObjectTypeResearched(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("SetLast", {Off(this), static_cast<int64_t>(type), id.mIndex});
}
int32_t CParkAnalyser::GetVisitorsInPark(uint32_t arg) const {
    ev("GetVisitorsInPark", {Off(this), arg});
    return static_cast<int32_t>(ret("gvip", 0));
}
int32_t CParkAnalyser::GetRidesInPark(uint32_t seconds) const {
    ev("GetRidesInPark", {Off(this), seconds});
    return static_cast<int32_t>(ret("grip", 0));
}
float CParkAnalyser::GetHappinessLevel(uint32_t unused) const {
    ev("GetHappinessLevel", {Off(this), unused});
    const uint32_t bits = static_cast<uint32_t>(ret("hap", 0));
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}
int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("NumberOfVisitorsSinceBeginning", {Off(this)});
    return static_cast<int32_t>(ret("sinceb", 0));
}
int32_t CParkAnalyser::GetParkValue(uint32_t seconds) const {
    ev("GetParkValue", {Off(this), seconds});
    return static_cast<int32_t>(ret("pv", 0));
}
int32_t CParkAnalyser::GetOverallParkRating() const {
    ev("GetOverallParkRating", {Off(this)});
    return static_cast<int32_t>(ret("rating", 0));
}

template <class T, int N>
void CHistory<T, N>::SetAtEnd(T value) {
    if constexpr (sizeof(T) == 4 && static_cast<T>(0.5) != static_cast<T>(0)) ev("HistSetAtEndF", {Off(this), CanonBits(static_cast<float>(value))});
    else ev("HistSetAtEnd", {Off(this), static_cast<int64_t>(value)});
}
template <class T, int N>
void CHistory<T, N>::SetFromTemp() {
    ev("HistSetFromTemp", {Off(this), static_cast<int64_t>(mTemp)});
}
template <class T, int N>
void CHistory<T, N>::GetAverageOfLast(T& out, int32_t count) const {
    ev("HistAverage", {Off(this), count});
    out = static_cast<T>(ret("avg", 0));
}
template void CHistory<int, 144>::SetAtEnd(int);
template void CHistory<float, 144>::SetAtEnd(float);
template void CHistory<int, 144>::SetFromTemp();
template void CHistory<int, 50>::GetAverageOfLast(int&, int32_t) const;

template <class T>
int32_t CParkAnalyserVal<T>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev(sizeof(T) == 4 && static_cast<T>(0.5) != static_cast<T>(0) ? "ValSerialise<float>" : "ValSerialise<int>", {Off(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("ans", 0));
}
template int32_t CParkAnalyserVal<int>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CParkAnalyserVal<float>::Serialise(void*, int32_t, int32_t, uint32_t);

template <> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Fake("LOADVAR<int>", true, f, v, n, l); }
template <> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Fake("SAVEVAR<int>", false, f, v, n, l); }

// The message centre: PostMessage logs the admission message, the nothrow new and the CSubscriptionHeader constructor stand for the Mac's operator new and
// __construct_array (the first header of the new block reports the construction).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    const auto& m = static_cast<const CMsgAddToChallenge&>(message);
    ev("PostMessage", {lab(this), type, message.mFlags, static_cast<int64_t>(m.mType), m.mValue, m.mObj, m.mThing});
    return static_cast<int32_t>(ret("post", 0));
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    ++gNewCount;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

extern "C" {

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_pn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host address of a global by the module's name for it.
uint64_t tpw_pn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"rdb", &gRideDatabase},
        {"w19c", &gAnalyserWord1034C19C},
        {"w1a0", &gAnalyserWord1034C1A0},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The object under test (the log's offsets are from it).
void tpw_pn_base(uint64_t object) { gBase = reinterpret_cast<const char*>(static_cast<uintptr_t>(object)); }

// The world address (never dereferenced), the bank, the ride record, the centre (0: none) of the next case; the allocation count is reset.
void tpw_pn_set(uint64_t world, uint64_t bank, uint64_t ride, uint64_t centre) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gGameWorld = gWorld;
    gBank = reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(bank));
    gRide = reinterpret_cast<unsigned char*>(static_cast<uintptr_t>(ride));
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gNewCentre = nullptr;
    gNewCount = 0;
}
void tpw_pn_row(int32_t id, uint64_t thing) { gThingArray[id].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }
int64_t tpw_pn_centre_label() { return lab(gMessageCentre); }
int32_t tpw_pn_news() { return gNewCount; }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_pn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyser));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100));
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mMisbehavingKids));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLastStrike));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mParkLastOpened));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mMostPaidForTicket));
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyserCell));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mUIType));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(sizeof(TbTimeStamp));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(CBankAccount));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mBalance));
    return n;
}

// ---- the functions
// A message of the given type for CParkAnalyser::ReceiveMessage: 0x11 a research message (a = the object id), 0x17 a staff message (b = the situation),
// anything else the event shape (a = the event word).
void tpw_pn_receive(uint64_t analyser, int32_t type, int32_t a, int32_t b) {
    std::memset(gMsgBuf, 0xa5, sizeof gMsgBuf);
    CParkAnalyser* p = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser));
    if (type == 0x11) {
        auto* m = new (gMsgBuf) Typed<CMsgResearchCompleted>;
        m->mWantedType = type;
        m->mObj = static_cast<uint16_t>(a);
        p->ReceiveMessage(*m);
    } else if (type == 0x17) {
        auto* m = new (gMsgBuf) Typed<CMsgStaffInfo>;
        m->mWantedType = type;
        m->mSituation = static_cast<StaffSituation>(b);
        p->ReceiveMessage(*m);
    } else {
        auto* m = new (gMsgBuf) EventShape;
        m->mWantedType = type;
        m->mEvent = a;
        p->ReceiveMessage(*m);
    }
}
uint64_t tpw_pn_msg() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMsgBuf)); }
uint64_t tpw_pn_centre_block() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gCentreBlock)); }
void tpw_pn_paid(uint64_t analyser, int32_t price) { reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser))->PersonPaidAdmission(price); }
void tpw_pn_month(uint64_t analyser) { reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser))->UpdateEndOfMonth(); }
void tpw_pn_thoughts(uint64_t analyser, uint64_t out) {
    const CParkAnalyser* p = reinterpret_cast<const CParkAnalyser*>(static_cast<uintptr_t>(analyser));
    uint8_t* b = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(out));
    p->GetThreeMostDominantThoughts(b[0], b[4], b[8]);
}
void tpw_pn_final(uint64_t analyser) { reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser))->FinalDestroy(); }
int32_t tpw_pn_cell(uint64_t cell, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CParkAnalyserCell*>(static_cast<uintptr_t>(cell))->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
int32_t tpw_pn_balance(uint64_t bank) { return reinterpret_cast<const CBankAccount*>(static_cast<uintptr_t>(bank))->GetBalance(); }

// The messages. kind 0 AddToChallenge (args: type, value, object id, thing id), 1 ResearchCompleted (id, category), 2 StaffInfo (staff, situation).
// out of the constructor: the flags, the type, then the payload words in the Mac's order; out[13] = nothing outside the written fields changed.
void tpw_pn_ctor(int32_t kind, const int64_t* args, int64_t* out) {
    std::memset(gMsg, 0xa5, sizeof gMsg);
    unsigned char* written[16][2];
    int32_t nwritten = 0;
    auto note = [&](const void* p, size_t n) {
        written[nwritten][0] = reinterpret_cast<unsigned char*>(const_cast<void*>(p));
        written[nwritten][1] = reinterpret_cast<unsigned char*>(const_cast<void*>(p)) + n;
        ++nwritten;
    };
    CMsg* base;
    if (kind == 0) {
        auto* m = new (gMsg) CMsgAddToChallenge(static_cast<ChallengeType>(args[0]), static_cast<int32_t>(args[1]),
                                                tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(args[2])},
                                                tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(args[3])});
        base = m;
        note(&m->mFlags, 4); note(&m->mType, 4); note(&m->mValue, 4); note(&m->mObj, 2); note(&m->mThing, 2);
        out[2] = static_cast<int64_t>(m->mType); out[3] = m->mValue; out[4] = m->mObj; out[5] = m->mThing;
    } else if (kind == 1) {
        auto* m = new (gMsg) CMsgResearchCompleted(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(args[0])},
                                                   static_cast<ResearchCat>(args[1]));
        base = m;
        note(&m->mFlags, 4); note(&m->mObj, 2); note(&m->mCategory, 4);
        out[2] = m->mObj; out[3] = static_cast<int64_t>(m->mCategory);
    } else {
        auto* m = new (gMsg) CMsgStaffInfo(static_cast<StaffType>(args[0]), static_cast<StaffSituation>(args[1]));
        base = m;
        note(&m->mFlags, 4); note(&m->mStaff, 4); note(&m->mSituation, 4);
        out[2] = static_cast<int64_t>(m->mStaff); out[3] = static_cast<int64_t>(m->mSituation);
    }
    out[0] = base->mFlags;
    out[1] = base->GetType();
    bool intact = true;
    note(gMsg, sizeof(void*));   // the vtable
    for (size_t i = 0; i < sizeof gMsg; ++i) {
        bool member = false;
        for (int32_t k = 0; k < nwritten; ++k) member = member || (gMsg + i >= written[k][0] && gMsg + i < written[k][1]);
        if (!member && gMsg[i] != 0xa5) intact = false;
    }
    out[13] = intact ? 1 : 0;
}
int32_t tpw_pn_gettype(int32_t kind, uint32_t flags) {
    std::memset(gMsg, 0xa5, sizeof gMsg);
    CMsg* m;
    if (kind == 0) m = new (gMsg) CMsgAddToChallenge;
    else if (kind == 1) m = new (gMsg) CMsgResearchCompleted;
    else m = new (gMsg) CMsgStaffInfo;
    m->mFlags = flags;
    return m->GetType();
}
// The destructor with the Mac's delete flag: a delete-expression for a positive (signed 16 bit) flag, else an explicit call; out = the flags left behind.
void tpw_pn_dtor(int32_t kind, int32_t flag, int32_t present, uint32_t* out) {
    out[0] = 0;
    if (present == 0) return;
    if (static_cast<int16_t>(flag) > 0) {
        CMsg* m;
        if (kind == 0) m = new CMsgAddToChallenge;
        else if (kind == 1) m = new CMsgResearchCompleted;
        else m = new CMsgStaffInfo;
        --tpw_cl_heap[0];
        tpw_cl_label(3, m);
        delete m;
        return;
    }
    std::memset(gMsg, 0xa5, sizeof gMsg);
    if (kind == 0) { auto* w = new (gMsg) CMsgAddToChallenge; w->mFlags = 0x11223344; w->~CMsgAddToChallenge(); std::memcpy(&out[0], &w->mFlags, 4); }
    else if (kind == 1) { auto* w = new (gMsg) CMsgResearchCompleted; w->mFlags = 0x11223344; w->~CMsgResearchCompleted(); std::memcpy(&out[0], &w->mFlags, 4); }
    else { auto* w = new (gMsg) CMsgStaffInfo; w->mFlags = 0x11223344; w->~CMsgStaffInfo(); std::memcpy(&out[0], &w->mFlags, 4); }
}

}  // extern "C"
