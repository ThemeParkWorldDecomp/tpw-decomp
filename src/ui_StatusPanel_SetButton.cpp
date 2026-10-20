// ui_StatusPanel_SetButton.cpp (main): SetButton 0x101904f8 (1316 B), DoButton 0x10190a60 (588 B).
#include "Debug.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

namespace {
// The Mac's string pool after "mb_locate" (0x1024dd34): the names SetButton looks up are at these offsets.
const char kNames[] =
    "mb_locate\0mb_buy\0b_loan\0mb_resrch\0b_pkinfo\0b_staffcost\0mb_callmech\0b_entry\0mb_accept\0mb_decline";
}  // namespace

void SetButton(InterfaceWindow* window, TagButtonAction action) {
    const char* name;
    int32_t help;   // -1: leaves mHelpText alone
    switch (static_cast<int32_t>(action)) {
        case 3: name = kNames + 0x00; help = 0x204; break;
        case 4: name = kNames + 0x00; help = 0x203; break;
        case 9: name = kNames + 0x0a; help = 0x209; break;
        case 10: name = kNames + 0x0a; help = 0x20a; break;
        case 11: name = kNames + 0x0a; help = 0x20b; break;
        case 12: name = kNames + 0x0a; help = 0x20c; break;
        case 13: name = kNames + 0x0a; help = 0x20d; break;
        case 14: name = kNames + 0x0a; help = 0x20e; break;
        case 15: name = kNames + 0x0a; help = 0x20f; break;
        case 16: name = kNames + 0x0a; help = 0x210; break;
        case 17: name = kNames + 0x0a; help = 0x211; break;
        case 18: name = kNames + 0x11; help = 0x212; break;
        case 19: name = kNames + 0x18; help = 0x213; break;
        case 20: name = kNames + 0x22; help = 0x214; break;
        case 21: name = kNames + 0x2b; help = 0x215; break;
        case 22: return;   // the jump table's entry 19 goes straight to the epilogue
        case 23: name = kNames + 0x37; help = 0x217; break;
        case 24: name = kNames + 0x43; help = 0xec; break;
        case 26: name = kNames + 0x4b; help = -1; break;
        case 27: name = kNames + 0x55; help = -1; break;
        default:
            window->SetDrawObject(nullptr);
            window->Enable(0);
            return;
    }
    window->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>(name)));
    window->Enable(1);
    if (help != -1) window->mHelpText = help;
}

int32_t DoButton(TagButtonAction action, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id, int32_t extra) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ride;
    ride.mIndex = static_cast<uint16_t>(extra);
    switch (static_cast<int32_t>(action)) {
        case 3:
            UIMain_ZoomToThing(id);
            return 0;
        case 4: {
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map;
            map.mIndex = static_cast<uint16_t>(extra);
            UIMain_ZoomToMapId(map);
            return 0;
        }
        case 9: CreateRideListPanel(static_cast<UIObjType>(0), ride); return 2;
        case 10: CreateRideListPanel(static_cast<UIObjType>(1), ride); return 2;
        case 11: CreateRideListPanel(static_cast<UIObjType>(2), ride); return 2;
        case 12: CreateRideListPanel(static_cast<UIObjType>(3), ride); return 2;
        case 13: CreateHireListPanel(static_cast<StaffType>(0)); return 2;
        case 14: CreateHireListPanel(static_cast<StaffType>(1)); return 2;
        case 15: CreateHireListPanel(static_cast<StaffType>(2)); return 2;
        case 16: CreateHireListPanel(static_cast<StaffType>(3)); return 2;
        case 17: CreateHireListPanel(static_cast<StaffType>(4)); return 2;
        case 18: CreateLoansPanel(); return 2;
        case 19: CreateResearchPanel(); return 2;
        case 20: CreateParkStatusPanel(); return 2;
        case 21: CreateStaffCostsPanel(); return 2;
        case 22: CreateYearEndPanel(); return 2;
        case 23:
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            static_cast<CObject*>(gThingArray[id.mIndex].mThing)->RequestMaintenance(1);
            return 1;
        case 24: CreateEntryPricePanel(); return 2;
        case 26: gGameWorld->GetChallengeHQ()->ChallengeAccepted(); return 2;
        case 27: gGameWorld->GetChallengeHQ()->ChallengeDeclined(); return 2;
        default: return 0;
    }
}
