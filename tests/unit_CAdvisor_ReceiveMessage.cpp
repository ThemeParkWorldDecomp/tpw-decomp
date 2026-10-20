// Test library for the advisor's message receiver (unit v5631; libtpw_unit_CAdvisor_ReceiveMessage.dylib,
// tests/test_CAdvisor_ReceiveMessage.py), main app:
//   src/AdvisorQueue_CAdvisor_ReceiveMessage.cpp   CAdvisor::ReceiveMessage (0x1000c874)
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: the request constructor
// (it also fills the request from the queues), TheScoring (the original's inline guard: on a clear guard byte it records the scoring's
// constructor and __register_global_object, as the PowerPC side does), GetMessageCategory, CanIQueueThis, FindLowestScoringSlotInQueue,
// HandleEvent, HandleStaffInfo, CTRACE, TRACE and GAME_ASSERTMSG. The message classes' out-of-line members are fakes too, and the test's
// messages are subclasses whose GetType answers the case's type. gThingArray is defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "MsgAdvisor.h"
#include "ParkMessages.h"
#include "ThingMessages.h"
#include "ThingMessages2.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

uint8_t gScoringGuard;                       // the Mac's guard byte 0x102b4100
alignas(16) uint8_t gScoringStore[sizeof(CAdvisorScoring)];   // the Mac's object 0x102b3ac8 (never constructed on the host)
uint32_t gScoringDtor;                       // stands for the destructor's TVector 0x102657e0
uint32_t gScoringNode[4];                    // stands for the registration node 0x102b4104
int32_t gTestType;                           // what the test messages' GetType answers
void* gMessageStore;                         // the test message's block

int64_t I(int64_t v) { return v; }

void FillRequest(CAdvisorRequest* request, const uint32_t* w) {
    request->mThingId = static_cast<uint16_t>(w[0] >> 16);
    request->mPad02[0] = static_cast<uint8_t>(w[0] >> 8);
    request->mPad02[1] = static_cast<uint8_t>(w[0]);
    request->mSecondary = w[1];
    request->mResponseNo = static_cast<int32_t>(w[2]);
    request->mMessage = static_cast<AdvisorMessage>(static_cast<int32_t>(w[3]));
    request->mScore = static_cast<int32_t>(w[4]);
    request->mValid = static_cast<uint8_t>(w[5] >> 24);
    request->mOverrideOnlyOnce = static_cast<uint8_t>(w[5] >> 16);
    request->mScoreOverridden = static_cast<uint8_t>(w[5] >> 8);
    request->mPad17 = static_cast<uint8_t>(w[5]);
}

// ---- the test messages: each class's GetType answers the case's type
struct TAdvisor : CMsgAdvisor {
    TAdvisor() : CMsgAdvisor(CAdvisorRequest()) {}
    int32_t GetType() const override { return gTestType; }
};
struct TCondemned : CMsgRideCondemned {
    TCondemned() : CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}) {}
    int32_t GetType() const override { return gTestType; }
};
struct TAdvice : CMsgThingAdvice {
    int32_t GetType() const override { return gTestType; }
};
struct TResearch : CMsgResearchCompleted {
    int32_t GetType() const override { return gTestType; }
};
struct TEvent : CMsgEvent {
    TEvent() : CMsgEvent(static_cast<EventType>(0)) {}
    int32_t GetType() const override { return gTestType; }
};
struct TStaff : CMsgStaffInfo {
    int32_t GetType() const override { return gTestType; }
};
struct TChallenge : CMsgChallenge {
    int32_t GetType() const override { return gTestType; }
};
struct TOther : CMsg {
    int32_t GetType() const override { return gTestType; }
    uint32_t mWords[10];
};

void* MessageStore() {
    if (gMessageStore == nullptr) gMessageStore = tpw::ArenaAllocZeroed(0x100);
    return gMessageStore;
}

// Builds the case's message from its Mac words (+8 on) in the store.
const CMsg* MakeMessage(int32_t kind, const uint32_t* w) {
    void* p = MessageStore();
    std::memset(p, 0, 0x100);
    switch (kind) {
    case 0: {
        TAdvisor* m = new (p) TAdvisor();
        FillRequest(&m->mRequest, w);
        return m;
    }
    case 1: {
        TCondemned* m = new (p) TCondemned();
        m->mThingId = static_cast<uint16_t>(w[0] >> 16);
        return m;
    }
    case 2: {
        TAdvice* m = new (p) TAdvice();
        m->mFlags = 0;
        m->mThingId = static_cast<uint16_t>(w[0] >> 16);
        m->mKind = static_cast<int32_t>(w[1]);
        return m;
    }
    case 3: {
        TResearch* m = new (p) TResearch();
        m->mFlags = 0;
        m->mObj = static_cast<uint16_t>(w[0] >> 16);
        m->mCategory = static_cast<ResearchCat>(static_cast<int32_t>(w[1]));
        return m;
    }
    case 4: {
        TEvent* m = new (p) TEvent();
        m->mEvent = static_cast<int32_t>(w[0]);
        return m;
    }
    case 5: {
        TStaff* m = new (p) TStaff();
        m->mFlags = 0;
        m->mStaff = static_cast<StaffType>(static_cast<int32_t>(w[0]));
        m->mSituation = static_cast<StaffSituation>(static_cast<int32_t>(w[1]));
        return m;
    }
    case 6: {
        TChallenge* m = new (p) TChallenge();
        m->mFlags = 0;
        m->mTagId = static_cast<int32_t>(w[0]);
        m->mMessage = static_cast<AdvisorMessage>(static_cast<int32_t>(w[1]));
        return m;
    }
    default: {
        TOther* m = new (p) TOther();
        m->mFlags = 0;
        for (int i = 0; i < 10; i++) m->mWords[i] = w[i];
        return m;
    }
    }
}

}  // namespace

// ---- the message classes' out-of-line members (their files are not in this library)
CMsgAdvisor::CMsgAdvisor(const CAdvisorRequest& request) {
    mFlags = 0;
    mRequest = request;
}
int32_t CMsgAdvisor::GetType() const { return 5; }
CMsgAdvisor::~CMsgAdvisor() {}
CMsgRideCondemned::CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mFlags = 0;
    mThingId = id.mIndex;
}
int32_t CMsgRideCondemned::GetType() const { return 8; }
CMsgRideCondemned::~CMsgRideCondemned() {}
int32_t CMsgResearchCompleted::GetType() const { return 0x11; }
CMsgResearchCompleted::~CMsgResearchCompleted() {}
CMsgEvent::CMsgEvent(EventType type) {
    mFlags = 0;
    mEvent = type;
}
int32_t CMsgEvent::GetType() const { return 0x13; }
CMsgEvent::~CMsgEvent() {}
int32_t CMsgStaffInfo::GetType() const { return 0x17; }
CMsgStaffInfo::~CMsgStaffInfo() {}
int32_t CMsgChallenge::GetType() const { return 0x19; }
CMsgChallenge::~CMsgChallenge() {}

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("CAdvisorRequest", {lab(this), I(message), I(thingId.mIndex), I(secondary), I(responseNo), I(overrideOnlyOnce ? 1 : 0), I(score)});
    const uint32_t bits = static_cast<uint32_t>(ret("ReqBits"));
    const uint32_t w[6] = {(static_cast<uint32_t>(thingId.mIndex) << 16) | 0x5a00u | (bits >> 24), secondary, static_cast<uint32_t>(responseNo),
                           static_cast<uint32_t>(message), static_cast<uint32_t>(ret("ReqScore")),
                           ((bits & 0xffu) << 24) | ((overrideOnlyOnce ? 1u : 0u) << 16) | (((bits >> 8) & 0xffu) << 8) | ((bits >> 16) & 0xffu)};
    FillRequest(this, w);
}

CAdvisorScoring& TheScoring() {
    if (gScoringGuard == 0) {
        ev("CAdvisorScoring", {lab(gScoringStore)});
        ev("__register_global_object", {lab(gScoringStore), lab(&gScoringDtor), lab(gScoringNode)});
        gScoringGuard = 1;
    }
    return *reinterpret_cast<CAdvisorScoring*>(gScoringStore);
}

int32_t CAdvisorScoring::GetMessageCategory(AdvisorMessage message) const {
    ev("GetMessageCategory", {lab(this), I(message)});
    return static_cast<int32_t>(ret("GetMessageCategory"));
}

bool CAdvisor::CanIQueueThis(AdvisorMessage message, bool force) const {
    ev("CanIQueueThis", {lab(this), I(message), I(force ? 1 : 0)});
    return (ret("CanIQueueThis") & 0xff) != 0;
}

int32_t CAdvisor::FindLowestScoringSlotInQueue() {
    ev("FindLowestScoringSlotInQueue", {lab(this)});
    return static_cast<int32_t>(ret("FindLowestScoringSlotInQueue"));
}

void CAdvisor::HandleEvent(EventType event) { ev("HandleEvent", {lab(this), I(static_cast<int32_t>(event))}); }

void CAdvisor::HandleStaffInfo(const CMsgStaffInfo& info) { ev("HandleStaffInfo", {lab(this), lab(&info)}); }

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {I(level)}, {format}); }

void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("TRACE", {I(value)}, {format});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {I(condition)}, {format}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_rm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"guard", &gScoringGuard}, {"scoring", gScoringStore}, {"dtor", &gScoringDtor},
        {"node", gScoringNode},    {"msg", MessageStore()},     {"things", gThingArray},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable).
uint64_t tpw_rm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on.
int32_t tpw_rm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CAdvisor));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mQueue));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mLastTutorialRequest));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorRequest));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mValid));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    return n;
}

// Builds the message (`kind` picks the class, `type` is its GetType answer, `words` its Mac words from +8) and lets the advisor receive it.
void tpw_rm_receive(uint64_t advisor, int32_t kind, int32_t type, const uint32_t* words) {
    gTestType = type;
    const CMsg* message = MakeMessage(kind, words);
    reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(advisor))->ReceiveMessage(*message);
}

}  // extern "C"
