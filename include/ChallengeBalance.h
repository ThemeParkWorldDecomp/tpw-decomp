// ChallengeBalance.h: CChallengeBalance, the table of data:challenges.sam that CChallengeHQ::Init builds on its stack (unit w123b).
// Mac size 0x970: mRows at +0, the vptr at +4, then 602 four-byte value slots from +8. Slot 0 is reserved, slots 1..600 are the sixty rows of ten
// columns (the description rows Type, FollowupType, TargetTime, TargetVal, TargetObj, TargetObj2, TargetStaffType, Prize, CheckAtEndOnly, Independent),
// slot 601 counts the rows. The native class has its own vptr and eight-byte slots (BalanceValue).
#pragma once
#include "CBalanceTable.h"

// The thirteen description rows at 0x1028e908 (the table the original's GetBalanceDescription indexes at 0x3c bytes a row).
extern const BalanceDescription gChallengeBalanceDescriptions[13];

class CChallengeBalance : public CBalanceTable {
public:
    ~CChallengeBalance();   // 0x100f6acc, nonvirtual
    // 0x100f9318 (16 bytes, src/game/park/ChallengeHQ.cpp): the description array at 0x1028e908 plus row * 0x3c; no bound test.
    const BalanceDescription* GetBalanceDescription(int32_t row) override;
    // 0x100f9368 (8 bytes, src/game/park/ChallengeHQ.cpp): this + 8, the first value slot.
    BalanceValue* GetBalanceValuesStart() override;

    BalanceValue mValues[602];   // 0x08
};
