// CChallengeHQ::Init, main app 0x100f6904 (412 bytes; Entertainer.cpp in the inventory, probably ChallengeHQ.cpp), unit w123b. The description table of
// challenges.sam that its CChallengeBalance reads is in src/ChallengeBalance.cpp (unit w177).
#include <cstddef>
#include <new>

#include "ChallengeBalance.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ThingReceivers.h"
#include "analyser.h"

// Loads the level's challenge descriptions and offers the ones the level asks for. Only the twenty type words of the challenge slots are cleared first
// (no other state of the HQ is touched). A local CChallengeBalance (the Mac builds its table by hand: the base's vptr, then the derived one, then
// CBalanceTable::Init) loads data:challenges.sam fresh; a failed load only traces (the text is the original's, with no final newline) and carries on
// with whatever the table holds. Then for each of the twenty signed row choices of the main balance (slot 483 on; the count slot 503 is not read), a
// choice <= 0 is skipped and a positive one j selects the row of table slots 1 + 10 * j .. 10 + 10 * j, with no test against the sixty rows or the
// loaded count. The row's columns, in the order of the descriptions: 0 Type, 1 FollowupType, 2 TargetTime, 3 TargetVal, 4 TargetObj, 5 TargetObj2,
// 6 TargetStaffType, 7 Prize, 8 CheckAtEndOnly, 9 Independent. The two object ids are the low 16 bits of columns 4 and 5, the two flags are "!= 0".
// One CChallenge is built over the SAME stack storage each time (never default constructed first, as the Mac's temporary) with the parameter
// constructor (a stand-in) and handed to AddChallenge (a stand-in). After the loop ClearInvalidFollowups runs once, and the table's destructor (the Mac inlines it:
// the derived vptr put back, then UnInit) runs at the end of the scope.
void CChallengeHQ::Init() {
    for (int32_t i = 0; i < 0x14; ++i) mChallenges[i].mType = 0;
    CChallengeBalance table;
    table.Init();
    if (table.Load("data:challenges.sam", CBalanceTable::kLoadFresh) == 0) {
        CTRACE(0, "!!!!!\nLoading challenges.sam went horribly wrong! Shout at Bjarne!\n!!!!!");
    }
    alignas(CChallenge) unsigned char temporary[sizeof(CChallenge)];
    for (int32_t i = 0; i < kMainBalanceChallengesInThisLevelCount; ++i) {
        const int32_t choice = gMainBalance.mValues[kMainBalanceChallengesInThisLevel + i].mInteger;
        if (choice <= 0) continue;
        const BalanceValue* row = table.mValues + 1 + static_cast<std::ptrdiff_t>(choice) * 10;
        const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj{static_cast<uint16_t>(row[4].mInteger)};
        const tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj2{static_cast<uint16_t>(row[5].mInteger)};
        CChallenge* challenge = new (temporary) CChallenge(
            static_cast<ChallengeType>(row[0].mInteger), row[2].mInteger, row[7].mInteger, static_cast<ChallengeType>(row[1].mInteger),
            row[8].mInteger != 0, row[3].mInteger, targetObj, targetObj2, static_cast<StaffType>(row[6].mInteger), row[9].mInteger != 0);
        AddChallenge(*challenge);
    }
    ClearInvalidFollowups();
}
