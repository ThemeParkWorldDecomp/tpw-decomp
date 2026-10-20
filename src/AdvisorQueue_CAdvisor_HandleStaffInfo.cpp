// Advisor.cpp (main app): CAdvisor::HandleStaffInfo 0x1000c40c and CAdvisor::Serialise 0x1000a468. Offsets are the Mac's.
#include "AdvisorQueue.h"
#include "Debug.h"
#include "ParkMessages.h"
#include "Serialise.h"

// 0x1000c40c, 660 bytes. For a staff type outside 0..4 the original traces and carries on with whatever r26 held (the
// message below stays 0 here).
void CAdvisor::HandleStaffInfo(const CMsgStaffInfo& info) {
    const int32_t staff = static_cast<int32_t>(info.mStaff);
    const int32_t situation = static_cast<int32_t>(info.mSituation);
    int32_t message = 0;
    switch (staff) {
        case 0: message = situation * 5 + 0x22; break;
        case 1: message = situation * 5 + 0x21; break;
        case 2: message = situation * 5 + 0x24; break;
        case 3: message = situation * 5 + 0x23; break;
        case 4: message = situation * 5 + 0x25; break;
        default: CTRACE(3, "CAdvisor::HandleStaffInfo() doesn't know about the staff type %d.\n", staff); break;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId;
    thingId.mIndex = 0;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), thingId, 0, -1, false, -1);
    if (TheScoring().GetMessageCategory(request.mMessage) == 1) mLastTutorialRequest = request;
    if (!CanIQueueThis(request.mMessage, request.mOverrideOnlyOnce != 0)) return;
    int32_t slot;
    for (slot = 0; slot < 8; slot++) {
        if (mQueue[slot].Valid() == 0) break;
    }
    if (slot >= 8) slot = -1;
    if (slot >= 0) {
        mQueue[slot] = request;
        return;
    }
    const int32_t lowest = FindLowestScoringSlotInQueue();
    if (lowest < 0) return;
    const int32_t newScore = request.mValid != 0 ? request.mScore : -1;
    const int32_t oldScore = mQueue[lowest].mValid != 0 ? mQueue[lowest].mScore : -1;
    if (oldScore < newScore) mQueue[lowest] = request;
}

namespace {
template<class V> int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}
template<class V> int32_t Struct(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVARSTRUCT<V>(file, value, name, 0) : LOADVARSTRUCT<V>(file, value, name, 0);
}
}  // namespace

// 0x1000a468, 1072 bytes. The save and load halves are the same walk.
int32_t CAdvisor::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (version < 0 || version >= 2) return 0;
    if (Clock().Serialise(file, save, version, unused) != 0) return 1;
    if (Var<MacLong>(save, file, reinterpret_cast<MacLong*>(&mF0C[4]), "mSoundId") != 0) return 1;
    for (int32_t i = 0; i < 8; i++) {
        if (mQueue[i].Serialise(file, save, version, unused) != 0) return 1;
    }
    if (Var<int32_t>(save, file, reinterpret_cast<int32_t*>(&mFD4[0]), "mNextAvailableQueuePos") != 0) return 1;
    if (Var<MacULong>(save, file, reinterpret_cast<MacULong*>(&mPlayStartTime), "mLastActionStarted") != 0) return 1;
    if (Var<MacULong>(save, file, reinterpret_cast<MacULong*>(&mPlayDuration), "mLastActionDuration") != 0) return 1;
    for (int32_t i = 0; i < 0x15f; i++) {
        if (Struct<AdvisorMessageState>(save, file, &mMessageState[i], "mHistory[i]") != 0) return 1;
    }
    if (mLastTutorialRequest.Serialise(file, save, version, unused) != 0) return 1;
    if (Var<bool>(save, file, reinterpret_cast<bool*>(&mF16E8[0]), "mRequestedLastTutorial") != 0) return 1;
    if (Var<AdvisorMessage>(save, file, &mTypePlaying, "mMessagePlaying") != 0) return 1;
    return 0;
}
