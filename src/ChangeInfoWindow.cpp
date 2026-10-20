// Unit v5102: the ride list panel's info window (ChangeInfoWindow 0x101b7108) and help line (UpdateHelpText 0x101b6d18).
#include <cstdint>

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

int32_t gHelpTextState = -1;   // 0x102acee0 (name invented)

namespace {

// GetWindow of the info window, then Show when it is there.
void ShowChild(InterfaceWindow* info, int32_t id, int16_t on) {
    InterfaceWindow* child = info->GetWindow(id);
    if (child != nullptr) child->Show(on);
}

}  // namespace

void ChangeInfoWindow(UIObjType type) {
    InterfaceWindow* info = gRideListPanel->GetWindow(0x1ed);
    if (info == nullptr) return;
    int32_t kind = static_cast<int32_t>(type);
    if (kind == 2) {
        ShowChild(info, 0x1f3, 1);
        ShowChild(info, 0x1f1, 0);
        ShowChild(info, 0x1f6, 0);
        ShowChild(info, 0x1f6, 0);   // asked twice
        ShowChild(info, 0x1f7, 0);
        ShowChild(info, 0x1ee, 1);
        ShowChild(info, 0x1f2, 0);
        ShowChild(info, 0x1ef, 0);
        ShowChild(info, 0x1f0, 0);
    } else if (kind == 0 || kind == 1 || kind == 3) {
        int16_t on = (kind == 0) ? 1 : 0;
        ShowChild(info, 0x1f3, on);
        ShowChild(info, 0x1f1, on);
        ShowChild(info, 0x1f6, on);
        ShowChild(info, 0x1f7, on);
        ShowChild(info, 0x1ee, on);
        ShowChild(info, 0x1f2, on);
        ShowChild(info, 0x1ef, on);
        ShowChild(info, 0x1f0, on);
    }
}

void UpdateHelpText(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    char16_t text[0x80];     // r1+0xdc; left unset for a negative id other than -1 and -2, as on the Mac
    char16_t name[0x40];     // r1+0x5c
    InterfaceWindow* window = gRideListPanel->GetWindow(0x1f8);
    CoolRect rect;
    window->GetClientRect(&rect);
    int16_t mouseX, mouseY;
    UI_GetMouseXY(mouseX, mouseY);
    if (!rect.PointIn(mouseX, mouseY)) {
        gHelpTextState = -1;
        return;
    }
    int16_t key = static_cast<int16_t>(id.mIndex);
    if (key < 0) {
        if (static_cast<uint16_t>(gRideListF88) == id.mIndex) return;
        gHelpTextState = 4;
        gRideListF88 = static_cast<int16_t>(id.mIndex);
        if (key == -1) {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x21a);
        } else if (key == -2) {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x21b);
        }
    } else {
        ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(id, 0);
        int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        RideInfo* ride = gRideDatabase.GetRideInfo(id);
        int32_t state;
        if (ride->Value(kRideSlot_GoldenTicketCost).mInteger == 0 ||
            gGameWorld->mParkTime.GoldenTickets()->HasBeenBought(id)) {
            state = (control->mUpgradeCost > balance) ? 2 : 3;
        } else {
            state = gGameWorld->mParkTime.GoldenTickets()->CanIAfford(id) ? 0 : 1;
        }
        if (state == gHelpTextState && static_cast<uint16_t>(gRideListF88) == id.mIndex) return;
        gHelpTextState = state;
        gRideListF88 = static_cast<int16_t>(id.mIndex);
        if (state == 2 || state == 3) {
            ride = gRideDatabase.GetRideInfo(id);
            static_cast<TbITable*>(gText.mResourcePointers[21])
                ->FormatString(name, 0x3f, static_cast<uint32_t>(ride->Value(kRideSlot_RideTypeStringIndex).mInteger));
            TbUniStringVTag tag(0, name);
            static_cast<TbITable*>(gText.mResourcePointers[7])
                ->FormatString(text, 0x80, state == 2 ? 0x96 : 0x94, &tag);
        } else if (state == 0) {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x97);
        } else {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x98);
        }
    }
    SetHelpTextString(text);
}
