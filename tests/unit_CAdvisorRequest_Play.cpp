// Test library for CAdvisorRequest::Play and PlayWithoutScore (unit v51003; libtpw_unit_CAdvisorRequest_Play.dylib), main app:
//   src/game/advisor/Advisor.cpp
//
// The callees outside the unit are recording fakes: the scoring object's three methods, ADV_Call, ADV_GetTag, the message constructor,
// PostMessage and the subscription header constructor (stands for the Mac's __construct_array). gMessageCentre is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Advisor.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"

using namespace calllog;

CMessageCentre* gMessageCentre = nullptr;

namespace {
alignas(16) unsigned char gScoringBuffer[0x8000];
}

CAdvisorScoring& TheScoring() { return *reinterpret_cast<CAdvisorScoring*>(gScoringBuffer); }

CSubscriptionHeader::CSubscriptionHeader() { ev("sub_ctor", {}); }
CSubscriptionHeader::~CSubscriptionHeader() {}

int32_t CAdvisorScoring::LastMinuteCheck(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& thingId, uint32_t& value) {
    ev("LastMinuteCheck", {lab(this), static_cast<int64_t>(message)});
    if (ret("lmc_write") != 0) {
        thingId.mIndex = static_cast<uint16_t>(ret("lmc_id"));
        value = static_cast<uint32_t>(ret("lmc_val"));
    }
    return static_cast<int32_t>(ret("lmc"));
}

int32_t CAdvisorScoring::GetNumResponsesFor(AdvisorMessage message) const {
    ev("GetNumResponsesFor", {lab(this), static_cast<int64_t>(message)});
    return static_cast<int32_t>(ret("num"));
}

int32_t CAdvisorScoring::GetFirstResponseFor(AdvisorMessage message) const {
    ev("GetFirstResponseFor", {lab(this), static_cast<int64_t>(message)});
    return static_cast<int32_t>(ret("first"));
}

int32_t ADV_Call(AdvisorResponse response) {
    ev("ADV_Call", {response.mId});
    return static_cast<int32_t>(ret("call"));
}

int32_t ADV_GetTag(AdvisorResponse response) {
    ev("ADV_GetTag", {response.mId});
    return static_cast<int32_t>(ret("tag"));
}

// The real constructor (src/game/ui/MessageCentre.cpp) keeps only the thing word's high half; the fake saves the whole word, which the PPC stub
// stores at +0xc, so the PostMessage records still compare it.
static uint32_t gMsgTagThingWord = 0;

CMsgTagObj::CMsgTagObj(int32_t tag, uint32_t thingWord, uint32_t secondary, int32_t extra)
    : mTag(tag), mThing(static_cast<uint16_t>(thingWord >> 16)), mTime(extra), mValue(secondary) {
    gMsgTagThingWord = thingWord;
    ev("CMsgTag", {tag, thingWord, secondary, extra});
}

int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const CMsgTagObj& m = static_cast<const CMsgTagObj&>(message);
    ev("PostMessage", {lab(this), m.mTag, gMsgTagThingWord, m.mValue, m.mTime});
    return 0;
}

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"scoring", gScoringBuffer},
        {"nullid", &gAdvisorNullThingId},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_adv_set_mc(uint64_t p) { gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(p)); }
uint64_t tpw_adv_get_mc() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gMessageCentre)); }

int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mSecondary));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mValid));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorRequest));
    return n;
}

uint32_t tpw_adv_play(uint64_t req, uint64_t length, int32_t n) {
    return static_cast<uint32_t>(reinterpret_cast<CAdvisorRequest*>(static_cast<uintptr_t>(req))
                                     ->Play(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(length)), n));
}

uint32_t tpw_adv_playws(uint64_t req, uint64_t length, int32_t n) {
    return static_cast<uint32_t>(reinterpret_cast<CAdvisorRequest*>(static_cast<uintptr_t>(req))
                                     ->PlayWithoutScore(reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(length)), n));
}

}  // extern "C"
