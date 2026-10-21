#include <cstdint>
#include <new>
#include "AdvisorQueue.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"

// 0x10180b84, 1028 bytes. The panel state of a ride or shop: 3 condemned, 0x19/0x1a track problems, 5 to 9 upgrade wanted, 0xb to 0xf
// flagged for maintenance with relief, 0x11 to 0x15 flagged without, 2 or 0x15 broken down, 0x16/0x17 no queue, 0x18 exit unconnected,
// else 1 or 0.
int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    CWorld* world;
    RideInfo* ride = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType});
    if (object->IsCondemned()) return 3;
    const int32_t kind = ride->Value(kRideSlot_WhichTrackType).mInteger;
    if (kind == 3) {
        if (!IsTrackClosed(object)) return 0x19;
        if (!IsTrackValid(object)) return 0x1a;
    } else if (kind == 1 || kind == 2) {
        if (object->mF28 == 0) return 0x19;
    }
    if (object->RequiresUpgrade()) {
        world = gGameWorld;
        if (world->mF1DA73E == gUIMainId7858) return 5;
        if (world->GetStaffHQ()->IsStriking(static_cast<StaffType>(1))) return 7;
        if (object->GetAssignedStaffMember().mIndex == gUIMainId785A) return 6;
        return object->IsBeingWorkedUpon() ? 9 : 8;
    }
    if (object->IsFlaggedForMaintenance()) {
        world = gGameWorld;
        if (object->mFlags & 1) {
            if (world->mF1DA73C == gUIMainId785C) return 0xb;
            if (world->GetStaffHQ()->IsStriking(static_cast<StaffType>(0))) return 0xd;
            if (object->GetAssignedStaffMember().mIndex == gUIMainId785E) return 0xc;
            return object->IsBeingWorkedUpon() ? 0xf : 0xe;
        }
        if (world->mF1DA73E == gUIMainId7860) return 0x11;
        if (world->GetStaffHQ()->IsStriking(static_cast<StaffType>(1))) return 0x13;
        if (object->GetAssignedStaffMember().mIndex == gUIMainId7862) return 0x12;
        return object->IsBeingWorkedUpon() ? 0x15 : 0x14;
    }
    if (object->IsBrokenDown()) return object->IsBeingWorkedUpon() ? 0x15 : 2;
    if ((object->mFlags & 8) && !object->IsBOQConnected()) return object->IsClosed() ? 0x17 : 0x16;
    if (ride->mUIType == 0) {
        if (object->UpgradesWhat().mIndex == gUIMainId7864 && ride->Value(kRideSlot_UpgradeType).mInteger != 2 &&
            !object->IsExitCellConnected())
            return 0x18;
    }
    if (flag == 0) return 1;
    if (flag > 1 && object->IsClosed()) return 1;
    return 0;
}
