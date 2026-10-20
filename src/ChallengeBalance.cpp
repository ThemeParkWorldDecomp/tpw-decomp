// CChallengeBalance (main app, unit w177): the table of data:challenges.sam that CChallengeHQ::Init builds on its stack. The destructor 0x100f6acc (100 bytes)
// and the two virtuals GetBalanceDescription 0x100f9318 (16) and GetBalanceValuesStart 0x100f9368 (8), and the description table at 0x1028e908.
#include "ChallengeBalance.h"

// Row 0 opens the array group (type 2, no name), rows 1..10 are its integer members, row 11 closes it (type 3, named Challenges, sixty elements), row 12
// ends the table. Every range and flag is zero: nothing is range checked.
const BalanceDescription gChallengeBalanceDescriptions[13] = {
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Type", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "FollowupType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetTime", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetVal", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetObj", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetObj2", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "TargetStaffType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Prize", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CheckAtEndOnly", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Independent", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "Challenges", 0, 0, 0.0f, 0.0f, 60, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CChallengeBalance::GetBalanceDescription(int32_t row) { return gChallengeBalanceDescriptions + row; }
BalanceValue* CChallengeBalance::GetBalanceValuesStart() { return mValues; }

// 0x100f6acc (100 bytes). The Mac's destructor, when `this` is not NULL, stores the base's vtable word (0x102a08a8, TOC -0x6a48) at +4, calls
// CBalanceTable::UnInit and, when the halfword flag it is given is above 0, operator delete(this); it answers `this`. The NULL test, the vptr
// store and the delete are the compiler's (the host's destructor and delete expression); the body is the UnInit.
CChallengeBalance::~CChallengeBalance() { UnInit(); }
