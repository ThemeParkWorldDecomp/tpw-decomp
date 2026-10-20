// CStaffDB::Init (0x10129298, 60 bytes) and CStaffDB::Update (0x1012a10c, 856 bytes), main app, unit w129. The staff database is the first member of the
// world (the world's CWorldSavvy::GetWorld answers gWorld, which CTimeSig's inline members and the dice use).
#include <cstdint>

#include "MainBalance.h"
#include "StaffDB.h"
#include "TimeSig.h"
#include "UIInterface.h"
#include "World.h"

namespace {
CTimeSig* Stamp(uint32_t* word) { return reinterpret_cast<CTimeSig*>(word); }
}  // namespace

// The opening pool is made, then the top-up clock is stamped with the world's turn.
void CStaffDB::Init() {
    GenerateOpeningStaffPool();
    Stamp(&mF294)->SetTimeSig();
}

// Pass 1: each of the 0x20 records that is valid (byte 0xa nonzero) and has been idle for more than its timeout (byte 0x10, shifted left by 2: four turns
// a second), compared UNSIGNED, and is not on the pointer (byte 0xb zero) is retired (byte 0xa cleared) and the UI told its slot.
// Pass 2, only when the turns since the top-up clock was stamped (signed answer, compared unsigned) are more than the balance's interval shifted left by 2:
// the count of valid records; the most to make is the balance's maximum, replaced by 0x20 minus GetNumStaff when it is not below the room (unsigned
// compare); CountStaffTypes; for each of the five categories the need is 0 when its stop flag (byte 0x299 + category) is set, else the balance's target
// (read after the second of two GetNumStaffOfType calls, the first answer is ignored) minus the category's count; the total of the needs (32 bit wrap).
// Then while the most is above 0 and the total above 0 (signed): the dice answer r (gWorld), r % total picks a place in the needs, walked in category order
// (a need the pick is below, signed, is the category, otherwise the need is taken from the pick); a stopped category or no free record (0x20) ends the
// loop; else a staff member of the category is generated into the free record and the most, the total and the category's need go down by 1. The loop's
// category after a walk that finds none (only reachable when the needs wrapped) is the one of the previous round: the original keeps it in a register.
// Last, also when the loop did not run, EnsureMinimumAmountInPool and the clock stamp.
void CStaffDB::Update() {
    for (uint32_t slot = 0; slot < 0x20; ++slot) {
        CStaffInfo* record = reinterpret_cast<CStaffInfo*>(mRecords + 0x14 * slot);
        if (record->mValid != 0) {
            const int32_t turns = Stamp(&record->mTimeSig)->TurnsSinceLastUpdate();
            if (static_cast<uint32_t>(turns) > (record->mTimeoutTime << 2) && record->mOnPointer == 0) {
                record->mValid = 0;
                gUIInterface.StaffDBChange(static_cast<int32_t>(slot));
            }
        }
    }
    const uint32_t limit = static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceStaffTopUpInterval].mInteger) << 2;
    const int32_t sinceTopUp = Stamp(&mF294)->TurnsSinceLastUpdate();
    if (static_cast<uint32_t>(sinceTopUp) > limit) {
        int32_t count = 0;
        for (uint32_t slot = 0; slot < 0x20; ++slot) {
            if (mRecords[0x14 * slot + 0xa] != 0) ++count;
        }
        int32_t most = gMainBalance.mValues[kMainBalanceStaffTopUpMax].mInteger;
        if (static_cast<uint32_t>(most) >= static_cast<uint32_t>(0x20 - count)) most = 0x20 - GetNumStaff();
        CountStaffTypes();
        int32_t need[5];
        for (int32_t k = 0; k < 5; ++k) {
            if (mStopProducing[k] != 0) {
                need[k] = 0;
            } else {
                GetNumStaffOfType(k);
                const int32_t have = GetNumStaffOfType(k);
                need[k] = static_cast<int32_t>(static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceStaffTarget + k].mInteger) -
                                               static_cast<uint32_t>(have));
            }
        }
        int32_t total = static_cast<int32_t>(static_cast<uint32_t>(need[0]) + static_cast<uint32_t>(need[1]) + static_cast<uint32_t>(need[2]) +
                                             static_cast<uint32_t>(need[3]) + static_cast<uint32_t>(need[4]));
        int32_t category = 0;
        while (most > 0 && total > 0) {
            const uint32_t dice = gWorld->GetRand();
            int32_t pick = static_cast<int32_t>(dice % static_cast<uint32_t>(total));
            for (int32_t k = 0; k < 5; ++k) {
                if (pick < need[k]) {
                    category = k;
                    break;
                }
                pick = static_cast<int32_t>(static_cast<uint32_t>(pick) - static_cast<uint32_t>(need[k]));
            }
            if (mStopProducing[category] != 0) break;
            const uint32_t slot = GetFirstFreeSlot();
            if (slot == 0x20) break;
            reinterpret_cast<CStaffInfo*>(mRecords + 0x14 * slot)->Generate(category, static_cast<int32_t>(slot));
            --most;
            --total;
            --need[category];
        }
        EnsureMinimumAmountInPool();
        Stamp(&mF294)->SetTimeSig();
    }
}
