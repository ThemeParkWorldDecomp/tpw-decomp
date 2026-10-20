// Unit v5767: CAdvisorScoring::Load, Save, GetFirstResponseFor and three scorers (_ADVISOR_TUTORIAL_UI_PANEL,
// _ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT, _ADVISOR_PRICING_DRINKS_POOR_VALUE).
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FS_Online.h"

#include "FS_MasterSave.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

uint16_t gAdvisorNoneDrinksPoorValue = 0;   // 0x1027df0c (zero in the original's data)

namespace {

// A balance value of the advisor table by its Mac offset in the scoring object (values start at 0x14 on the Mac).
inline const BalanceValue& Slot(const CAdvisorScoring* self, int32_t macOffset) {
    return self->mBalance.mValues[(macOffset - 0x14) / 4];
}

}  // namespace

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

int32_t CAdvisorScoring::_ADVISOR_TUTORIAL_UI_PANEL() {
    const TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    const int32_t day = static_cast<int32_t>(since.mTicks / 864000000000LL);
    if (day > Slot(this, 0x5dc).mInteger && day < Slot(this, 0x5e0).mInteger) return Slot(this, 0x5e4).mInteger;
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
