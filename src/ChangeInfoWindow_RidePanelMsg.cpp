// RidePanelMsg 0x101b7538 (2084 bytes): the message handler of the ride list panel (unit v51005).
#include <cstdint>
#include <cstdlib>

#include "analyser.h"
#include "InterfaceWindow.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"
#include "ui_tooltip.h"
#include "Mouse.h"
#include "ui_controls.h"
#include "ui_iterators.h"

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> RideTag;

// The mouse is made on first use (the same code is inlined everywhere in the original): the mode is assigned to it afterwards.
void SetMouseMode(CMouseMode* mode) {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse();
        CMMDefault* normal = new CMMDefault();
        mouse->AssignMouseMode(normal);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(mode);
}

// The tail the two ways of placing a ride share (the ride is chosen and affordable).
int32_t PlaceRide(CMMPathDraw* draw, RideTag id) {
    SetMouseMode(draw);
    RemoveRideListPanel();
    RideInfo* ride = gRideDatabase.GetRideInfo(id);
    if (ride->mBalance.mValues[kRideSlot_WhichUIType].mInteger == 0) {
        if (UI_IterateThingNext(kUIIterRides, nullptr) == nullptr)
            UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc9));
        else
            UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xce));
    } else {
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xce));
    }
    return 0;
}

}  // namespace

int32_t RidePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        RideTag& view = reinterpret_cast<RideTag&>(gRideListF8C);
        if (static_cast<uint16_t>(gRideListF8C) != gRideListPending) {
            if (static_cast<int32_t>(IveGotTicks()) - gRideListF90 > 500) {
                UpdateViewWindow(view);
                gRideListF8C = 0;
            }
        }
        UIMain_TickRidePreview();
        if (static_cast<uint16_t>(gRideListF8A) != gRideListHelpShown)
            UpdateHelpText(reinterpret_cast<RideTag&>(gRideListF8A));
    } else if (message < 0x1e) {
        if (message == 0x10) {
            if (a == 0x80080) {
                InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0x1f8));
                list->UpdateDisplay();
                InterfaceWindow* title = window->GetWindow(0x200);
                int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                TbITable* strings = static_cast<TbITable*>(gText.mResourcePointers[6]);
                if (balance < 0) {
                    ShowStatusTag tag(0xd, static_cast<int32_t>(labs(balance)));
                    strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cb, &tag);
                } else {
                    ShowStatusTag tag(0xd, balance);
                    strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ca, &tag);
                }
                title->SetText(gUIMainConvertBuffer);
            }
        } else if (message < 0x10) {
            if (message == 5) {
                RemoveRideListPanel();
                return 1;
            }
        } else if (message == 0x15) {
            InitInfoWindow();
            ChangeInfoWindow(static_cast<UIObjType>(gRideListType));
            if (window->setAnimTimer(0x80080, 1000) != 0) window->startAnimTimer(0x80080);
        } else if (message < 0x15 && message > 0x13) {
            gRideListPanel = nullptr;
            UIMain_DestroyRidePreview();
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xbc));
        }
    } else if (message == 0x401) {
        InterfaceWindow* found = window->GetWindow(a);
        if (found != nullptr && b != -1) {
            int32_t data;
            static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
            RideTag id{static_cast<uint16_t>(data)};
            if (id.mIndex != static_cast<uint16_t>(gRideListF8C)) {
                gRideListF8C = static_cast<int16_t>(id.mIndex);
                gRideListF90 = static_cast<int32_t>(IveGotTicks());
            }
            gRideListF8A = static_cast<int16_t>(id.mIndex);
            UpdateInfoWindow(id);
        }
    } else if (message < 0x401) {
        if (message == 0x100) {
            if (a == 0x1ff) {
                CreateHireListPanel(static_cast<StaffType>(-1));
                return 0;
            }
            if (a < 0x1ff && a == -2) {
                RemoveRideListPanel();
                return 0;
            }
        } else if (message > 0xff && message > 0x3ff) {
            InterfaceWindow* found = window->GetWindow(a);
            if (found != nullptr && b != -1) {
                int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                int32_t data;
                static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
                if (data < 1) {
                    if (data == -1) {
                        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(0x39));
                        SetMouseMode(draw);
                        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x15a));
                    } else if (data < -1 && data > -3) {
                        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(0x3a));
                        SetMouseMode(draw);
                        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x159));
                    }
                    RemoveRideListPanel();
                    return 0;
                }
                RideTag id{static_cast<uint16_t>(data)};
                ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(id, 0);
                RideInfo* ride = gRideDatabase.GetRideInfo(id);
                CGoldenTicketControl* tickets =
                    reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2c0);
                if (ride->mBalance.mValues[kRideSlot_GoldenTicketCost].mInteger == 0 || tickets->HasBeenBought(id)) {
                    if (control->mUpgradeCost <= balance)
                        return PlaceRide(new CMMPathDraw(static_cast<CELL_MODES>(4), id), id);
                    UIMain_NotEnoughDough();
                } else if (tickets->CanIAfford(id)) {
                    return PlaceRide(new CMMPathDraw(static_cast<CELL_MODES>(4), id), id);
                } else {
                    UIMain_NotEnoughDough();
                }
            }
        }
    } else if (message == 0x406) {
        gRideListSortColumn = b;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}
