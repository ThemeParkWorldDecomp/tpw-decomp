// The park analyser's messages, month end, admissions, thoughts and cell save (main app, analyser.cpp range; unit w161): CParkAnalyser::ReceiveMessage
// 0x100e41bc (360 bytes), PersonPaidAdmission 0x100e4470 (216), UpdateEndOfMonth 0x100e47fc (528), GetThreeMostDominantThoughts 0x100e5dc8 (576),
// FinalDestroy 0x100e6fd0 (36), CParkAnalyserCell::Serialise 0x100e60a0 (1,220) and __sinit_analyser_cpp 0x100f1fb0 (28). The messages themselves are in
// src/ParkMessages.cpp. CPeep has no layout in the headers, so the thought of a visitor is read at its Mac offset.
#include <cstdint>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "History.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"

// __sinit_analyser_cpp 0x100f1fb0 (28 bytes): two words of the file's BSS, set at start-up (0 first, then 0x10000). Their use is not known; the names are
// mine (the addresses are their names).
uint32_t gAnalyserWord1034C19C = 0x10000;   // 0x1034c19c
uint32_t gAnalyserWord1034C1A0 = 0;         // 0x1034c1a0

namespace {

constexpr uint16_t kNoThing = 0;
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242
constexpr uint8_t kKindPerson = 1;
constexpr uint32_t kPeepThought = 0x30;   // word: the visitor's current thought, 0 .. 0x16 (name mine)
constexpr int32_t kThoughtCount = 0x17;
constexpr uint8_t kNoThought = 0x17;

constexpr int32_t kMsgEndOfMonth = 0xc;
constexpr int32_t kMsgResearchCompleted = 0x11;
constexpr int32_t kMsgEvent = 0x13;
constexpr int32_t kMsgStaffInfo = 0x17;
constexpr int32_t kEventParkOpened = 3;
constexpr int32_t kSituationStrike = 2;

// Copy the timestamp image as words, second word first (as src/ParkAnalyser_ctor.cpp does).
void CopyStamp(uint32_t (&target)[2], const TbTimeStamp& stamp) {
    uint32_t words[2];
    std::memcpy(words, &stamp, sizeof words);
    target[1] = words[1];
    target[0] = words[0];
}

template <class T>
T PeepWord(const CThing* thing, uint32_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const unsigned char*>(thing) + offset, sizeof value);
    return value;
}

template <class V>
int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

}  // namespace

// 0x100e6fd0 (36 bytes): CThing::FinalDestroy of the base.
void CParkAnalyser::FinalDestroy() { mBase.FinalDestroy(); }

// 0x100e41bc (360 bytes). The message type minus 0xc selects (jump table 0x102a0424): 0xc month end, 0x11 research completed, 0x13 event, 0x17 staff
// info; every other type does nothing.
void CParkAnalyser::ReceiveMessage(const CMsg& message) {
    switch (message.GetType()) {
        case kMsgEndOfMonth:
            UpdateEndOfMonth();
            return;
        case kMsgResearchCompleted: {
            const CMsgResearchCompleted& done = static_cast<const CMsgResearchCompleted&>(message);
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id{done.mObj};
            const RideInfo* ride = gRideDatabase.GetRideInfo(id);
            SetLastOfObjectTypeResearched(static_cast<UIObjType>(ride->mUIType), id);
            return;
        }
        case kMsgEvent: {
            const CMsgEvent& event = static_cast<const CMsgEvent&>(message);
            if (event.mEvent == kEventParkOpened) {
                CopyStamp(mParkLastOpened, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            }
            return;
        }
        case kMsgStaffInfo: {
            const CMsgStaffInfo& info = static_cast<const CMsgStaffInfo&>(message);
            if (info.mSituation == kSituationStrike) {
                CopyStamp(mLastStrike, gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
            }
            return;
        }
        default:
            return;
    }
}

// 0x100e4470 (216 bytes): counts the visitor, keeps the largest price, and posts a CMsgAddToChallenge (type 0xb, value 1, both ids 0) to the message
// centre, which is made on the first post (a failed new leaves the pointer NULL). The answer of PostMessage is ignored.
void CParkAnalyser::PersonPaidAdmission(int32_t price) {
    ++mLifetimeVisitors;
    if (price > mMostPaidForTicket) mMostPaidForTicket = price;
    CMsgAddToChallenge message(static_cast<ChallengeType>(0xb), 1, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0},
                               tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0});
    MessageCentreForPost()->PostMessage(message);
}

// 0x100e47fc (528 bytes): the month-end entries, in the original's order.
void CParkAnalyser::UpdateEndOfMonth() {
    mHistory1F100[0].SetAtEnd((*gGameWorldRef).GetBankAccount()->GetBalance());
    static const int kFromTemp[] = {2, 3, 4, 5, 6, 7, 8, 12};
    for (int k : kFromTemp) {
        mHistory1F100[k].SetFromTemp();
        mHistory1F100[k].mTemp = 0;
    }
    mHistory1F100[9].SetAtEnd(GetVisitorsInPark(0));
    mHistory1F100[10].SetAtEnd(NumberOfVisitorsSinceBeginning());
    mHistory1F100[11].SetAtEnd(GetRidesInPark(0));
    mHistory20F10.SetAtEnd(GetHappinessLevel(0));
    mHistory1F100[1].SetAtEnd(GetParkValue(0));
    mMisbehavingKids = 0;
    mMisbehavingKidsThatGotAwayWithIt = 0;
    int32_t mean;
    mHistory21600.GetAverageOfLast(mean, 50);
    mHistory21160[0].SetAtEnd(mean);
    mHistory21160[1].SetAtEnd(GetOverallParkRating());
}

// 0x100e5dc8 (576 bytes): counts the thought of every visitor in the park, then picks the three most common (a thought counts only when strictly
// greater than the one it replaces; ties keep the earlier). Thought 0 is never picked; counts[1] and counts[2] are not cleared before the ranking.
void CParkAnalyser::GetThreeMostDominantThoughts(uint8_t& first, uint8_t& second, uint8_t& third) const {
    int32_t counts[kThoughtCount];
    for (int32_t i = 0; i < kThoughtCount; ++i) counts[i] = 0;
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == kKindPerson && reinterpret_cast<CPeep*>(thing)->IsInsidePark() != 0) {
            ++counts[PeepWord<int32_t>(thing, kPeepThought)];
        }
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
    counts[0] = 0;
    uint8_t best[3] = {0, 1, 2};
    for (int32_t i = 3; i < kThoughtCount; ++i) {
        int32_t gain = 0;
        int32_t slot = -1;
        for (int32_t j = 0; j < 3; ++j) {
            const int32_t d = counts[i] - counts[best[j]];
            if (d > gain) {
                gain = d;
                slot = j;
            }
        }
        if (slot >= 0) best[slot] = static_cast<uint8_t>(i);
    }
    first = counts[best[0]] == 0 ? kNoThought : best[0];
    second = counts[best[1]] == 0 ? kNoThought : best[1];
    third = counts[best[2]] == 0 ? kNoThought : best[2];
}

// 0x100e60a0 (1,220 bytes): for 0 <= version < 2 the four rectangle words (LOADVAR / SAVEVAR of int), then the two float Vals and the seven int Vals;
// the first nonzero answer ends it with 1, the end answers 0 (so does any other version, with no call).
int32_t CParkAnalyserCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version < 0 || version >= 2) return 0;
    if (Var<int32_t>(save, file, &mStartX, "mStartX") != 0) return 1;
    if (Var<int32_t>(save, file, &mStartY, "mStartY") != 0) return 1;
    if (Var<int32_t>(save, file, &mSizeX, "mSizeX") != 0) return 1;
    if (Var<int32_t>(save, file, &mSizeY, "mSizeY") != 0) return 1;
    if (mVal10.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal1C.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal28.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal34.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal40.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal4C.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal58.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal64.Serialise(file, save, version, unused) != 0) return 1;
    if (mVal70.Serialise(file, save, version, unused) != 0) return 1;
    return 0;
}
