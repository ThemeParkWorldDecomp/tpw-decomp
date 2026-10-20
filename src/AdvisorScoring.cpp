// AdvisorScoring.cpp (main app of SimTheme Park 2.04b3): the advisor's message scoring, 0x10010054..0x10010f38 and the destructor at
// 0x1000adb0 (the inventory files it under Advisor.cpp). The constructor loads the advisor's balance table; the ten queries look a
// message up in the static record table (src/AdvisorScoring_table.cpp) and answer one of its fields, or a number of the message's category in
// the balance table, or run the record's scorer. The scorers themselves (the _ADVISOR_* members) are not here.
//
// Every query repeats the original's inlined lookup of a message's record: the two numbers kAdvisorMessageCount and kAdvisorNoMessage
// have none; otherwise the record at the message's own index is taken if its number matches, else all 351 records are searched, and a
// hit there STILL answers the record at the original index (the code adds the index it started with, not the one it found, to the
// table). With the table in order the search cannot hit; a patched table (the tests) shows it. A miss traces "Not found: %d\n" at level 3.
#include "AdvisorScoring.h"

#include "Debug.h"
#include "FS_Online.h"

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

// 0x1000adb0: puts the derived vptr back and releases the table (the deleting form, with a positive flag, frees the object: the caller).
CAdvisorScoring::~CAdvisorScoring() {
    mBalance.UnInit();
}

// 0x1001019c: -1 for no record, then -1 when the message is not valid in the current game, else the scorer's answer.
int32_t CAdvisorScoring::GetScoreFor(AdvisorMessage message) {
    const AdvisorMessageRecord* record = FindMessage(message);
    if (record == nullptr) return -1;
    if (!IsThisValidInTheCurrentMode(message)) return -1;
    return (this->*record->mScorer)();
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
