#include "Serialise.h"
#include "TbTimeDiff.h"
#include "analyser.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "Advisor.h"
#include "CMessageCentre.h"

uint16_t gAdvisorNullThingId = 0;   // 0x1027de64

namespace {

// The request's first word as the Mac reads it with lwz (big-endian: thing id in the high half, then the two padding bytes).
uint32_t FirstWord(const CAdvisorRequest& r) {
    return (static_cast<uint32_t>(r.mThingId) << 16) | (static_cast<uint32_t>(r.mPad02[0]) << 8) | r.mPad02[1];
}

// The tail both functions share: the line was chosen; say 0 and fail for 0x266, else call, tag and post.
int32_t Speak(CAdvisorRequest& r, uint32_t* length, int32_t response) {
    if (response == 0x266) {
        *length = 0;
        return 0;
    }
    *length = static_cast<uint32_t>(ADV_Call(AdvisorResponse{response}));
    int32_t tag = ADV_GetTag(AdvisorResponse{response});
    CMsgTagObj msg(tag, FirstWord(r), r.mSecondary, -1);
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre();
    gMessageCentre->PostMessage(msg);
    return 1;
}

int32_t PickResponse(const CAdvisorRequest& r, int32_t responseNo) {
    CAdvisorScoring& scoring = TheScoring();
    if (responseNo < scoring.GetNumResponsesFor(r.mMessage)) return responseNo + TheScoring().GetFirstResponseFor(r.mMessage);
    return TheScoring().GetFirstResponseFor(r.mMessage);
}

}  // namespace

// 0x1000dff4 (684 bytes).
int32_t CAdvisorRequest::Play(uint32_t* length, int32_t responseNo) {
    if (mValid == 0) return 0;
    mValid = 0;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId;
    thingId.mIndex = 0;
    uint32_t value = 0;
    int32_t score = TheScoring().LastMinuteCheck(mMessage, thingId, value);
    if (score < kAdvisorBalanceMinScore) return 0;
    if (thingId.mIndex != gAdvisorNullThingId) mThingId = thingId.mIndex;
    if (value != 0) mSecondary = value;
    return Speak(*this, length, PickResponse(*this, responseNo));
}

// 0x1000e340 (492 bytes).
int32_t CAdvisorRequest::PlayWithoutScore(uint32_t* length, int32_t responseNo) {
    if (mValid == 0) return 0;
    mValid = 0;
    return Speak(*this, length, PickResponse(*this, responseNo));
}
