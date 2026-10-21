// UIMain_GotoStatusPanelForThing (unit v5071): shows the status panel that fits a thing.
#include <cstdint>

#include "Debug.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
}

uint16_t gNoUpgradeParent = 0;   // 0x102a7868
uint16_t gNoUpgradeTarget = 0;   // 0x102a786a

// 0x10181e2c
void UIMain_GotoStatusPanelForThing(void* thingPtr) {
    if (thingPtr == nullptr) return;
    CThing* thing = static_cast<CThing*>(thingPtr);
    const uint8_t* bytes = static_cast<const uint8_t*>(thingPtr);
    uint8_t kind = bytes[2];
    if (kind == 3) {
        CObject* object = static_cast<CObject*>(thingPtr);
        // The record's UI type (+0x7a8) and UpgradeType slot (+0x288) by name: the host's record is laid out wider (unit v5L31).
        const RideInfo* info = object->GetDBInfo();
        int32_t type = info->mUIType;
        switch (type) {
            case 0:
                if (info->Value(kRideSlot_UpgradeType).mInteger == 2) {
                    gUpgradeStatusPanel.Show(thing);
                } else {
                    uint16_t parent = object->GetUpgradeParent().mIndex;
                    if (parent == gNoUpgradeParent) {
                        if (object->UpgradesWhat().mIndex == gNoUpgradeTarget) gRideStatusPanel.Show(thing);
                    } else {
                        uint16_t again = object->GetUpgradeParent().mIndex;
                        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                        gRideStatusPanel.Show(static_cast<CThing*>(gThingArray[again].mThing));
                    }
                }
                break;
            case 1: gShopStatusPanel.Show(thing); break;
            case 2: gShowStatusPanel.Show(thing); break;
            case 3: {
                uint16_t flags = *reinterpret_cast<const uint16_t*>(bytes + 0x2e);
                if (flags & 1) gBogStatusPanel.Show(thing);
                else if (flags & 2) gStaffRoomStatusPanel.Show(thing);
                else gFeatureStatusPanel.Show(thing);
                break;
            }
            default: break;
        }
    } else if (kind == 5 || kind == 4 || kind == 6 || kind == 7 || kind == 8) {
        gStaffStatusPanel.Show(thing);
    } else if (kind == 1) {
        gVisitorStatusPanel.Show(thing);
    }
}
