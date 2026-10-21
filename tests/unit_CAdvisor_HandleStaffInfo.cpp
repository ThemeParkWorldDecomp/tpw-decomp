// Test library for CAdvisor::HandleStaffInfo and CAdvisor::Serialise (unit v5223; libtpw_unit_CAdvisor_HandleStaffInfo.dylib,
// tests/test_CAdvisor_HandleStaffInfo.py), main app:
//   src/game/advisor/Advisor.cpp
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues: CTRACE, the
// CAdvisorRequest constructor (fills the request from the queue "req"), CAdvisorScoring::GetMessageCategory, TheScoring (the lazy
// construction of the singleton with its guard byte: the constructor and __register_global_object events), CanIQueueThis,
// FindLowestScoringSlotInQueue, CThing::Serialise, CAdvisorRequest::Serialise and the SAVEVAR / LOADVAR instances.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Serialise.h"
// TimeSig.h (included by AdvisorQueue.h) uses these two inline, so they are declared first.
template<> int32_t SAVEVAR<MacULong>(void*, MacULong*, const char*, int32_t);
template<> int32_t LOADVAR<MacULong>(void*, MacULong*, const char*, int32_t);
#include "AdvisorQueue.h"
#include "Debug.h"
#include "ParkMessages.h"

using namespace calllog;

// ---- globals of other files, defined here
uint8_t gScoringGuard;                              // Mac 0x102b4100
alignas(16) static uint8_t gScoringObject[0x1000];  // Mac 0x102b3ac8
static uint32_t gScoringDtor[3];                    // Mac 0x102657e0
static uint32_t gScoringNode[3];                    // Mac 0x102b4104

namespace {
uint8_t* gBase;  // unused by the fakes: pointers are logged as labels
}

// ---- the fakes
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int64_t>(level), value}, {format});
}

CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("CAdvisorRequest", {static_cast<int64_t>(message), thingId.mIndex, secondary, responseNo, overrideOnlyOnce ? 1 : 0, score});
    uint32_t words[5];
    for (uint32_t& w : words) w = static_cast<uint32_t>(ret("req"));
    std::memcpy(this, words, 20);
    uint8_t* bytes = reinterpret_cast<uint8_t*>(this);
    for (int i = 0; i < 4; i++) bytes[0x14 + i] = static_cast<uint8_t>(ret("req"));
}

int32_t CAdvisorScoring::GetMessageCategory(AdvisorMessage message) const {
    ev("GetMessageCategory", {lab(this), static_cast<int64_t>(message)});
    return static_cast<int32_t>(ret("GetMessageCategory"));
}

CAdvisorScoring& TheScoring() {
    if (gScoringGuard == 0) {
        ev("ScoringCtor", {lab(gScoringObject)});
        ev("register", {lab(gScoringObject), lab(gScoringDtor), lab(gScoringNode)});
        gScoringGuard = 1;
    }
    return *reinterpret_cast<CAdvisorScoring*>(gScoringObject);
}

bool CAdvisor::CanIQueueThis(AdvisorMessage message, bool force) const {
    ev("CanIQueueThis", {lab(this), static_cast<int64_t>(message), force ? 1 : 0});
    return ret("CanIQueueThis") != 0;
}

int32_t CAdvisor::FindLowestScoringSlotInQueue() {
    ev("FindLowest", {lab(this)});
    return static_cast<int32_t>(ret("FindLowest"));
}

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CThing::Serialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CThing::Serialise"));
}

int32_t CAdvisorRequest::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CAdvisorRequest::Serialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CAdvisorRequest::Serialise"));
}

#define FAKE(KIND, NAME, TYPE)                                                                 \
    template<> int32_t KIND<TYPE>(void* file, TYPE* value, const char* name, int32_t noLog) { \
        ev(NAME, {lab(file), lab(value), noLog}, {name});                                      \
        return static_cast<int32_t>(ret(NAME));                                                \
    }
FAKE(SAVEVAR, "SAVEVAR<long>", MacLong)
FAKE(LOADVAR, "LOADVAR<long>", MacLong)
FAKE(SAVEVAR, "SAVEVAR<int>", int32_t)
FAKE(LOADVAR, "LOADVAR<int>", int32_t)
FAKE(SAVEVAR, "SAVEVAR<unsigned long>", MacULong)
FAKE(LOADVAR, "LOADVAR<unsigned long>", MacULong)
FAKE(SAVEVAR, "SAVEVAR<bool>", bool)
FAKE(LOADVAR, "LOADVAR<bool>", bool)
FAKE(SAVEVAR, "SAVEVAR<AdvisorMessage>", AdvisorMessage)
FAKE(LOADVAR, "LOADVAR<AdvisorMessage>", AdvisorMessage)
FAKE(SAVEVARSTRUCT, "SAVEVARSTRUCT<AdvisorHistory>", AdvisorMessageState)
FAKE(LOADVARSTRUCT, "LOADVARSTRUCT<AdvisorHistory>", AdvisorMessageState)

extern "C" {

uint64_t tpw_hsi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"guard", &gScoringGuard}, {"object", gScoringObject}, {"dtor", gScoringDtor}, {"node", gScoringNode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hsi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hsi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mQueue));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mMessageState));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mLastTutorialRequest));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mTypePlaying));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisor));
    out[n++] = static_cast<int64_t>(offsetof(CMsgStaffInfo, mStaff));
    out[n++] = static_cast<int64_t>(offsetof(CMsgStaffInfo, mSituation));
    return n;
}

// info: a block whose +8 and +0xc hold the staff type and situation (host byte order).
void tpw_hsi_handle(uint64_t object, uint64_t info) {
    reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(object))->HandleStaffInfo(*reinterpret_cast<const CMsgStaffInfo*>(static_cast<uintptr_t>(info)));
}

int32_t tpw_hsi_serialise(uint64_t object, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(object))
        ->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}

}  // extern "C"
