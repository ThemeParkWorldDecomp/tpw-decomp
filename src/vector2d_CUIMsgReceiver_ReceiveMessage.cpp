#include "vector2d.h"
#include "ThingReceivers.h"
#include "ThingMessages2.h"
#include "UIInterface.h"

// 0x10130998 (364 bytes): the message's type (virtual slot 2) picks a UIInterface call through a jump table over types 7 to 28; the
// other types do nothing. The Mac reads the word at +8 as a uint16 id (types 7, 8, 10, 15, 27, 28) or as an int (types 18, 19); the
// host reads them through the message classes that have those fields.
void CUIMsgReceiver::ReceiveMessage(const CMsg& message) {
    UIInterface* ui = &gUIInterface;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    switch (message.GetType()) {
        case 7:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->RideBrokenDown(id);
            break;
        case 8:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->RideCondemned(id);
            break;
        case 10:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->RideDeleted(id);
            break;
        case 15: {
            const CMsgStaffRoomEvent& m = static_cast<const CMsgStaffRoomEvent&>(message);
            id.mIndex = m.mThingId;
            ui->StaffRoomEvent(id, m.mArriving);
            break;
        }
        case 18:
            ui->ResearchCategoryComplete(static_cast<const CMsgResearchCategoryUpdated&>(message).mCategory);
            break;
        case 19: {
            int32_t value = static_cast<int32_t>(static_cast<const CMsgResearchCategoryUpdated&>(message).mCategory);
            if (value == 5) {
                ui->EnableResearchButton(true);
            } else if (value == 6) {
                ui->EnableResearchButton(false);
            }
            break;
        }
        case 27:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->ThingDeath(id);
            break;
        case 28:
            id.mIndex = static_cast<const CMsgRideDeleted&>(message).mThingId;
            ui->KidArrived(id);
            break;
        default:
            break;
    }
}
