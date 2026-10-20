// Test library for CAdvisor::HandleEvent (unit v5679; libtpw_unit_CAdvisor_HandleEvent.dylib, tests/test_CAdvisor_HandleEvent.py), main app:
//   src/AdvisorQueue_CAdvisor_HandleEvent.cpp   CAdvisor::HandleEvent
//
// The callees outside the unit (the request constructor, GetMessageCategory, CanIQueueThis, FindLowestScoringSlotInQueue, UDebug, the
// game type constructor) are fakes that record the arguments and answer from the test's queues. TheScoring is defined here, with the
// guard byte at 0x102b4100 and the two events the original's first-use code makes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "FS_Online.h"
#include "platform/samsutils.h"

using namespace calllog;

uint8_t gScoringGuard = 0;
static uint8_t gGameTypeGuardDummy = 0;
static intptr_t gAdvBase = 0;
static bool gQuiet = false;
alignas(16) static uint8_t gScoringStorage[sizeof(CAdvisorScoring)];

CAdvisorScoring& TheScoring() {
    CAdvisorScoring* scoring = reinterpret_cast<CAdvisorScoring*>(gScoringStorage);
    if (gScoringGuard == 0) {
        ev("CAdvisorScoring", {lab(scoring)});
        ev("register", {lab(scoring), 0x102657e0, 0x102b4104});
        gScoringGuard = 1;
    }
    return *scoring;
}

CGameType::CGameType() {
    if (!gQuiet) ev("CGameType", {lab(this)});
    mType = kGameTypeNormal;
}

CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("CAdvisorRequest", {static_cast<int64_t>(message), static_cast<int64_t>(thingId.mIndex), static_cast<int64_t>(secondary),
                           static_cast<int64_t>(responseNo), static_cast<int64_t>(overrideOnlyOnce), static_cast<int64_t>(score)});
    mThingId = thingId.mIndex;
    mPad02[0] = mPad02[1] = 0;
    mSecondary = secondary;
    mResponseNo = responseNo;
    mMessage = message;
    mScore = static_cast<int32_t>(ret("rscore"));
    mValid = static_cast<uint8_t>(ret("rvalid"));
    mOverrideOnlyOnce = static_cast<uint8_t>(ret("rover") & 1);
    mScoreOverridden = static_cast<uint8_t>(ret("rso"));
    mPad17 = 0;
}

int32_t CAdvisorScoring::GetMessageCategory(AdvisorMessage message) const {
    ev("GetMessageCategory", {lab(this), static_cast<int64_t>(message)});
    return static_cast<int32_t>(ret("cat"));
}

bool CAdvisor::CanIQueueThis(AdvisorMessage message, bool force) const {
    ev("CanIQueueThis", {static_cast<int64_t>(reinterpret_cast<intptr_t>(this) - gAdvBase), static_cast<int64_t>(message),
                         static_cast<int64_t>(force)});
    return (ret("can") & 0xff) != 0;
}

int32_t CAdvisor::FindLowestScoringSlotInQueue() {
    ev("FindLowest", {static_cast<int64_t>(reinterpret_cast<intptr_t>(this) - gAdvBase)});
    return static_cast<int32_t>(ret("low"));
}

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_hev_addr(const char* name) {
    if (std::strcmp(name, "scoring_guard") == 0) return reinterpret_cast<uintptr_t>(&gScoringGuard);
    if (std::strcmp(name, "scoring") == 0) return reinterpret_cast<uintptr_t>(gScoringStorage);
    if (std::strcmp(name, "gametype_guard") == 0) return reinterpret_cast<uintptr_t>(&gGameTypeGuardDummy);
    if (std::strcmp(name, "gametype") == 0) {
        gQuiet = true;
        CGameType* type = &TheGameType();
        gQuiet = false;
        return reinterpret_cast<uintptr_t>(type);
    }
    return 0;
}

uint64_t tpw_hev_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hev_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mQueue));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorRequest));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mMessageState));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisor, mLastTutorialRequest));
    out[n++] = static_cast<int64_t>(sizeof(AdvisorMessageState));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisor));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mValid));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mScore));
    return n;
}

void tpw_hev_call(uint64_t adv, int32_t event) {
    gAdvBase = static_cast<intptr_t>(adv);
    reinterpret_cast<CAdvisor*>(adv)->HandleEvent(static_cast<EventType>(event));
}

}  // extern "C"
