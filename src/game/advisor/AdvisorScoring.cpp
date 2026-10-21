// AdvisorScoring.cpp: name proven by __sinit_AdvisorScoring_cpp in the main application.

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "AudioPlaceHolder.h"
#include "CMpegBase.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "helpers/FS_OnlineListHelpers.h"
#include "helpers/InterfaceTimerHelpers.h"
#include "helpers/PeepHelpers.h"
#include "helpers/PlaceHolderSentenceHelpers.h"
#include "helpers/StreamableMpeg2Helpers.h"
#include "helpers/TbSoundSampleCallbackHelpers.h"
#include "helpers/TbSoundSampleWAVHelpers.h"
#include "helpers/ui_allStaffPanelHelpers.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "LbFile.h"
#include "LbTime.h"
#include "MainBalance.h"
#include "MainWindowCallback.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "PlaceHolderHelpers.h"
#include "PlaceHolderTypes.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Serialise.h"
#include "snd_handles.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundManager.h"
#include "TbSoundSampleCallback.h"
#include "TbSoundSampleMPEG2.h"
#include "TbTimeDiff.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// AdvisorScoring.cpp (main app of SimTheme Park 2.04b3): the advisor's message scoring, 0x10010054..0x10010f38 and the destructor at
// 0x1000adb0 (the inventory files it under Advisor.cpp). The constructor loads the advisor's balance table; the ten queries look a
// message up in the static record table (src/AdvisorScoring_table.cpp) and answer one of its fields, or a number of the message's category in
// the balance table, or run the record's scorer. The scorers themselves (the _ADVISOR_* members) are not here.
//
// Every query repeats the original's inlined lookup of a message's record: the two numbers kAdvisorMessageCount and kAdvisorNoMessage
// have none; otherwise the record at the message's own index is taken if its number matches, else all 351 records are searched, and a
// hit there STILL answers the record at the original index (the code adds the index it started with, not the one it found, to the
// table). With the table in order the search cannot hit; a patched table (the tests) shows it. A miss traces "Not found: %d\n" at level 3.

namespace {

// The record of a message, or null (with the trace line). The fast path compares the number of the record at the message's own index, which
// for an index beyond the table (or before it) reads whatever the file image holds there: only message -1 matches (record -1, see
// AdvisorScoring.h), and the host, which cannot read beyond its block, takes every message further out for a miss (the Mac's data there
// holds no word equal to its message number: checked over the whole image).
const AdvisorMessageRecord* FindMessage(int32_t message) {
    if (static_cast<uint32_t>(message) - static_cast<uint32_t>(kAdvisorMessageCount) < 2u) return nullptr;
    const bool inBlock = message >= -1 && message <= kAdvisorMessageCount;
    if (inBlock && message == gAdvisorMessages[message].mMessage) return &gAdvisorMessages[message];
    for (int32_t i = 0; i < kAdvisorMessageCount; ++i) {
        if (message == gAdvisorMessages[i].mMessage) return gAdvisorMessages + message;
    }
    CTRACE(3, "Not found: %d\n", message);
    return nullptr;
}

// The balance slot of a category's MessageGroups element: member 0 MinTimeSameMessage, 1 SayOnlyOnce, 2 DiscardAfterSlaps.
int32_t GroupSlot(int32_t category, int32_t member) {
    return kAdvisorGroupSlot + category * kAdvisorGroupStride + member;
}

}  // namespace

// 0x10010054: the table's vptr is written twice (the base's, then CAdvisorBalance's); on the host the compiler does that. Init, the
// cursor, then the file; a failed Load only traces.
CAdvisorScoring::CAdvisorScoring() {
    mBalance.Init();
    mCursor = 0;
    if (mBalance.Load("data:advisor:advisor.sam", CBalanceTable::kLoadFresh) == 0) {
        CTRACE(0, "!!!!!\nLoading the advisor.sam went horribly wrong! Shout at Bjarne!\n!!!!!");
    }
}

// 0x1001019c: -1 for no record, then -1 when the message is not valid in the current game, else the scorer's answer.
int32_t CAdvisorScoring::GetScoreFor(AdvisorMessage message) {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return -1;
    if (!IsThisValidInTheCurrentMode(message)) return -1;
    return (this->*record->mScorer)();
}

// Unit v5937: SAVEVARSTRUCT<AdvisorHistory> 0x1000f96c, CAdvisorScoring::LastMinuteCheck 0x100102d8, CAdvisorRequest::Serialise 0x1000e568.

// 0x100102d8 (276 bytes). The record is always the one at the message's own index; the search for another record with the number only
// decides whether there is a record at all (the original's quirk).
int32_t CAdvisorScoring::LastMinuteCheck(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& thingId, uint32_t& value) {
    AdvisorMessageRecord* record;
    if (static_cast<uint32_t>(message - 0x15f) < 2u) {
        record = nullptr;
    } else {
        record = &gAdvisorMessages[message];
        if (message != record->mMessage) {
            int32_t i = 0;
            for (; i < 0x15f; i++) {
                if (message == gAdvisorMessages[i].mMessage) break;
            }
            if (i >= 0x15f) {
                CTRACE(3, "Not found: %d\n", static_cast<int>(message));
                record = nullptr;
            }
        }
    }
    if (record == nullptr) return -1;
    mWord4 = 0;
    mWord8 = 0;
    int32_t result;
    if (IsThisValidInTheCurrentMode(message)) result = (this->*(record->mScorer))();
    else result = -1;
    thingId.mIndex = mWord4;
    value = mWord8;
    return result;
}

// Unit v5767: CAdvisorScoring::Load, Save, GetFirstResponseFor and three scorers (_ADVISOR_TUTORIAL_UI_PANEL,
// _ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT, _ADVISOR_PRICING_DRINKS_POOR_VALUE).

uint16_t gAdvisorNoneDrinksPoorValue = 0;   // 0x1027df0c (zero in the original's data)

namespace {

// A balance value of the advisor table by its Mac offset in the scoring object (values start at 0x14 on the Mac).
inline const BalanceValue& Slot(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4];
}

}  // namespace

// The first response line of the message's record, 0x266 without a record. Message 0x15f and 0x160 have none. The record is
// found at index `message` (no range check, as the original); when its own number differs the table is searched for the number,
// and the record at index `message` is used all the same when it is found.
int32_t CAdvisorScoring::GetFirstResponseFor(AdvisorMessage message) const {
    const AdvisorMessageRecord* record;
    if (static_cast<uint32_t>(message) - 0x15f <= 1u) {
        record = nullptr;
    } else {
        record = &gAdvisorMessages[message];
        if (message != record->mMessage) {
            int32_t i = 0;
            for (; i < 0x15f; i++)
                if (message == gAdvisorMessages[i].mMessage) break;
            if (i >= 0x15f) {
                CTRACE(3, "Not found: %d\n", static_cast<int32_t>(message));
                record = nullptr;
            }
        }
    }
    if (record == nullptr) return 0x266;
    return record->mFirstResponse;
}

// 0x1001055c.
int32_t CAdvisorScoring::GetNumResponsesFor(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return 0;
    return record->mNumResponses;
}

// 0x10010654: the record's word 7 as a byte.
uint32_t CAdvisorScoring::GetMaxDuplicatesInQueueFor(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return 0;
    return static_cast<uint32_t>(record->mMaxDuplicates) & 0xff;
}

// 0x10010758: no check of the category (a record's is 0 .. 9; 10 reads the slots after the array's last element).
int32_t CAdvisorScoring::GetMinTimeFor(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return 0;
    return mBalance.mValues[GroupSlot(record->mCategory, 0)].mInteger;
}

// 0x10010864.
bool CAdvisorScoring::SayOnlyOnce(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return false;
    return mBalance.mValues[GroupSlot(record->mCategory, 1)].mInteger != 0;
}

// 0x10010980.
int32_t CAdvisorScoring::GetCyclingFor(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return 5;
    return record->mCycling;
}

// 0x10010a74.
int32_t CAdvisorScoring::GetMessageCategory(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return 10;
    return record->mCategory;
}

// 0x10010b6c: no record or category 10 never; a DiscardAfterSlaps of 0 never; else when `killed` has reached it (signed compare).
bool CAdvisorScoring::HasThisBeenKilledTooManyTimes(AdvisorMessage message, int32_t killed) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    const int32_t category = record == nullptr ? 10 : record->mCategory;
    if (category == 10) return false;
    const int32_t limit = mBalance.mValues[GroupSlot(category, 2)].mInteger;
    if (limit == 0) return false;
    if (killed < limit) return false;
    return true;
}

// 0x10010cc8: mode 2 anywhere; mode 0 when the game type is 0 (normal), mode 1 when it is 2 (easy mode). The game type object is made on
// first use (TheGameType's guard). Any other mode, or a mismatch, is "not valid".
bool CAdvisorScoring::IsThisValidInTheCurrentMode(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return false;
    if (record->mMode == 2) return true;
    if (record->mMode == 0) {
        if (TheGameType().mType == 0) return true;
    }
    if (record->mMode == 1) {
        if (TheGameType().mType == 2) return true;
    }
    return false;
}

// 0x10010e78: the tutorial messages are the category 1 ones.
bool CAdvisorScoring::IsThisATutorialMessage(AdvisorMessage message) const {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return false;
    return record->mCategory == 1;
}

// Unit v5971: the open / close park, researcher and coverage scorers of CAdvisorScoring and GetHighestScoringFromNext.
// Balance values are read by their Mac offset in CAdvisorScoring (the values start at 0x14, four bytes each).

namespace {

int32_t Slot_2(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}

CThing::ThingModel Model(int32_t n) { return static_cast<CThing::ThingModel>(n); }

// The park time in 0x1792F8648000-tick units, as the 64 bit divide helper 0x10237e1c leaves it (the low word is used).
int32_t SessionUnits() {
    TbTimeDiff t = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    return static_cast<int32_t>(t.mTicks / 0x1792F8648000LL);
}

// The original's inlined record lookup (the same as src/AdvisorScoring.cpp's), including its bug of answering the record at the
// message's own index when the number was found elsewhere in the table.
const AdvisorMessageRecord* Lookup(int32_t message) {
    if (static_cast<uint32_t>(message) - static_cast<uint32_t>(kAdvisorMessageCount) < 2u) return nullptr;
    const bool inBlock = message >= -1 && message <= kAdvisorMessageCount;
    if (inBlock && message == gAdvisorMessages[message].mMessage) return &gAdvisorMessages[message];
    for (int32_t i = 0; i < kAdvisorMessageCount; ++i) {
        if (message == gAdvisorMessages[i].mMessage) return gAdvisorMessages + message;
    }
    CTRACE(3, "Not found: %d\n", message);
    return nullptr;
}

}  // namespace

// 0x10010f88: scores the next `count` considered records from the cursor (wrapping at 0x15f); the best one the advisor can queue wins.
AdvisorMessage CAdvisorScoring::GetHighestScoringFromNext(int32_t count) {
    int32_t result = kAdvisorNoMessage;
    int32_t best = 0;
    for (int32_t done = 0; done < count;) {
        if (mCursor >= kAdvisorMessageCount) mCursor = 0;
        if (gAdvisorMessages[mCursor].mSkipped == 0) {
            const AdvisorMessageRecord* record = Lookup(gAdvisorMessages[mCursor].mMessage);
            bool valid;
            if (record == nullptr) {
                valid = false;
            } else if (record->mMode == 2) {
                valid = true;
            } else {
                valid = false;
                if (record->mMode == 0) {
                    if (TheGameType().mType == 0) valid = true;
                }
                if (!valid && record->mMode == 1) {
                    if (TheGameType().mType == 2) valid = true;
                }
            }
            int32_t score = valid ? (this->*gAdvisorMessages[mCursor].mScorer)() : -1;
            if (best < score) {
                if (gGameWorldRef->GetAdvisor()->CanIQueueThis(static_cast<AdvisorMessage>(gAdvisorMessages[mCursor].mMessage), false)) {
                    result = gAdvisorMessages[mCursor].mMessage;
                    best = score;
                }
            }
            ++done;
        }
        ++mCursor;
    }
    return static_cast<AdvisorMessage>(result);
}

// Reads the cursor from the file as 4 raw bytes, then swaps it. "read failed" when the count read shifted right by 2 is not 1
// (so 4 to 7 bytes pass).
int32_t CAdvisorScoring::Load(void*& file) {
    uint32_t transferred;
    LbFile_Read(file, &mCursor, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    mCursor = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(mCursor)));
    return 1;
}

int32_t CAdvisorScoring::Save(void*& file) const {
    uint32_t swapped = __builtin_bswap32(static_cast<uint32_t>(mCursor));
    uint32_t transferred;
    LbFile_Write(file, &swapped, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

// Unit v51116: the 8 byte scorers (a balance value read, or 0), two small destructors and the sound library's inline byte swaps.

namespace {
// The Mac offset of a balance value in the scoring object: the table is at 0x0c, its values from +8, so slot i is at 0x14 + 4 * i.
inline int32_t Bal(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}
}  // namespace

int32_t CAdvisorScoring::_ADVISOR_NO_SCORE() { return 0; }                          // 0x10011358
int32_t CAdvisorScoring::_ADVISOR_WELCOME() { return Bal(this, 0xa0); }             // 0x1001139c

// 0x100113dc
int32_t CAdvisorScoring::_ADVISOR_OPEN_PARK() {
    if (gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    bool rides = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) > 0;
    bool sideshows = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjSideshow, 0) > 0;
    if (rides || sideshows) {
        int32_t outside = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsOutsidePark();
        return Slot_2(this, 0xa4) * outside;
    }
    return 0;
}

// 0x1001151c
int32_t CAdvisorScoring::_ADVISOR_CLOSE_PARK() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    bool rides = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) > 0;
    bool sideshows = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjSideshow, 0) > 0;
    if (rides || sideshows) return 0;
    return Slot_2(this, 0xa8);
}

// AdvisorScoring (main app): the staff hire, hungry and thirsty visitor scorers (unit v5136).
//   _ADVISOR_VISITORS_THIRSTY 0x10011640, _ADVISOR_VISITORS_HUNGRY 0x100116d8, _ADVISOR_STAFF_HIRE_MECHANICS_1 0x1001176c,
//   _ADVISOR_STAFF_HIRE_MECHANICS_2 0x10011838, _ADVISOR_STAFF_HIRE_MECHANICS_3 0x10011900, _ADVISOR_STAFF_HIRE_HANDYMEN_1 0x10011a08,
//   _ADVISOR_STAFF_HIRE_HANDYMEN_2 0x10011b0c, _ADVISOR_STAFF_HIRE_HANDYMEN_3 0x10011c0c.
// The balance values are read at Mac offsets of the scoring object: the table's values start at 0x18 (slot 0), four bytes a slot.

namespace {

// The balance value the Mac reads at `macOffset` of the scoring object.
inline int32_t Bal_2(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x18) / 4].mInteger;
}

inline CParkAnalyser* Analyser() { return gGameWorld->GetParkAnalyser(); }

const CThing::ThingModel kMechanic = static_cast<CThing::ThingModel>(4);
const CThing::ThingModel kHandyman = static_cast<CThing::ThingModel>(5);

}  // namespace

int32_t CAdvisorScoring::_ADVISOR_VISITORS_THIRSTY() {
    int32_t count = Analyser()->NumberOfThirstyVisitors(static_cast<uint8_t>(Bal_2(this, 0xb0)));
    return Bal_2(this, 0xac) * count;
}

int32_t CAdvisorScoring::_ADVISOR_VISITORS_HUNGRY() {
    int32_t count = Analyser()->NumberOfHungryVisitors(static_cast<uint8_t>(Bal_2(this, 0xb8)));
    return Bal_2(this, 0xb4) * count;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_1() {
    if (Analyser()->NumberOfStaffType(kMechanic) > 0) return 0;
    int32_t worn = Analyser()->NumberOfWornRidesInPark(Bal_2(this, 0xc0));
    return Bal_2(this, 0xbc) * worn;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_2() {
    if (Analyser()->NumberOfStaffType(kMechanic) > 0) return 0;
    int32_t broken = Analyser()->NumberOfBrokenDownRidesInPark();
    return Bal_2(this, 0xc4) * broken;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_MECHANICS_3() {
    if (Analyser()->GetDaysSinceLastStaffHired() < Bal_2(this, 0x630)) return 0;
    int32_t rides = Analyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    int32_t surplus = rides - Bal_2(this, 0xc8) * Analyser()->NumberOfStaffType(kMechanic);
    if (surplus <= 0) return 0;
    return surplus * Bal_2(this, 0xcc);
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_1() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(kHandyman) > 0) return 0;
    int32_t littered = Analyser()->GetPercentageLitteredCells();
    if (littered > Bal_2(this, 0xd8)) return 0;
    return Bal_2(this, 0xd0) * littered - Bal_2(this, 0xd4);
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_2() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(kHandyman) > 0) return 0;
    int32_t littered = Analyser()->GetPercentageLitteredCells();
    int32_t limit = Bal_2(this, 0xe0);
    if (littered < limit) return 0;
    return Bal_2(this, 0xdc) * littered - limit;
}

int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_HANDYMEN_3() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(kHandyman) == 0) return 0;
    int32_t cells = Analyser()->GetNumDevelopedCells(0);
    if (cells < Bal_2(this, 0xec)) return 0;
    int32_t surplus = cells - Bal_2(this, 0xe4) * Analyser()->NumberOfStaffType(kHandyman);
    if (surplus <= 0) return 0;
    return surplus * Bal_2(this, 0xe8);
}

// Unit v5265: seven staff scorers of CAdvisorScoring (guards 1..3 hiring, entertainer hiring, researcher hiring, mechanic and handyman
// patrol areas). Balance values are read by their Mac offset from the scoring object (slot 0 is at 0x14).

namespace {

inline CThing::ThingModel Model_2(int32_t m) { return static_cast<CThing::ThingModel>(m); }

}  // namespace

#define ADV_BAL(off) (mBalance.mValues[((off) - 0x14) / 4].mInteger)

// 0x10011d4c: hire guards when the park is open, the last hire was long enough ago, there is no guard and the park is busy.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_1() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->GetDaysSinceLastStaffHired() < ADV_BAL(0x630)) return 0;
    if (Analyser()->NumberOfStaffType(Model_2(7)) > 0) return 0;
    int32_t busy = Analyser()->HowBusyIsPark();
    return ADV_BAL(0xf4) * (busy - ADV_BAL(0xf0));
}

// 0x10011e64: no guard yet: a base plus a weight per misbehaving visitor last month.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_2() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(Model_2(7)) > 0) return 0;
    int32_t bad = Analyser()->MisbehavingVisitorsLastMonth(false);
    return ADV_BAL(0xf8) + ADV_BAL(0xfc) * bad;
}

// 0x10011f54: hire more guards by the developed cells left uncovered.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_GUARDS_3() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->GetDaysSinceLastStaffHired() < ADV_BAL(0x630)) return 0;
    int32_t cells = Analyser()->GetNumDevelopedCells(0);
    if (cells < ADV_BAL(0x108)) return 0;
    int32_t guards = Analyser()->NumberOfStaffType(Model_2(7));
    int32_t left = cells - ADV_BAL(0x100) * guards;
    if (left <= 0) return 0;
    return left * ADV_BAL(0x104);
}

// 0x10012090
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_ENTERTAINERS_1() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(Model_2(6)) > 0) return 0;
    if (Analyser()->PercentageOfVisitorsWithHappinessUnder(30) <= 30) return 0;
    int32_t busy = Analyser()->HowBusyIsPark();
    return ADV_BAL(0x110) * (busy - ADV_BAL(0x10c));
}

// 0x100121b0
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_1() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(8)) > 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfRidesLeftToResearch() == 0) return 0;
    int32_t available = gGameWorldRef->GetParkAnalyser()->TypesOfRidesAvailable(false);
    if (available == 0) return 0;
    int32_t inPark = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    if (inPark == available) return 0;
    int32_t percent = (inPark * 100) / available;
    if (percent > Slot_2(this, 0x114)) return Slot_2(this, 0x118) * (percent - Slot_2(this, 0x114));
    return 0;
}

// 0x10012328: no researcher, rides left to research, and every available ride type is in the park.
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_2() {
    if (gGameWorld->IsParkClosed()) return 0;
    if (Analyser()->NumberOfStaffType(Model_2(8)) > 0) return 0;
    if (Analyser()->TypesOfRidesLeftToResearch() == 0) return 0;
    int32_t available = Analyser()->TypesOfRidesAvailable(false);
    int32_t inPark = Analyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    return inPark == available ? ADV_BAL(0x11c) : 0;
}

// 0x10012468
int32_t CAdvisorScoring::_ADVISOR_STAFF_HIRE_RESEARCHERS_3() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    uint32_t maxAbility = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->MaxResearcherAbility());
    if (maxAbility == 0) return 0;
    if (SessionUnits() < Slot_2(this, 0x120)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(8)) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->CurrentNumberOfStaffTypeForHire(static_cast<StaffType>(4)) == 0) return 0;
    uint32_t ability = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->CurrentResearcherAbility() * 100);
    if (ability / maxAbility < static_cast<uint32_t>(Slot_2(this, 0x124))) return Slot_2(this, 0x128);
    return 0;
}

namespace {

// The funny time since the session start, in units of 0x1792f8648000 ticks (the 64-bit divide at 0x10237e1c), low word.
inline int32_t FunnyPeriods() {
    const CParkTime* parkTime = reinterpret_cast<const CParkTime*>(reinterpret_cast<const uint8_t*>(gGameWorld) + 0x2a0);
    TbTimeDiff d = parkTime->GetFunnyTimeSinceSessionStart();
    return static_cast<int32_t>(d.mTicks / 0x1792f8648000LL);
}

}  // namespace

// 0x1001260c
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_MECHANIC_AREA() {
    if (FunnyPeriods() < ADV_BAL(0x138)) return 0;
    int32_t staff = Analyser()->NumberOfStaffType(Model_2(4));
    if (staff < ADV_BAL(0x12c)) return 0;
    int32_t percent = Analyser()->NumberOfStaffTypeWithPatrolArea(Model_2(4)) * 100 / staff;
    if (percent > ADV_BAL(0x130)) return 0;
    return (100 - percent) * ADV_BAL(0x134);
}

// 0x1001274c (reads the world through gGameWorld for the last query)
int32_t CAdvisorScoring::_ADVISOR_STAFF_MECHANIC_COVERAGE() {
    if (SessionUnits() < Slot_2(this, 0x138)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(4)) < Slot_2(this, 0x12c)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffTypeWithPatrolArea(Model(4)) == 0) return 0;
    int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfAreaPatrolledBy(Model(4), static_cast<CParkAnalyser::AreaType>(2));
    int32_t result = Slot_2(this, 0x140) * (100 - percent);
    if (result < Slot_2(this, 0x13c)) return 0;
    return result;
}

// 0x100128a4
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_HANDYMAN_AREA() {
    if (FunnyPeriods() < ADV_BAL(0x150)) return 0;
    int32_t staff = Analyser()->NumberOfStaffType(Model_2(5));
    if (staff < ADV_BAL(0x144)) return 0;
    int32_t percent = Analyser()->NumberOfStaffTypeWithPatrolArea(Model_2(5)) * 100 / staff;
    if (percent > ADV_BAL(0x148)) return 0;
    return (100 - percent) * ADV_BAL(0x14c);
}

// 0x100129e4
int32_t CAdvisorScoring::_ADVISOR_STAFF_HANDYMAN_COVERAGE() {
    if (SessionUnits() < Slot_2(this, 0x150)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(5)) == 0) return 0;
    int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfAreaPatrolledBy(Model(5), static_cast<CParkAnalyser::AreaType>(3));
    int32_t result = Slot_2(this, 0x158) * (100 - percent);
    if (result < Slot_2(this, 0x154)) return 0;
    return result;
}

// Unit v5118: the staff scorers of the advisor (CAdvisorScoring::_ADVISOR_STAFF_*), 0x10012b0c .. 0x10013340.
// Staff types: 4 mechanic, 5 handyman, 6 entertainer, 7 guard. The balance values are read by their Mac offset in the scoring object
// (the balance table's value slot 0 is at 0x14, four bytes a slot on the Mac).

namespace {

int32_t StaffSlot(CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}

// The scorer common to the three SET_*_AREA functions.
int32_t SetArea(CAdvisorScoring* self, int32_t type, int32_t timeOff, int32_t minOff, int32_t maxOff, int32_t mulOff) {
    if (SessionUnits() < StaffSlot(self, timeOff)) return 0;
    int32_t count = gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(type));
    if (count < StaffSlot(self, minOff)) return 0;
    int32_t withArea = gGameWorldRef->GetParkAnalyser()->NumberOfStaffTypeWithPatrolArea(Model(type));
    int32_t percent = withArea * 100 / count;
    if (percent > StaffSlot(self, maxOff)) return 0;
    return (100 - percent) * StaffSlot(self, mulOff);
}

// The scorer common to the four TRAIN_* functions: hqOff is the training word of the headquarters (0xc + 4 * index).
int32_t Train(CAdvisorScoring* self, int32_t type, int32_t hqIndex, int32_t limitOff, int32_t scoreOff) {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(type)) == 0) return 0;
    if (static_cast<int32_t>(gGameWorldRef->GetStaffHQ()->mTraining.mF00[hqIndex]) > 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->AverageQualityOfStaffType(Model(type)) < StaffSlot(self, limitOff))
        return StaffSlot(self, scoreOff);
    return 0;
}

}  // namespace

// 0x10012b0c
int32_t CAdvisorScoring::_ADVISOR_STAFF_GUARD_BUILD_CAMERA() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(7)) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->MisbehavingVisitorsLastMonth(true) > StaffSlot(this, 0x15c)) return StaffSlot(this, 0x160);
    return 0;
}

// 0x10012bec
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_GUARD_AREA() { return SetArea(this, 7, 0x170, 0x164, 0x168, 0x16c); }

int32_t CAdvisorScoring::_ADVISOR_STAFF_GUARD_COVERAGE() { return 0; }              // 0x10012d2c
int32_t CAdvisorScoring::_ADVISOR_STAFF_GUARD_COVERAGE_LOST_WORLD() { return 0; }   // 0x10012d7c

// 0x10012dd4
int32_t CAdvisorScoring::_ADVISOR_STAFF_SET_ENTERTAINER_AREA() { return SetArea(this, 6, 0x188, 0x17c, 0x180, 0x184); }

// 0x10012f18
int32_t CAdvisorScoring::_ADVISOR_STAFF_ENTERTAINER_COVERAGE() {
    if (SessionUnits() < StaffSlot(this, 0x188)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(6)) == 0) return 0;
    int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfAreaPatrolledBy(Model(6), static_cast<CParkAnalyser::AreaType>(1));
    int32_t result = StaffSlot(this, 0x190) * (100 - percent);
    if (result < StaffSlot(this, 0x18c)) return 0;
    return result;
}

// 0x10013044
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_MECHANICS() { return Train(this, 4, 1, 0x194, 0x198); }

// 0x10013144
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_HANDYMEN() { return Train(this, 5, 0, 0x19c, 0x1a0); }

// 0x10013244
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_GUARDS() { return Train(this, 7, 3, 0x1a4, 0x1a8); }

// 0x10013340
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_ENTERTAINERS() { return Train(this, 6, 2, 0x1ac, 0x1b0); }

// 0x10013444
int32_t CAdvisorScoring::_ADVISOR_STAFF_TRAIN_RESEARCHERS() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model(8)) == 0) return 0;
    if (static_cast<int32_t>(gGameWorldRef->GetStaffHQ()->mTraining.mF00[4]) > 0) return 0;   // 0x1c: the training centre's fifth word
    if (gGameWorldRef->GetParkAnalyser()->AverageQualityOfStaffType(Model(8)) < Slot_2(this, 0x1b4)) return Slot_2(this, 0x1b8);
    return 0;
}

namespace {
// lwz r3, off(r3): the word at the Mac offset from this (unaligned-safe).
int32_t WordAt(const void* self, size_t off) {
    int32_t v;
    std::memcpy(&v, static_cast<const char*>(self) + off, 4);
    return v;
}
}  // namespace

#define ADV_WORD(name, off) \
    int32_t CAdvisorScoring::name() { return WordAt(this, off); }

ADV_WORD(_ADVISOR_STAFF_UNHAPPY_MECHANICS, 0x1bc)
ADV_WORD(_ADVISOR_STAFF_UNHAPPY_HANDYMEN, 0x1c0)
ADV_WORD(_ADVISOR_STAFF_UNHAPPY_GUARDS, 0x1c4)
ADV_WORD(_ADVISOR_STAFF_UNHAPPY_ENTERTAINERS, 0x1c8)
ADV_WORD(_ADVISOR_STAFF_UNHAPPY_RESEARCHERS, 0x1cc)
ADV_WORD(_ADVISOR_STAFF_VERY_UNHAPPY_MECHANICS, 0x1d0)
ADV_WORD(_ADVISOR_STAFF_VERY_UNHAPPY_HANDYMEN, 0x1d4)
ADV_WORD(_ADVISOR_STAFF_VERY_UNHAPPY_GUARDS, 0x1d8)
ADV_WORD(_ADVISOR_STAFF_VERY_UNHAPPY_ENTERTAINERS, 0x1dc)
ADV_WORD(_ADVISOR_STAFF_VERY_UNHAPPY_RESEARCHERS, 0x1e0)
ADV_WORD(_ADVISOR_STAFF_STRIKING_MECHANICS, 0x1e4)
ADV_WORD(_ADVISOR_STAFF_STRIKING_HANDYMEN, 0x1e8)
ADV_WORD(_ADVISOR_STAFF_STRIKING_GUARDS, 0x1ec)
ADV_WORD(_ADVISOR_STAFF_STRIKING_ENTERTAINERS, 0x1f0)
ADV_WORD(_ADVISOR_STAFF_STRIKING_RESEARCHERS, 0x1f4)
ADV_WORD(_ADVISOR_STAFF_HAPPIER_MECHANICS, 0x1f8)
ADV_WORD(_ADVISOR_STAFF_HAPPIER_HANDYMEN, 0x1fc)
ADV_WORD(_ADVISOR_STAFF_HAPPIER_GUARDS, 0x200)
ADV_WORD(_ADVISOR_STAFF_HAPPIER_ENTERTAINERS, 0x204)
ADV_WORD(_ADVISOR_STAFF_HAPPIER_RESEARCHERS, 0x208)
ADV_WORD(_ADVISOR_STAFF_STRIKE_END_BAD_MECHANICS, 0x20c)
ADV_WORD(_ADVISOR_STAFF_STRIKE_END_BAD_HANDYMEN, 0x210)
ADV_WORD(_ADVISOR_STAFF_STRIKE_END_BAD_GUARDS, 0x214)
ADV_WORD(_ADVISOR_STAFF_STRIKE_END_BAD_ENTERTAINERS, 0x218)
ADV_WORD(_ADVISOR_STAFF_STRIKE_END_BAD_RESEARCHERS, 0x21c)

// Unit v5973: six advisor scorers (0x10013dd8, 0x10014c00, 0x10014d54, 0x10015168, 0x10017528, 0x10017b2c).

namespace {
constexpr int32_t SlotOf(int32_t macOffset) { return (macOffset - 0x14) / 4; }
constexpr int64_t kTicksPerDay = 864000000000LL;      // 0xc92a69c000
constexpr int64_t kTicksPerMonth = 25920000000000LL;  // 0x17 92f8648000
}  // namespace

// 0x10013dd8: rides available, enough days since the last build (0x634), enough rides (0x224) and a share of ride types below 0x220 percent
// (unsigned): the value at 0x228.
int32_t CAdvisorScoring::_ADVISOR_RIDES_POOR_SELECTION() {
    CParkAnalyser* a = gGameWorldRef->GetParkAnalyser();
    const uint32_t available = static_cast<uint32_t>(a->TypesOfRidesAvailable(false));
    if (available == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) < mBalance.mValues[SlotOf(0x224)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x220)].mInteger))
        return mBalance.mValues[SlotOf(0x228)].mInteger;
    return 0;
}

// Unit v5939: nine more scorers of the advisor (rides, shops, sideshows). The Mac offsets of the balance values they read are in the comments;
// the balance table's slot s lies at Mac offset 0x14 + 4 * s of the scoring object.

namespace {

constexpr int64_t kDayTicks = 864000000000LL;           // 0xc9 2a69c000: the 100 ns ticks of a day
constexpr int64_t kThirtyDayTicks = 25920000000000LL;   // 0x1792 f8648000

constexpr CThing::ThingModel kResearcher = static_cast<CThing::ThingModel>(8);

inline CParkAnalyser* Analyser_2() { return gGameWorldRef->GetParkAnalyser(); }

}  // namespace

// 0x10013f28: with a researcher, rides in the park, rides left to research, no research speed on category 0 and every available ride type
// built: the value at 0x22c.
int32_t CAdvisorScoring::_ADVISOR_RIDES_RESEARCH_MORE() {
    if (Analyser_2()->NumberOfStaffType(kResearcher) == 0) return 0;
    const int32_t built = Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    if (built == 0) return 0;
    if (Analyser_2()->TypesOfRidesLeftToResearch() == 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(0)) != 0) return 0;
    if (built != Analyser_2()->TypesOfRidesAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x22c)].mInteger;
}

// 0x10014078: no ride built (golden tickets counted): 0x230 + 0x234 * the whole days since the session start (low 32 bits of the quotient).
int32_t CAdvisorScoring::_ADVISOR_RIDES_BUILD_RIDES() {
    if (Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjRide, true) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kDayTicks);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x230)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x234)].mInteger) * static_cast<uint32_t>(days));
}

// 0x10014160: at least 0x238 periods of 30 days since the session start, a researcher, no upgrade available and no research speed on category 4: 0x23c.
int32_t CAdvisorScoring::_ADVISOR_RIDES_NO_UPGRADES() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t periods = static_cast<int32_t>(since.mTicks / kThirtyDayTicks);
    if (periods < mBalance.mValues[SlotOf(0x238)].mInteger) return 0;
    if (Analyser_2()->NumberOfStaffType(kResearcher) == 0) return 0;
    if (Analyser_2()->TypesOfUpgradesAvailable(false) > 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(4)) != 0) return 0;
    return mBalance.mValues[SlotOf(0x23c)].mInteger;
}

// 0x100142a4: a researcher, upgrades available but at most 0x240 of them, no research speed on category 4: 0x244.
int32_t CAdvisorScoring::_ADVISOR_RIDES_MORE_UPGRADES() {
    if (Analyser_2()->NumberOfStaffType(kResearcher) == 0) return 0;
    const int32_t upgrades = Analyser_2()->TypesOfUpgradesAvailable(false);
    if (upgrades == 0 || mBalance.mValues[SlotOf(0x240)].mInteger < upgrades) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(4)) != 0) return 0;
    return mBalance.mValues[SlotOf(0x244)].mInteger;
}

ADV_WORD(_ADVISOR_RIDES_BREAKDOWN_IMMINENT, 0x248)
ADV_WORD(_ADVISOR_RIDES_BREAKDOWN_NO_MECHANICS, 0x24c)
ADV_WORD(_ADVISOR_RIDES_BREAKDOWN_BUSY_MECHANICS, 0x250)

int32_t CAdvisorScoring::_ADVISOR_RIDES_LONG_QUEUES_BORED_VISITORS() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_RIDES_USE_UPGRADES() { return 0; }

int32_t CAdvisorScoring::_ADVISOR_RIDES_BREAKDOWN_MECHANIC_ON_IT() { return Bal(this, 0x254); }   // 0x10014500

// 0x10014558: shops available, enough days since the last build (0x634), enough shops (0x25c) and a share of shop types below 0x258 percent
// (unsigned): the value at 0x260.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_POOR_SELECTION() {
    const uint32_t available = static_cast<uint32_t>(Analyser_2()->TypesOfShopsAvailable(false));
    if (available == 0) return 0;
    if (Analyser_2()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (Analyser_2()->NumberOfObjectTypeInPark(kUIObjShop, 0) < mBalance.mValues[SlotOf(0x25c)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjShop, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x258)].mInteger))
        return mBalance.mValues[SlotOf(0x260)].mInteger;
    return 0;
}

// 0x100146a8: like RIDES_RESEARCH_MORE for shops (category 1, value at 0x264).
int32_t CAdvisorScoring::_ADVISOR_SHOPS_RESEARCH_MORE() {
    if (Analyser_2()->NumberOfStaffType(kResearcher) == 0) return 0;
    const int32_t built = Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjShop, false);
    if (built == 0) return 0;
    if (Analyser_2()->TypesOfShopsLeftToResearch() == 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(1)) != 0) return 0;
    if (built != Analyser_2()->TypesOfShopsAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x264)].mInteger;
}

// 0x100147f8: rides built, enough days since the last build, no shop built: 0x268 + 0x26c * the whole days since the session start.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_BUILD_SHOPS() {
    if (Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjRide, false) == 0) return 0;
    if (Analyser_2()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjShop, false) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kDayTicks);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x268)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x26c)].mInteger) * static_cast<uint32_t>(days));
}

// 0x1001493c: shops built, enough days since the last build, the shop count (as float) below the second count times the float at 0x270 (a NaN
// answers 0), and a balance above 0x274: the value at 0x278.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_BUILD_MORE_SHOPS() {
    const int32_t first = Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjShop, false);
    if (first == 0) return 0;
    if (Analyser_2()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    const int32_t second = Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjShop, false);
    const float a = static_cast<float>(first);
    const float limit = static_cast<float>(second) * mBalance.mValues[SlotOf(0x270)].mFloat;
    if (!(a < limit)) return 0;   // bge tests the LT bit, which a NaN leaves clear
    if (gGameWorldRef->GetBankAccount()->GetBalance() <= mBalance.mValues[SlotOf(0x274)].mInteger) return 0;
    return mBalance.mValues[SlotOf(0x278)].mInteger;
}

// 0x10014aac: like SHOPS_POOR_SELECTION for sideshows (percent 0x27c, count 0x280, value 0x284).
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_POOR_SELECTION() {
    const uint32_t available = static_cast<uint32_t>(Analyser_2()->TypesOfSideshowsAvailable(false));
    if (available == 0) return 0;
    if (Analyser_2()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (Analyser_2()->NumberOfObjectTypeInPark(kUIObjSideshow, 0) < mBalance.mValues[SlotOf(0x280)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(Analyser_2()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x27c)].mInteger))
        return mBalance.mValues[SlotOf(0x284)].mInteger;
    return 0;
}

// 0x10014c00: a researcher (staff 8), sideshow types built, some left to research, research speed 0 and every available type built: 0x288.
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_RESEARCH_MORE() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(8)) == 0) return 0;
    const int32_t built = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false);
    if (built == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfSideshowsLeftToResearch() == 0) return 0;
    if (gGameWorldRef->GetResearchLab()->GetResearchSpeed(static_cast<ResearchCat>(2)) != 0) return 0;
    if (built != gGameWorldRef->GetParkAnalyser()->TypesOfSideshowsAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x288)].mInteger;
}

// 0x10014d54: shops built, enough days since the last build (0x634), no sideshows: 0x28c + 0x290 * (whole days since the session start, low word).
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_BUILD_SIDESHOWS() {
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjShop, false) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kTicksPerDay);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf(0x28c)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf(0x290)].mInteger) * static_cast<uint32_t>(days));
}

// Unit v5081: five advisor scorers about toilets, the choice of features and the sideshows (CAdvisorScoring, main app).
// The balance values are read by their Mac offset in the scoring object; the host's slots are 8 bytes, so the slot is (offset - 0x14) / 4.

// The four toilet ids the scorers compare the filthiest toilet with (never written in the original: zero).
uint16_t gAdvisorNoToilet1 = 0;   // 0x1027defc
uint16_t gAdvisorNoToilet2 = 0;   // 0x1027defe
uint16_t gAdvisorNoToilet3 = 0;   // 0x1027df00
uint16_t gAdvisorNoToilet4 = 0;   // 0x1027df02

// 0x10014ea0: sideshows exist, enough days since the last build (0x634), sideshow types fewer than the ride types times the float at
// 0x294, and a balance above 0x298: the value at 0x29c.
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS() {
    const int32_t sideshows = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false);
    if (sideshows == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    const int32_t rides = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjRide, false);
    const float rideCount = static_cast<float>(rides);
    const float sideshowCount = static_cast<float>(sideshows);
    const float limit = rideCount * mBalance.mValues[SlotOf(0x294)].mFloat;
    if (!(sideshowCount < limit)) return 0;
    if (gGameWorldRef->GetBankAccount()->GetBalance() > mBalance.mValues[SlotOf(0x298)].mInteger)
        return mBalance.mValues[SlotOf(0x29c)].mInteger;
    return 0;
}
// 0x10015018: with features available, enough days since the last build (0x634), enough feature objects (0x2a4) and a share of feature
// types below 0x2a0 percent (unsigned): the value at 0x2a8.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_POOR_SELECTION() {
    const uint32_t available = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->TypesOfFeaturesAvailable(false));
    if (available == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjFeature, 0) < mBalance.mValues[SlotOf(0x2a4)].mInteger) return 0;
    const uint32_t types = static_cast<uint32_t>(gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjFeature, false));
    if (types * 100u / available < static_cast<uint32_t>(mBalance.mValues[SlotOf(0x2a0)].mInteger))
        return mBalance.mValues[SlotOf(0x2a8)].mInteger;
    return 0;
}

// 0x10015168: a researcher, feature types built, some left to research and every available type built: 0x2ac.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_RESEARCH_MORE() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(8)) == 0) return 0;
    const int32_t built = gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjFeature, false);
    if (built == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfFeaturesLeftToResearch() == 0) return 0;
    if (built != gGameWorldRef->GetParkAnalyser()->TypesOfFeaturesAvailable(false)) return 0;
    return mBalance.mValues[SlotOf(0x2ac)].mInteger;
}

int32_t CAdvisorScoring::_ADVISOR_FEATURES_FAR_TO_TOILETS() { return Bal(this, 0x2c4); }          // 0x10015514

// 0x10015628: no score with the park closed or a staff type 5 present, or the filthiest toilet being the "none" id, or the mean filth
// outside [0x2d0, 0x2e0]; else the mean times the value at 0x2d4.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_1() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) > 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet1) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2d0)].mInteger) return 0;
    if (average > mBalance.mValues[SlotOf(0x2e0)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2d4)].mInteger;
}

// 0x10015798: the same, but with staff type 5 absent giving 0; limits 0x2d8 and 0x2e8, factor 0x2d4.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_2() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) == 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet2) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2d8)].mInteger) return 0;
    if (average > mBalance.mValues[SlotOf(0x2e8)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2d4)].mInteger;
}

// 0x10015908: staff type 5 present gives 0; lower limit 0x2e0 only, factor 0x2e4.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_3() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) > 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet3) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2e0)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2e4)].mInteger;
}

// 0x10015a64: staff type 5 absent gives 0; lower limit 0x2e8 only, factor 0x2ec.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_DIRTY_TOILETS_4() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) == 0) return 0;
    int32_t filth;
    mWord4 = gGameWorldRef->GetParkAnalyser()->FilthiestToilet(filth).mIndex;
    if (mWord4 == gAdvisorNoToilet4) return 0;
    const int32_t average = gGameWorldRef->GetParkAnalyser()->AvgFilthLevelOfToilets();
    if (average < mBalance.mValues[SlotOf(0x2e8)].mInteger) return 0;
    return average * mBalance.mValues[SlotOf(0x2ec)].mInteger;
}

int32_t CAdvisorScoring::_ADVISOR_FEATURES_BUILD_REST_AREA() { return 0; }          // 0x10015bc0
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_RIDE_RESEARCHED() { return Bal(this, 0x300); }         // 0x10015dc0
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_ADDON_RESEARCHED() { return Bal(this, 0x304); }        // 0x10015e14
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_SHOP_RESEARCHED() { return Bal(this, 0x308); }         // 0x10015e68
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_SIDESHOW_RESEARCHED() { return Bal(this, 0x30c); }     // 0x10015ebc
int32_t CAdvisorScoring::_ADVISOR_RESEARCH_FEATURE_RESEARCHED() { return Bal(this, 0x310); }      // 0x10015f14

// Unit v5252: eight advisor scorers about the bank account and the park analyser (main app, 0x10015f68 .. 0x10016a3c).

namespace {

// BeenInTheRedFor() / 0x1792f8648000 (the runtime's 64-bit divide, 0x10237e1c); the answer's low word is tested.
inline int32_t MonthsInTheRed() {
    TbTimeDiff d = gGameWorld->GetBankAccount()->BeenInTheRedFor();
    return static_cast<int32_t>(d.mTicks / 0x1792F8648000LL);
}

// The shared head of the in-the-red scorers: in the red with no loan available.
inline bool RedWithoutLoan() {
    if (gGameWorld->GetBankAccount()->GetBalance() >= 0) return false;
    return gGameWorld->GetBankAccount()->AnyLoanAvailable() == 0;
}

}  // namespace

int32_t CAdvisorScoring::_ADVISOR_GENERAL_LAX_SECURITY() {   // 0x10015f68
    if (gGameWorld->GetParkAnalyser()->MisbehavingVisitorsLastMonth(true) > Bal_2(this, 0x314))
        return Bal_2(this, 0x318) * gGameWorld->GetParkAnalyser()->MisbehavingVisitorsLastMonth(true);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_GENERAL_WON_GOLDEN_TICKET() { return Bal(this, 0x31c); }        // 0x10016038
int32_t CAdvisorScoring::_ADVISOR_AWARD_GIVEN() { return Bal(this, 0x320); }                      // 0x1001608c
int32_t CAdvisorScoring::_ADVISOR_AWARD_MOVED() { return Bal(this, 0x324); }                      // 0x100160d0

// 0x1001010c: the vptr store (Mac +4) has no host equivalent.
CAdvisorBalance* CAdvisorBalanceDestruct(CAdvisorBalance* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->UnInit();
        if (deleteFlag > 0) v51116_callee::OperatorDelete(self);
    }
    return self;
}

int32_t CAdvisorScoring::_ADVISOR_WEALTH_HIGH() {   // 0x10016160
    if (gGameWorld->GetBankAccount()->GetBalance() > Bal_2(this, 0x330)) {
        if (gGameWorld->GetBankAccount()->GetNumberOfLoansBought() == 0) return Bal_2(this, 0x334);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_TAKE_LOAN() {   // 0x1001636c
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && gGameWorld->GetBankAccount()->AnyLoanAvailable())
        return Bal_2(this, 0x340);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_IN_THE_RED_SIX_MONTHS() {   // 0x10016420
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && !gGameWorld->GetBankAccount()->AnyLoanAvailable()) {
        if (MonthsInTheRed() < 3) return Bal_2(this, 0x344);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_IN_THE_RED_THREE_MONTHS() {   // 0x10016524
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && !gGameWorld->GetBankAccount()->AnyLoanAvailable()) {
        int32_t months = MonthsInTheRed();
        if (months >= 3 && months < 5) return Bal_2(this, 0x348);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_IN_THE_RED_MONTH_LEFT() {   // 0x10016630
    if (gGameWorld->GetBankAccount()->GetBalance() < 0 && !gGameWorld->GetBankAccount()->AnyLoanAvailable()) {
        if (MonthsInTheRed() == 5) return Bal_2(this, 0x34c);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_LOTS_HANDYMAN() {   // 0x100168e8
    int32_t limit = Bal_2(this, 0x360) + gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(5)) > limit)
        return Bal_2(this, 0x364);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_LOTS_ENTERTAINERS() {   // 0x100169b4
    int32_t limit = Bal_2(this, 0x368) + gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(6)) > limit)
        return Bal_2(this, 0x36c);
    return 0;
}

// Unit v5108: the eight "max staff" advisor scorers (CAdvisorScoring::_ADVISOR_MAX_*, 0x10016e68 .. 0x10017460); unit v5L21 added the
// ninth, _ADVISOR_MAX_MECHANICS (0x100170b0), and _ADVISOR_BOAT_ARRIVED (0x10017ae4).
// Each compares a staff-limit slot of gMainBalance (Mac +0x2a8 .. +0x2b8) with a slot of the advisor's own balance table, then asks the park
// analyser how many staff of one type there are. The Mac offsets in the comments are of the 4-byte slots; the host's slots are wider.

namespace {

// The advisor table's value at the Mac offset `offset` of the scoring object (the table is at 0x0c, its values 8 on).
int32_t AdvisorSlot(CAdvisorScoring* self, int32_t offset) { return self->mBalance.mValues[(offset - 0x14) / 4].mInteger; }

// gMainBalance's value at the Mac offset `offset` (values start at 8).
int32_t MainSlot(int32_t offset) { return gMainBalance.mValues[(offset - 8) / 4].mInteger; }

int32_t StaffCount(int32_t model) {
    return gGameWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(model));
}

}  // namespace

// 0x10016e68
int32_t CAdvisorScoring::_ADVISOR_MAX_HANDYMEN_V2() {
    if (MainSlot(0x2a8) == AdvisorSlot(this, 0x38c)) return 0;
    if (StaffCount(5) == MainSlot(0x2a8)) return AdvisorSlot(this, 0x394);
    return 0;
}

// 0x10016f2c
int32_t CAdvisorScoring::_ADVISOR_MAX_ENTERTAINERS() {
    if (MainSlot(0x2b0) != AdvisorSlot(this, 0x398)) return 0;
    if (StaffCount(6) < AdvisorSlot(this, 0x398)) return 0;
    return AdvisorSlot(this, 0x39c);
}

// 0x10016fe8
int32_t CAdvisorScoring::_ADVISOR_MAX_ENTERTAINERS_V2() {
    if (MainSlot(0x2b0) == AdvisorSlot(this, 0x398)) return 0;
    if (StaffCount(6) == MainSlot(0x2b0)) return AdvisorSlot(this, 0x3a0);
    return 0;
}

// 0x100170b0
int32_t CAdvisorScoring::_ADVISOR_MAX_MECHANICS() {
    if (MainSlot(0x2ac) != AdvisorSlot(this, 0x3a4)) return 0;
    if (StaffCount(4) < AdvisorSlot(this, 0x3a4)) return 0;
    return AdvisorSlot(this, 0x3a8);
}

// 0x10017168
int32_t CAdvisorScoring::_ADVISOR_MAX_MECHANICS_V2() {
    if (MainSlot(0x2ac) == AdvisorSlot(this, 0x3a4)) return 0;
    if (StaffCount(4) == MainSlot(0x2ac)) return AdvisorSlot(this, 0x3ac);
    return 0;
}

// 0x10017230
int32_t CAdvisorScoring::_ADVISOR_MAX_GUARDS() {
    if (MainSlot(0x2b4) != AdvisorSlot(this, 0x3b0)) return 0;
    if (StaffCount(7) < AdvisorSlot(this, 0x3b0)) return 0;
    return AdvisorSlot(this, 0x3b4);
}

// 0x100172e4
int32_t CAdvisorScoring::_ADVISOR_MAX_GUARDS_V2() {
    if (MainSlot(0x2b4) == AdvisorSlot(this, 0x3b0)) return 0;
    if (StaffCount(7) == MainSlot(0x2b4)) return AdvisorSlot(this, 0x3b8);
    return 0;
}

// 0x100173a8 (the guard on the balance slot reads 0x3b0, the threshold 0x3bc: as in the listing)
int32_t CAdvisorScoring::_ADVISOR_MAX_RESEARCHERS() {
    if (MainSlot(0x2b4) != AdvisorSlot(this, 0x3b0)) return 0;
    if (StaffCount(8) < AdvisorSlot(this, 0x3bc)) return 0;
    return AdvisorSlot(this, 0x3c0);
}

// 0x10017460
int32_t CAdvisorScoring::_ADVISOR_MAX_RESEARCHERS_V2() {
    if (MainSlot(0x2b8) == AdvisorSlot(this, 0x3bc)) return 0;
    if (StaffCount(8) == MainSlot(0x2b8)) return AdvisorSlot(this, 0x3c4);
    return 0;
}

// 0x10017528: young park (months below 0x3c8, the low word of the quotient, signed) and the history (analyser + 0x216d8) fell by more than 0x3cc percent.
int32_t CAdvisorScoring::_ADVISOR_HYPE_NEWPARK() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t months = static_cast<int32_t>(since.mTicks / kTicksPerMonth);
    if (gGameWorld->GetParkAnalyser()->mHistory216D8.HowFarBack() <= 1) return 0;
    if (months >= mBalance.mValues[SlotOf(0x3c8)].mInteger) return 0;
    int32_t latest;
    int32_t previous;
    if (!gGameWorld->GetParkAnalyser()->mHistory216D8.GetValue(latest, 0)) return 0;
    if (!gGameWorld->GetParkAnalyser()->mHistory216D8.GetValue(previous, 1)) return 0;
    if (previous == 0) return 0;
    if (latest >= previous) return 0;
    const int32_t n = static_cast<int32_t>(static_cast<uint32_t>(latest) * 100u);
    const int32_t q = previous == -1 ? static_cast<int32_t>(0u - static_cast<uint32_t>(n)) : n / previous;
    if (100 - q > mBalance.mValues[SlotOf(0x3cc)].mInteger) return mBalance.mValues[SlotOf(0x3d0)].mInteger;
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_PRANK_SBOMB() { return mBalance.mValues[SlotOf(0x3d4)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PRANK_LITTER() { return mBalance.mValues[SlotOf(0x3d8)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PRANK_BALLOON() { return mBalance.mValues[SlotOf(0x3dc)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PARK_NOW_OPEN() { return mBalance.mValues[SlotOf(0x3e0)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PARK_NOW_CLOSED() { return mBalance.mValues[SlotOf(0x3e4)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_UPGRADE_HIRE_MECHANIC() { return mBalance.mValues[SlotOf(0x3e8)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_UPGRADE_STIKING_MECHANICS() { return mBalance.mValues[SlotOf(0x3ec)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_UPGRADE_NO_MECHANIC() { return mBalance.mValues[SlotOf(0x3f0)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_RIDE() { return mBalance.mValues[SlotOf(0x3f4)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_SHOP() { return mBalance.mValues[SlotOf(0x3f8)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_SIDESHOW() { return mBalance.mValues[SlotOf(0x3fc)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_NEW_RESEARCH_GROUP_FEATURE() { return mBalance.mValues[SlotOf(0x400)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_BUS_ARRIVED() { return mBalance.mValues[SlotOf(0x404)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_PLANE_ARRIVED() { return mBalance.mValues[SlotOf(0x408)].mInteger; }

// 0x10017ae4: the advisor table's value at 0x40c, nothing else.
int32_t CAdvisorScoring::_ADVISOR_BOAT_ARRIVED() { return AdvisorSlot(this, 0x40c); }

// 0x10017b2c: the latest history value above 0x410: remembers the bus stop B cell in mWord8 and answers 0x414.
int32_t CAdvisorScoring::_ADVISOR_BIG_BUSLOAD_ARRIVED() {
    if (gGameWorld->GetParkAnalyser()->mHistory216D8.HowFarBack() <= 0) return 0;
    int32_t latest;
    if (!gGameWorld->GetParkAnalyser()->mHistory216D8.GetValue(latest, 0)) return 0;
    if (latest <= mBalance.mValues[SlotOf(0x410)].mInteger) return 0;
    const auto stop = gGameWorldRef->GetMap()->GetBusStopPos(1);
    mWord8 = stop.mIndex;
    return mBalance.mValues[SlotOf(0x414)].mInteger;
}

// Unit v5892: twelve advisor scorers (0x10017c40 .. 0x1001b2b8). The balance values are read at Mac offsets from this
// (slot = (offset - 0x14) / 4, the table's values start at 0x14).

namespace {

inline CThing::ThingModel Model_3(int32_t n) { return static_cast<CThing::ThingModel>(n); }

// The shared shape of the five "trained" scorers: staff exist (the HQ word), more than a count, quality above a limit.
inline int32_t Trained(CAdvisorScoring* self, uint32_t hqWord, int32_t model, const BalanceValue& count,
                       const BalanceValue& quality, const BalanceValue& answer) {
    CStaffHQ* hq = gGameWorldRef->GetStaffHQ();
    if (!(static_cast<int32_t>(hq->mTraining.mF00[hqWord]) > 0)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->NumberOfStaffType(Model_3(model)) > count.mInteger)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->AverageQualityOfStaffType(Model_3(model)) > quality.mInteger)) return 0;
    (void)self;
    return answer.mInteger;
}

}  // namespace

#define MAC(off) (mBalance.mValues[((off) - 0x14) / 4])

// 0x10017c40: gates open, enough rides (count at 0x41c), average excitement above 0x418; the scariest ride is stored at +4.
int32_t CAdvisorScoring::_ADVISOR_VISITORS_TOO_SCARED() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0) < MAC(0x41c).mInteger) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->GetAverageExcitementLevelOfAllRides(10) > MAC(0x418).mInteger)) return 0;
    gGameWorldRef->GetParkAnalyser()->GetScariestRide(
        reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>&>(mWord4));
    return MAC(0x420).mInteger;
}

int32_t CAdvisorScoring::_ADVISOR_ONLINE_NEW_ITEM() { return mBalance.mValues[SlotOf(0x424)].mInteger; }
int32_t CAdvisorScoring::_ADVISOR_ONLINE_MESSAGE() { return mBalance.mValues[SlotOf(0x428)].mInteger; }

// 0x10017df4
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_STAFF_HAPPY() {
    if (gGameWorld->GetParkAnalyser()->NumberOfStaff() < MAC(0x42c).mInteger) return 0;
    const float happiness = gGameWorld->GetParkAnalyser()->GetStaffHappinessLevel(0);
    if (happiness >= static_cast<float>(MAC(0x430).mInteger)) return MAC(0x434).mInteger;
    return 0;
}

// 0x10017edc: these two read the world through gGameWorld itself (TOC 0x1037ebfc), not through the reference.
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_VISITORS_HAPPY() {
    if (gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark() < MAC(0x438).mInteger) return 0;
    const float happiness = gGameWorld->GetParkAnalyser()->GetHappinessLevel(0);
    if (happiness >= static_cast<float>(MAC(0x43c).mInteger)) return MAC(0x440).mInteger;
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_SECURITY_TRAINED() {   // 0x10017fc8
    return Trained(this, 3, 7, MAC(0x444), MAC(0x448), MAC(0x44c));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_RESEARCHERS_TRAINED() {   // 0x100180c0
    return Trained(this, 4, 8, MAC(0x450), MAC(0x454), MAC(0x458));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_HANDYMEN_TRAINED() {   // 0x100181b8
    return Trained(this, 0, 5, MAC(0x45c), MAC(0x460), MAC(0x464));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_ENTERTAINERS_TRAINED() {   // 0x100182b0
    return Trained(this, 2, 6, MAC(0x468), MAC(0x46c), MAC(0x470));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_MECHANICS_TRAINED() {   // 0x100183ac
    return Trained(this, 1, 4, MAC(0x474), MAC(0x478), MAC(0x47c));
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_BIG_COASTER() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FUN_COASTER() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FUN_GOKART() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FUN_WATERRIDE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_FAST_GOKART() { return 0; }

// 0x10018624
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PUBLISH_PARK() {
    if (!(gGameWorldRef->GetParkAnalyser()->GetParkValue(10) > MAC(0x480).mInteger)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark() > MAC(0x488).mInteger)) return 0;
    const float happiness = gGameWorldRef->GetParkAnalyser()->GetHappinessLevel(10);
    if (static_cast<float>(MAC(0x484).mInteger) < happiness) return MAC(0x48c).mInteger;
    return 0;
}

// 0x1001873c
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_SEND_POSTCARD() {
    if (!(gGameWorldRef->GetParkAnalyser()->GetParkValue(10) > MAC(0x490).mInteger)) return 0;
    if (!(gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark() > MAC(0x498).mInteger)) return 0;
    const float happiness = gGameWorldRef->GetParkAnalyser()->GetHappinessLevel(10);
    if (static_cast<float>(MAC(0x494).mInteger) < happiness) return MAC(0x49c).mInteger;
    return 0;
}

// 0x10018854
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_BALLOONS() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsWithBalloons() > MAC(0x4a0).mInteger) return MAC(0x4a4).mInteger;
    return 0;
}

// Eight advisor scorers (unit v5282): the staff and costume scorers 0x100188f4..0x10018f4c and the three poor value pricing scorers
// 0x100192a4..0x100194b4. Mac offsets of the balance values read are in the calls of Bal (a Mac offset in the scoring object).

uint16_t gAdvisorNoneFoodPoorValue = 0;         // 0x1027df06
uint16_t gAdvisorNoneShopsPoorValue = 0;        // 0x1027df08
uint16_t gAdvisorNoneRestaurantPoorValue = 0;   // 0x1027df0a

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_COSTUMES() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsWithCostumes() > Bal(this, 0x4a8)) return Bal(this, 0x4ac);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GIFTS() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_NONE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_JUNGLE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_HALLOWEEN() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_SPACE() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_GOLDEN_KEY_FANTASY() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_KEY_AT_START() { return 0; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_ALL_COMPLETE() { return 0x2710; }
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_ALL_COMPLETE_LEAVING() { return 0x270f; }

int32_t CAdvisorScoring::_ADVISOR_NEED_STAFF_ROOM() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    if (static_cast<int32_t>(since.mTicks / 0x1792F8648000LL) < 3) return 0;
    const int32_t tired = gGameWorldRef->GetParkAnalyser()->NumberOfStaffTooTiredToWork();
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfStaff();
    if (total == 0) return 0;
    if (tired * 100 / total > Bal(this, 0x4b8)) return Bal(this, 0x4bc);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_HIRE_STAFF_GENERAL() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsSinceBeginning() < Bal(this, 0x4c4)) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaff() == 0) return Bal(this, 0x4c0);
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_SET_PATROLS_GENERAL() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaff() > Bal(this, 0x4c8)) {
        if (gGameWorldRef->GetParkAnalyser()->NumberOfStaffTypeWithPatrolArea(static_cast<CThing::ThingModel>(0)) == 0)
            return Bal(this, 0x4cc);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_TRAIN_STAFF_GENERAL() {
    if (gGameWorldRef->GetParkAnalyser()->NumberOfStaff() > Bal(this, 0x4d0)) {
        if (gGameWorldRef->GetStaffHQ()->mTraining.mF14 == 0) return Bal(this, 0x4d4);
    }
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_FOOD_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = 0;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(static_cast<CObjectShopType>(2), id);
    if (id.mIndex == gAdvisorNoneFoodPoorValue) return 0;
    if (figure >= Bal(this, 0x4d8)) return 0;
    CTRACE(0x26, "Food poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Bal(this, 0x4dc) * (figure - Bal(this, 0x4d8));
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_SHOPS_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = 0;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(id);
    if (id.mIndex == gAdvisorNoneShopsPoorValue) return 0;
    if (figure >= Bal(this, 0x4e0)) return 0;
    CTRACE(0x26, "shops poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Bal(this, 0x4e4) * (figure - Bal(this, 0x4e0));
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_RESTAURANT_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = 0;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(static_cast<CObjectShopType>(3), id);
    if (id.mIndex == gAdvisorNoneRestaurantPoorValue) return 0;
    if (figure >= Bal(this, 0x4e8)) return 0;
    CTRACE(0x26, "restaurants poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Bal(this, 0x4ec) * (figure - Bal(this, 0x4e8));
}

int32_t CAdvisorScoring::_ADVISOR_PRICING_DRINKS_POOR_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstValue(static_cast<CObjectShopType>(4), id);
    if (id.mIndex == gAdvisorNoneDrinksPoorValue) return 0;
    if (figure >= Slot(this, 0x4f0).mInteger) return 0;
    CTRACE(0x26, "drinks poor value (%d)\n", figure);
    mWord4 = id.mIndex;
    mWord8 = figure;
    return Slot(this, 0x4f4).mInteger * (figure - Slot(this, 0x4f0).mInteger);
}

// Unit v5256: eight more advisor scorers (pricing satisfaction / value, gold ticket near to X peeps).
// The balance slot of a Mac offset N is (N - 0x14) / 4 (the table's values start at 0x14 in the scoring object).

// "no thing" ids, zero in the original's data
uint16_t gAdvisorNoneSideshowsPoorValue = 0;       // 0x1027df0e
uint16_t gAdvisorNoneFoodPoorSat = 0;              // 0x1027df10
uint16_t gAdvisorNoneShopsPoorSat = 0;             // 0x1027df12
uint16_t gAdvisorNoneRestaurantPoorSat = 0;        // 0x1027df14
uint16_t gAdvisorNoneDrinksPoorSat = 0;            // 0x1027df16
uint16_t gAdvisorNoneSideshowsPoorSat = 0;         // 0x1027df18
uint16_t gAdvisorNoneDrinksGreatSat = 0;           // 0x1027df2a

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;

int32_t Slot_3(CAdvisorScoring* self, int32_t slot) { return self->mBalance.mValues[slot].mInteger; }

// The common tail of the POOR scorers: limit at slot, multiplier at slot + 1.
int32_t Poor(CAdvisorScoring* self, int32_t figure, const ThingId& id, uint16_t none, int32_t slot, const char* text) {
    if (id.mIndex == none) return 0;
    if (figure < Slot_3(self, slot)) {
        CTRACE(0x26, text, figure);
        self->mWord4 = id.mIndex;
        self->mWord8 = static_cast<uint32_t>(figure);
        return Slot_3(self, slot + 1) * (figure - Slot_3(self, slot));
    }
    return 0;
}

}  // namespace

// 0x100196d0
int32_t CAdvisorScoring::_ADVISOR_PRICING_SIDESHOWS_POOR_VALUE() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetSideshowWithWorstValue(id);
    return Poor(this, figure, id, gAdvisorNoneSideshowsPoorValue, 313, "sideshows poor value (%d)\n");   // 0x4f8, 0x4fc
}
// 0x100197dc
int32_t CAdvisorScoring::_ADVISOR_PRICING_FOOD_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(static_cast<CObjectShopType>(2), id);
    return Poor(this, figure, id, gAdvisorNoneFoodPoorSat, 315, "Food poor satisfaction (%d)\n");   // 0x500, 0x504
}

// 0x100198ec
int32_t CAdvisorScoring::_ADVISOR_PRICING_SHOPS_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(id);
    return Poor(this, figure, id, gAdvisorNoneShopsPoorSat, 317, "shops poor satisfaction (%d)\n");   // 0x508, 0x50c
}

// 0x100199f8
int32_t CAdvisorScoring::_ADVISOR_PRICING_RESTAURANT_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(static_cast<CObjectShopType>(3), id);
    return Poor(this, figure, id, gAdvisorNoneRestaurantPoorSat, 319, "restaurant poor satisfaction (%d)\n");   // 0x510, 0x514
}

// 0x10019b10
int32_t CAdvisorScoring::_ADVISOR_PRICING_DRINKS_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithWorstSatisfaction(static_cast<CObjectShopType>(4), id);
    return Poor(this, figure, id, gAdvisorNoneDrinksPoorSat, 321, "drinks poor satisfaction (%d)\n");   // 0x518, 0x51c
}

// 0x10019c24
int32_t CAdvisorScoring::_ADVISOR_PRICING_SIDESHOWS_POOR_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetSideshowWithWorstSatisfaction(id);
    return Poor(this, figure, id, gAdvisorNoneSideshowsPoorSat, 323, "sideshows poor satisfaction (%d)\n");   // 0x520, 0x524
}

// Unit v5122: the CONGRAT_PRICING great value / great satisfaction scorers (main app). Each asks the park analyser for its best thing,
// and when that is not the "none" id and the figure beats the balance limit, records it in the object (halfword 4, word 8) and answers
// multiplier * (figure - limit).

uint16_t gAdvisorNoneFoodValue = 0;         // 0x1027df1a
uint16_t gAdvisorNoneShopsValue = 0;        // 0x1027df1c
uint16_t gAdvisorNoneRestaurantValue = 0;   // 0x1027df1e
uint16_t gAdvisorNoneDrinksValue = 0;       // 0x1027df20
uint16_t gAdvisorNoneSideshowsValue = 0;    // 0x1027df22
uint16_t gAdvisorNoneFoodSat = 0;           // 0x1027df24
uint16_t gAdvisorNoneShopsSat = 0;          // 0x1027df26
uint16_t gAdvisorNoneRestaurantSat = 0;     // 0x1027df28

namespace {

// The balance value at a Mac offset of the scoring object (the values start at 0x18 there, four bytes each).
int32_t Slot_4(const CAdvisorScoring* self, int32_t macOffset) { return self->mBalance.mValues[(macOffset - 0x18) / 4].mInteger; }

}  // namespace

#define TPW_CONGRAT_FINISH(NONE, OFFSET)                      \
    if (id.mIndex == (NONE)) return 0;                        \
    if (figure > Slot_4(this, OFFSET)) {                        \
        mWord4 = id.mIndex;                                   \
        mWord8 = static_cast<uint32_t>(figure);               \
        return Slot_4(this, (OFFSET) + 4) * (figure - Slot_4(this, OFFSET)); \
    }                                                         \
    return 0;

// 0x10019d34
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(static_cast<CObjectShopType>(2), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneFoodValue, 0x528)
}

// 0x10019e28
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(id);
    TPW_CONGRAT_FINISH(gAdvisorNoneShopsValue, 0x530)
}

// 0x10019f18
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(static_cast<CObjectShopType>(3), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneRestaurantValue, 0x538)
}

// 0x1001a010
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestValue(static_cast<CObjectShopType>(4), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneDrinksValue, 0x540)
}

// 0x1001a104
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_VALUE() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetSideshowWithBestValue(id);
    TPW_CONGRAT_FINISH(gAdvisorNoneSideshowsValue, 0x548)
}

// 0x1001a1f8
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_FOOD_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(static_cast<CObjectShopType>(2), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneFoodSat, 0x550)
}

// 0x1001a2f0
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(id);
    TPW_CONGRAT_FINISH(gAdvisorNoneShopsSat, 0x558)
}

// 0x1001a3e8
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(static_cast<CObjectShopType>(3), id);
    TPW_CONGRAT_FINISH(gAdvisorNoneRestaurantSat, 0x560)
}

// 0x1001a4e8
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_SATISFACTION() {
    ThingId id;
    const int32_t figure = gGameWorldRef->GetParkAnalyser()->GetShopWithBestSatisfaction(static_cast<CObjectShopType>(4), id);
    if (id.mIndex == gAdvisorNoneDrinksGreatSat) return 0;
    if (figure > Slot_3(this, 341)) {   // 0x568
        mWord4 = id.mIndex;
        mWord8 = static_cast<uint32_t>(figure);
        return Slot_3(this, 342) * (figure - Slot_3(this, 341));   // 0x56c
    }
    return 0;
}

// AdvisorScoring_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.cpp (main app of SimTheme Park 2.04b3): eight of CAdvisorScoring's scorers
// (unit v5040), kept apart from src/AdvisorScoring.cpp so the unit libraries that compile that file need none of their callees.

// Unit v5040: eight scorers. Mac offset o of the scoring object holds balance slot (o - 0x14) / 4 (the table's values start at 0x14).

uint16_t gSideshowNoneId = 0;   // 0x1027df2c (initial word 0): the "no thing" id GetSideshowWithBestSatisfaction answers

namespace {
int32_t ScoringSlot(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4].mInteger;
}
}  // namespace

// 0x1001a5e4: records the best sideshow in the object (halfword 4, word 8) when its satisfaction is above the limit.
int32_t CAdvisorScoring::_ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION() {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    const int32_t satisfaction = gGameWorldRef->GetParkAnalyser()->GetSideshowWithBestSatisfaction(id);
    if (id.mIndex == gSideshowNoneId) return 0;
    const int32_t limit = ScoringSlot(this, 0x570);
    if (satisfaction > limit) {
        mWord4 = id.mIndex;
        mWord8 = static_cast<uint32_t>(satisfaction);
        return ScoringSlot(this, 0x574) * (satisfaction - limit);
    }
    return 0;
}

// 0x1001a728: rides are UI type 0; only with the gates open and at least the minimum number of rides.
int32_t CAdvisorScoring::_ADVISOR_RIDES_CLOSED() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(static_cast<UIObjType>(0), 0);
    if (total == 0) return 0;
    if (total < ScoringSlot(this, 0x57c)) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(0));
    if (closed * 100 / total > ScoringSlot(this, 0x580)) return ScoringSlot(this, 0x584);
    return 0;
}

// 0x1001a84c: shops are UI type 1.
int32_t CAdvisorScoring::_ADVISOR_SHOPS_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(static_cast<UIObjType>(1), 0);
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(1));
    if (closed * 100 / total > ScoringSlot(this, 0x588)) return ScoringSlot(this, 0x58c);
    return 0;
}

// 0x1001a934: sideshows are UI type 2.
int32_t CAdvisorScoring::_ADVISOR_SIDESHOWS_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfObjectTypeInPark(static_cast<UIObjType>(2), 0);
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedObjectTypeInPark(static_cast<UIObjType>(2));
    if (closed * 100 / total > ScoringSlot(this, 0x590)) return ScoringSlot(this, 0x594);
    return 0;
}

// 0x1001aa20.
int32_t CAdvisorScoring::_ADVISOR_TOILETS_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfToiletsInPark();
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedToiletsInPark();
    if (closed * 100 / total > ScoringSlot(this, 0x598)) return ScoringSlot(this, 0x59c);
    return 0;
}

// 0x1001aafc.
int32_t CAdvisorScoring::_ADVISOR_STAFFROOM_CLOSED() {
    const int32_t total = gGameWorldRef->GetParkAnalyser()->NumberOfStaffRoomsInPark();
    if (total == 0) return 0;
    const int32_t closed = gGameWorldRef->GetParkAnalyser()->NumberOfClosedStaffRoomsInPark();
    if (closed * 100 / total > ScoringSlot(this, 0x5a0)) return ScoringSlot(this, 0x5a4);
    return 0;
}

// 0x1001abdc.
int32_t CAdvisorScoring::_ADVISOR_LOTS_OF_PRANKS() {
    const int32_t pranks = gGameWorldRef->GetParkAnalyser()->MisbehavingVisitorsLastMonth(false);
    if (pranks > ScoringSlot(this, 0x5a8)) return ScoringSlot(this, 0x5ac);
    return 0;
}

#define ZERO_SCORER(name) \
    int32_t CAdvisorScoring::name() { return 0; }
#define SLOT_SCORER(name, off) \
    int32_t CAdvisorScoring::name() { return static_cast<int32_t>(mBalance.mValues[((off) - 0x14) / 4].mInteger); }

SLOT_SCORER(_ADVISOR_GENERAL_WON_FIRST_GOLDEN_TICKET, 0x5b4)  // 0x1001adf8
ZERO_SCORER(_ADVISOR_WELCOME_NAMEENTRY)                // 0x1001adac
ZERO_SCORER(_ADVISOR_GENERAL_BLOWUP_ADVISOR_1)         // 0x1001ae50
ZERO_SCORER(_ADVISOR_WEATHER_FUNNY_STORM)              // 0x1001ad60
ZERO_SCORER(_ADVISOR_GENERAL_BLOWUP_ADVISOR_2)         // 0x1001aea4
ZERO_SCORER(_ADVISOR_WEATHER_FUNNY_SNOW)               // 0x1001ad14
ZERO_SCORER(_ADVISOR_WEATHER_FUNNY_RAIN)               // 0x1001acc8
ZERO_SCORER(_ADVISOR_RIDE_EXIT_BLOCKED)                // 0x1001ac7c

// 0x1001aef8: no points once a ticket is held; else visitors above (float)balance[0x750] * factor (truncated).
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE() {
    CGoldenTicketControl* control = reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<char*>(gGameWorld) + 0x2c0);
    if (control->HowManyTickets() > 0) return 0;
    const int32_t visitors = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
    const float scaled = static_cast<float>(gMainBalance.mValues[(0x750 - 8) / 4].mInteger) * mBalance.mValues[(0x5b8 - 0x14) / 4].mFloat;
    if (visitors > ppc_fctiwz(static_cast<double>(scaled))) return ScoringSlot(this, 0x5bc);
    return 0;
}

SLOT_SCORER(_ADVISOR_GOLD_TICKET_CLUES_FOR_REMAINDER, 0x5c0)  // 0x1001affc

// 0x1001b054
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XPEEPS() {
    const int32_t visitors = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
    // fsubs / fmuls / fctiwz: the balance integer (slot 466 of the main table, Mac +0x750) as a float times the float at 0x5c4
    const float product = static_cast<float>(gMainBalance.mValues[466].mInteger) * mBalance.mValues[364].mFloat;
    if (visitors > static_cast<int32_t>(product)) {
        if (gMasterSave == nullptr) {
            CFSGlobalMasterSave* made = static_cast<CFSGlobalMasterSave*>(::operator new(0x4c));
            if (made != nullptr) new (made) CFSGlobalMasterSave();
            gMasterSave = made;
        }
        if (!gMasterSave->HaveTicket(static_cast<GoldenTicketTypeLocal>(0))) return Slot_3(this, 365);   // 0x5c8
    }
    return 0;
}

// 0x1001b18c: happiness above (main balance 0x758 as float) * (float at 0x5cc), more than the visitors of 0x75c, no ticket 2 yet.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XHAPPINESS() {
    const float happiness = gGameWorldRef->GetParkAnalyser()->GetHappinessLevel(0);
    const float product = static_cast<float>(gMainBalance.mValues[467].mInteger) * MAC(0x5cc).mFloat;
    const float limit = static_cast<float>(ppc_fctiwz(product));
    if (!(happiness > limit)) return 0;
    const int32_t visitors = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark();
    if (!(visitors > gMainBalance.mValues[468].mInteger)) return 0;
    if (gMasterSave == nullptr) {
        CFSGlobalMasterSave* made = static_cast<CFSGlobalMasterSave*>(::operator new(0x4c));
        if (made != nullptr) new (made) CFSGlobalMasterSave();
        gMasterSave = made;
    }
    if (!gMasterSave->HaveTicket(static_cast<GoldenTicketTypeLocal>(2))) return MAC(0x5d0).mInteger;
    return 0;
}

int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT() {
    const int32_t profit = gGameWorldRef->GetBankAccount()->GetProfitThisYear();
    const float scaled = static_cast<float>(gMainBalance.mValues[(0x760 - 8) / 4].mInteger) * Slot(this, 0x5d4).mFloat;
    if (profit > ppc_fctiwz(static_cast<double>(scaled))) {
        if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
        if (!gMasterSave->HaveTicket(static_cast<GoldenTicketTypeLocal>(4))) return Slot(this, 0x5d8).mInteger;
    }
    return 0;
}

SLOT_SCORER(_ADVISOR_OBJECTIVE_MET_OFFERDOUBLE, 0x62c)        // 0x1001b444
SLOT_SCORER(_ADVISOR_OBJECTIVE_MET_OFFERDOUBLE_2, 0x62c)      // 0x1001b498
ZERO_SCORER(_ADVISOR_CONGRAT_GOLDEN_KEY_ANYPARK)       // 0x1001b4ec
SLOT_SCORER(_ADVISOR_RIDE_CONDEMNED, 0x578)            // 0x1001a6e0

int32_t CAdvisorScoring::_ADVISOR_TUTORIAL_UI_PANEL() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t day = static_cast<int32_t>(since.mTicks / 864000000000LL);
    if (day > Slot(this, 0x5dc).mInteger && day < Slot(this, 0x5e0).mInteger) return Slot(this, 0x5e4).mInteger;
    return 0;
}

// CAdvisorScoring scorers (main app, unit v5106): the gold ticket amount scorers, the gold ticket explanation and the tooltip tutorial.
// Mac offsets of the balance values read: see SlotOf (Mac offset of a scoring-object slot -> mBalance.mValues index).

// 0x1001b604 (128 bytes): the day number (the time since the session start over 864000000000 ticks, the low word of the 64-bit quotient)
// strictly between the values at 0x5e8 and 0x5ec gives the value at 0x5f0.
int32_t CAdvisorScoring::_ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t day = static_cast<int32_t>(since.mTicks / 864000000000LL);
    if (day > mBalance.mValues[SlotOf(0x5e8)].mInteger && day < mBalance.mValues[SlotOf(0x5ec)].mInteger)
        return mBalance.mValues[SlotOf(0x5f0)].mInteger;
    return 0;
}

// 0x1001b6d0 (92 bytes): the value at 0x5f4 while the analyser's history at +0x216d8 holds exactly one value.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_EXPLANATION() {
    if (gGameWorld->GetParkAnalyser()->mHistory216D8.HowFarBack() == 1) return mBalance.mValues[SlotOf(0x5f4)].mInteger;
    return 0;
}

ZERO_SCORER(_ADVISOR_CASH_LOW)                         // 0x1001b774
ZERO_SCORER(_ADVISOR_CASH_LOW_TAKE_LOAN)               // 0x1001b7b8
ZERO_SCORER(_ADVISOR_PEEPS_STUCK_BREAKDOWN)            // 0x1001b804
ZERO_SCORER(_ADVISOR_PEEPS_STUCK_CONDEMNED)            // 0x1001b854
SLOT_SCORER(_ADVISOR_PREBUILT_PARK, 0x5f8)             // 0x1001b8a4

// 0x1001b8ec (136 bytes): park open and the percentage of golden ticket objects researched in (0x5fc, 0x604] gives 0x600.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfTotalObjectsInPark();
    if (percent > mBalance.mValues[SlotOf(0x5fc)].mInteger && percent <= mBalance.mValues[SlotOf(0x604)].mInteger)
        return mBalance.mValues[SlotOf(0x600)].mInteger;
    return 0;
}

// 0x1001b9d0 (136 bytes): (0x604, 0x60c] gives 0x608.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_b() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfTotalObjectsInPark();
    if (percent > mBalance.mValues[SlotOf(0x604)].mInteger && percent <= mBalance.mValues[SlotOf(0x60c)].mInteger)
        return mBalance.mValues[SlotOf(0x608)].mInteger;
    return 0;
}

// 0x1001bab4 (124 bytes): above 0x60c gives 0x610.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_c() {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t percent = gGameWorld->GetParkAnalyser()->PercentageOfTotalObjectsInPark();
    if (percent > mBalance.mValues[SlotOf(0x60c)].mInteger) return mBalance.mValues[SlotOf(0x610)].mInteger;
    return 0;
}

namespace {

// The shared body of the three VISITORS scorers (identical code apart from the two slots): the visitors in the park as a percentage of
// the main balance value at 0x754 (zero: 0), clamped to 100 (the original asks the analyser a second time when it does not exceed 100).
int32_t VisitorsAmount(const CAdvisorScoring* self, int32_t thresholdSlot, int32_t amountSlot) {
    if (gGameWorldRef->IsParkClosed()) return 0;
    const int32_t* total = &gMainBalance.mValues[(0x754 - 8) / 4].mInteger;
    if (*total == 0) return 0;
    int32_t percent;
    if (gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark() * 100 / *total > 100) {
        percent = 100;
    } else {
        percent = gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark() * 100 / *total;
    }
    if (percent > self->mBalance.mValues[thresholdSlot].mInteger) return self->mBalance.mValues[amountSlot].mInteger;
    return 0;
}

}  // namespace

// 0x1001bb8c (228 bytes): thresholds 0x614 / amount 0x618.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a() { return VisitorsAmount(this, SlotOf(0x614), SlotOf(0x618)); }

// 0x1001bcc0 (228 bytes): 0x61c / 0x620.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_b() { return VisitorsAmount(this, SlotOf(0x61c), SlotOf(0x620)); }

// 0x1001bdf4 (228 bytes): 0x624 / 0x628.
int32_t CAdvisorScoring::_ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_c() { return VisitorsAmount(this, SlotOf(0x624), SlotOf(0x628)); }

SLOT_SCORER(_ADVISOR_CHALLENGE, 0x62c)                 // 0x1001bf28
ZERO_SCORER(_ADVISOR_WELCOME_LOST_WORLD)               // 0x10019258
ZERO_SCORER(_ADVISOR_WELCOME_SPACE)                    // 0x10019210
ZERO_SCORER(_ADVISOR_WELCOME_HALLOWEEN)                // 0x100191c4
ZERO_SCORER(_ADVISOR_WELCOME_FANTASY)                  // 0x1001917c
ZERO_SCORER(_ADVISOR_WELCOME_VETERAN)                  // 0x10019134
ZERO_SCORER(_ADVISOR_WELCOME_ENTER_NAME)               // 0x100190e8
ZERO_SCORER(_ADVISOR_WELCOME_NO_NAME)                  // 0x100190a0
ZERO_SCORER(_ADVISOR_WELCOME_NEWBIE)                   // 0x10019058
ZERO_SCORER(_ADVISOR_BUY_MORE_LAND)                    // 0x10019010
SLOT_SCORER(_ADVISOR_BANKRUPTED, 0x350)                // 0x10016734
SLOT_SCORER(_ADVISOR_TUTORIAL_MESSAGE, 0x328)          // 0x10016114

// Unit v5896: twelve more advisor scorers (pricing, wealth, wages, staff counts, queues, toilets, features).
// Mac offsets of the balance values read are in the calls of Bal (a Mac offset in the scoring object).

uint16_t gAdvisorNoRideQueue;   // 0x1027df04

namespace {

// The value slot at a Mac offset of the scoring object (the balance table starts at 0x0c, its values at 0x14, slot 0 reserved).
inline int32_t SlotOf_2(int32_t macOffset) { return (macOffset - 0x14) / 4; }

// 100 ns ticks in a day and in a month (the original divides with the 64-bit runtime routine at 0x10237e1c and keeps the low word).
constexpr int64_t kTicksPerMonth_2 = 0x1792F8648000LL;
constexpr int64_t kTicksPerDay_2 = 0xC92A69C000LL;

// CHistory<int,144>::GetAverageOfLast (0x1001c064) leaves the count it averaged in r3, and the callers test it; the translation of that
// callee answers nothing, so the count is taken from GetSumOfLast and the mean made as the callee makes it.
inline int32_t AverageOfLast(CHistory<int32_t, 144>& history, int32_t& out, int32_t count) {
    double sum;
    const int32_t n = history.GetSumOfLast(sum, count);
    if (n > 0) out = static_cast<int32_t>(ppc_fctiwz(sum / static_cast<double>(n)));
    else out = 0;
    return n;
}

}  // namespace

// 0x10015298: sideshows built, enough days since the last build, no features built: 0x2b0 plus 0x2b4 per day since the session started.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_BUILD_FEATURES() {
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjSideshow, false) == 0) return 0;
    if (gGameWorldRef->GetParkAnalyser()->GetDaysSinceLastObjectBuilt() < mBalance.mValues[SlotOf_2(0x634)].mInteger) return 0;
    if (gGameWorldRef->GetParkAnalyser()->TypesOfParticularObjectTypeInPark(kUIObjFeature, false) != 0) return 0;
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t days = static_cast<int32_t>(since.mTicks / kTicksPerDay_2);
    return static_cast<int32_t>(static_cast<uint32_t>(mBalance.mValues[SlotOf_2(0x2b0)].mInteger) +
                                static_cast<uint32_t>(mBalance.mValues[SlotOf_2(0x2b4)].mInteger) * static_cast<uint32_t>(days));
}
// 0x100153e0: gates open, no toilets, and a share of visitors (percent) needing one above 0x2bc: 0x2c0.
int32_t CAdvisorScoring::_ADVISOR_FEATURES_NO_TOILETS() {
    if (!gGameWorldRef->GetParkAnalyser()->GatesOpen()) return 0;
    if (gGameWorldRef->GetParkAnalyser()->NumberOfToiletsInPark() != 0) return 0;
    const int32_t needing = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsNeedingToilet(
        static_cast<uint8_t>(mBalance.mValues[SlotOf_2(0x2b8)].mInteger));
    const int32_t inPark = gGameWorldRef->GetParkAnalyser()->NumberOfVisitorsInPark();
    if (inPark == 0) return 0;
    const int32_t percent = static_cast<int32_t>(static_cast<uint32_t>(needing) * 100u) / inPark;
    if (percent > mBalance.mValues[SlotOf_2(0x2bc)].mInteger) return mBalance.mValues[SlotOf_2(0x2c0)].mInteger;
    return 0;
}

// 0x10015564: the average toilet queue is read and compared with 0x2c8, but both ways of the branch answer 0 (the original's bug).
int32_t CAdvisorScoring::_ADVISOR_FEATURES_NOT_ENOUGH_TOILETS() {
    if (gGameWorldRef->IsParkClosed() != 0) return 0;
    (void)gGameWorldRef->GetParkAnalyser()->GetAverageToiletQueueLenght();
    return 0;
}

// 0x10015c14: a complete set of opinions of the tickets and an average below 0x2f0: 0x2f4.
int32_t CAdvisorScoring::_ADVISOR_PRICING_TICKET_TOO_EXPENSIVE() {
    if (gGameWorld->GetParkAnalyser()->HaveCompleteSetOfOpinions(0)) {
        const int32_t average = gGameWorld->GetParkAnalyser()->GetAverageOpinionOn(0);
        if (average < mBalance.mValues[SlotOf_2(0x2f0)].mInteger) return mBalance.mValues[SlotOf_2(0x2f4)].mInteger;
    }
    return 0;
}

// 0x10015cec: a complete set of opinions of the tickets and an average above 0x2f8: 0x2fc.
int32_t CAdvisorScoring::_ADVISOR_PRICING_TICKET_TOO_CHEAP() {
    if (gGameWorld->GetParkAnalyser()->HaveCompleteSetOfOpinions(0)) {
        const int32_t average = gGameWorld->GetParkAnalyser()->GetAverageOpinionOn(0);
        if (average > mBalance.mValues[SlotOf_2(0x2f8)].mInteger) return mBalance.mValues[SlotOf_2(0x2fc)].mInteger;
    }
    return 0;
}

// 0x10016218: balance minus the loans outstanding, above the multiplier (0x338) times what the cheapest loan still owes: 0x33c.
int32_t CAdvisorScoring::_ADVISOR_WEALTH_HIGH_REPAY_LOAN() {
    const int32_t balance = gGameWorldRef->GetBankAccount()->GetBalance();
    const uint32_t total = gGameWorldRef->GetBankAccount()->TotalOutstandingOnLoans();
    const int32_t spare = static_cast<int32_t>(static_cast<uint32_t>(balance) - total);
    const int32_t cheapest = static_cast<int32_t>(gGameWorldRef->GetBankAccount()->GetCheapestLoanTaken());
    if (cheapest < 0) return 0;
    const uint32_t owed = gGameWorldRef->GetBankAccount()->OutstandingOnLoan(static_cast<uint32_t>(cheapest));
    const float limit = mBalance.mValues[SlotOf_2(0x338)].mFloat * static_cast<float>(owed);
    if (static_cast<float>(spare) > limit) return mBalance.mValues[SlotOf_2(0x33c)].mInteger;
    return 0;
}

// 0x10016778: at least 3 months into the session, the mean of the wages history (0x1f7f0) over the last 0x358 values above the mean of the
// other history (0x1fc90) times 0x354: 0x35c.
int32_t CAdvisorScoring::_ADVISOR_WAGES_HIGH() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t months = static_cast<int32_t>(since.mTicks / kTicksPerMonth_2);
    if (months < 3) return 0;
    int32_t wages;
    int32_t other;
    if (AverageOfLast(gGameWorld->GetParkAnalyser()->mHistory1F100[3], wages, mBalance.mValues[SlotOf_2(0x358)].mInteger) == 0) return 0;
    if (AverageOfLast(gGameWorld->GetParkAnalyser()->mHistory1F100[5], other, mBalance.mValues[SlotOf_2(0x358)].mInteger) == 0) return 0;
    const float limit = static_cast<float>(other) * mBalance.mValues[SlotOf_2(0x354)].mFloat;
    if (static_cast<float>(wages) > limit) return mBalance.mValues[SlotOf_2(0x35c)].mInteger;
    return 0;
}

// 0x10016a84: more mechanics than 0x370 plus the number of rides: 0x374.
int32_t CAdvisorScoring::_ADVISOR_LOTS_MECHANICS() {
    const int32_t limit = mBalance.mValues[SlotOf_2(0x370)].mInteger +
                          gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model_3(4)) > limit) return mBalance.mValues[SlotOf_2(0x374)].mInteger;
    return 0;
}

// 0x10016b50: more guards than 0x378 plus the number of rides: 0x37c.
int32_t CAdvisorScoring::_ADVISOR_LOTS_SECURITY() {
    const int32_t limit = mBalance.mValues[SlotOf_2(0x378)].mInteger +
                          gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model_3(7)) > limit) return mBalance.mValues[SlotOf_2(0x37c)].mInteger;
    return 0;
}

// 0x10016c1c: more researchers than 0x380 plus the number of rides: 0x384.
int32_t CAdvisorScoring::_ADVISOR_LOTS_RESEARCHERS() {
    const int32_t limit = mBalance.mValues[SlotOf_2(0x380)].mInteger +
                          gGameWorld->GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model_3(8)) > limit) return mBalance.mValues[SlotOf_2(0x384)].mInteger;
    return 0;
}

// 0x10016cec: a ride with an unconnected queue (kept in mWord4): 0x388.
int32_t CAdvisorScoring::_ADVISOR_QUEUE_UNCONNECTED() {
    mWord4 = gGameWorld->GetParkAnalyser()->FindRideWithUnconnectedQueue(false).mIndex;
    if (mWord4 != gAdvisorNoRideQueue) return mBalance.mValues[SlotOf_2(0x388)].mInteger;
    return 0;
}

// 0x10016db0: when gMainBalance's word at 0x2a8 equals 0x38c and as many handymen as that: 0x390.
int32_t CAdvisorScoring::_ADVISOR_MAX_HANDYMEN() {
    const int32_t limit = mBalance.mValues[SlotOf_2(0x38c)].mInteger;
    if (gMainBalance.mValues[(0x2a8 - 0x8) / 4].mInteger != limit) return 0;
    if (gGameWorld->GetParkAnalyser()->NumberOfStaffType(Model_3(5)) >= mBalance.mValues[SlotOf_2(0x38c)].mInteger)
        return mBalance.mValues[SlotOf_2(0x390)].mInteger;
    return 0;
}

// Static initialiser of AdvisorScoring.cpp (0x1001c34c): constructs the file's global string (0x102b4110), registers it for
// destruction, then copies the 351 scorer pointers to member (12 bytes each on the Mac, from the sources 0x1027df30 + 12 * k) into
// the scorer slot (+0x10) of record k of the message table (0x1027efa4 + 0x30 * k).

alignas(8) unsigned char gAdvisorStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x102b4110
unsigned char gAdvisorStringNode[8];                                                                            // 0x102b4124 (the registration node)

// The text at 0x1023c570.
static const char16_t kAdvisorStringText[] = u"gms.dat";

void DestroyAdvisorString(void* object, int16_t) {
    static_cast<TbDynamicStringTemplate<char16_t>*>(object)->~TbDynamicStringTemplate();
}

void __sinit_AdvisorScoring_cpp() {
    auto* text = new (gAdvisorStringStorage) TbDynamicStringTemplate<char16_t>(kAdvisorStringText);
    __register_global_object(text, reinterpret_cast<void*>(&DestroyAdvisorString), gAdvisorStringNode);
    for (int32_t k = 0; k < kAdvisorMessageCount; k++) gAdvisorMessages[k].mScorer = gAdvisorScorerSources[k];
}

// CPeep (Peep.cpp, Person.cpp): the destructor, the sprite members and FinalDestroy. The peep is reached by Mac offset.

// The all-staff panel's row maker AllStaffLCFormat (LCFormat 0x101991e8), its attach function AllStaffLCPostFormat (LCPostFormat
// 0x10199370) and the panel's button handler AllStaffPanelLCHandler 0x10198d54.

// Unit v51065: the online lists' small members, the park lists' GetList for the top ten and the by-park-id lists, the login callbacks,
// two destructors (the city list's and the theme list's base), the park instance of LoggedInGetList and the FTP download getters.

// PlaceHolderTypes.cpp: the kinds of playing sound built on CAudioPlaceHolder (unit w79g3, "sound symbols shared", the library's
// CSprocketsModule.cpp): CPlaceHolderOneShot, CPlaceHolderOneShotSentence and ...BranchingSentence, CPlaceHolderSentence and its
// Linear, Branching, Shuffle and CListSentence subclasses. See include/PlaceHolderTypes.h for the class tree, the vtable slots and the
// extra data's +0x1c word.
//
// Conventions of this file. The listing's operation order is kept (the call logs of the test compare it). A virtual call the Mac makes
// through the object's vtable is a virtual call here; a call the Mac makes by address (the base's functions) is a qualified call.
// Mac addresses of the sound data that the code compares or subtracts are compared as arena offsets (the 32-bit value a Ptr32 field
// holds), because the Mac does that arithmetic on 32-bit addresses. The objects are constructed in place into the list's slot
// (operator new(0x60, slot) returns the slot); every constructor leaves the list's words alone. The destructors' vtable stores and
// the base call with flag 0 are the compiler's chain; the flag-1 deleting form is `delete`.

// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.

_TbSoundSampleWAV* _TbSoundSampleWAV::mFirst_TbSoundSampleWAV = nullptr;
int32_t _TbSoundSampleWAV::mCurrent_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLow_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mHigh_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mTotal_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLevel_TbSoundSampleWAV = 10;

_TbSoundSampleWAVFile* _TbSoundSampleWAVFile::mFirst_TbSoundSampleWAVFile = nullptr;
int32_t _TbSoundSampleWAVFile::mCurrent_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLow_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mHigh_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mTotal_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLevel_TbSoundSampleWAVFile = 10;

_TbSoundCodecLoop* _TbSoundCodecLoop::mFirst_TbSoundCodecLoop = nullptr;
int32_t _TbSoundCodecLoop::mCurrent_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mLow_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mHigh_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mTotal_TbSoundCodecLoop = 0;
int32_t _TbSoundCodecLoop::mLevel_TbSoundCodecLoop = 10;

TbTimerCodec* TbTimerCodec::mFirstTbTimerCodec = nullptr;
int32_t TbTimerCodec::mCurrentTbTimerCodec = 0;
int32_t TbTimerCodec::mLowTbTimerCodec = 0;
int32_t TbTimerCodec::mHighTbTimerCodec = 0;
int32_t TbTimerCodec::mTotalTbTimerCodec = 0;
int32_t TbTimerCodec::mLevelTbTimerCodec = 10;

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

TbSoundUserCallback* TbSoundUserCallback::mFirstTbSoundUserCallback = nullptr;
int32_t TbSoundUserCallback::mCurrentTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLowTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mHighTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mTotalTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLevelTbSoundUserCallback = 10;

_TbSoundSampleCallback* _TbSoundSampleCallback::mFirst_TbSoundSampleCallback = nullptr;
int32_t _TbSoundSampleCallback::mCurrent_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLow_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mHigh_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mTotal_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLevel_TbSoundSampleCallback = 10;

_TbSoundSampleCompletedCallback* _TbSoundSampleCompletedCallback::mFirst_TbSoundSampleCompletedCallback = nullptr;
int32_t _TbSoundSampleCompletedCallback::mCurrent_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mLow_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mHigh_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mTotal_TbSoundSampleCompletedCallback = 0;
int32_t _TbSoundSampleCompletedCallback::mLevel_TbSoundSampleCompletedCallback = 10;

// The sound library's MPEG sample codecs (unit w80b, program "sound symbols shared"): _TbSoundSampleMPEG2
// (TbSoundSampleMPEG2.cpp) and _TbSoundSampleMPEG2File (StreamableMpeg2.cpp). See include/TbSoundSampleMPEG2.h for the layouts
// and the slot map.

_TbSoundSampleMPEG2* _TbSoundSampleMPEG2::mFirst_TbSoundSampleMPEG2 = nullptr;
int32_t _TbSoundSampleMPEG2::mCurrent_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mLow_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mHigh_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mTotal_TbSoundSampleMPEG2 = 0;
int32_t _TbSoundSampleMPEG2::mLevel_TbSoundSampleMPEG2 = 10;

_TbSoundSampleMPEG2File* _TbSoundSampleMPEG2File::mFirst_TbSoundSampleMPEG2File = nullptr;
int32_t _TbSoundSampleMPEG2File::mCurrent_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mLow_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mHigh_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mTotal_TbSoundSampleMPEG2File = 0;
int32_t _TbSoundSampleMPEG2File::mLevel_TbSoundSampleMPEG2File = 10;

uint8_t _TbSoundSampleMPEG2::sSetInfoScratch[0x300];
uint8_t _TbSoundSampleMPEG2::sRestartScratch[0x300];

// The UI timers' removal (main app, UI toolkit 0x101d8500 and 0x101d8a00; unit w51): InterfaceTimer::FindTimer and UI_RemoveTimer.
// The table, the list and AddTimer are src/UI_pump.cpp and src/UI_init.cpp.
