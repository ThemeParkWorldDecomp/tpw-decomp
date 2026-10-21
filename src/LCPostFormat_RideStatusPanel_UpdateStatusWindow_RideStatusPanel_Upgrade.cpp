// Unit v5914: the ride status panel's knobs and buttons.
//   0x101bbb88 RideStatusPanel::Upgrade (448 bytes): pays for an upgrade (flag 1: the ride's own, asked of the object at once; a negative `what`: the
//              ride's upgrade of that level; else builds the object type `what`, when the bank balance covers its control's upgrade cost)
//   0x101bbacc RideStatusPanel::ApplyChanges (136 bytes)
//   0x101bb9b8 RideStatusPanel::DoDial (228 bytes)
//   0x101bb71c RideStatusPanel::Edit (624 bytes)
//   0x101bc0bc RideStatusPanel::Show (176 bytes)
//   0x101bb494 RideStatusPanel::UpdateKnobsWindow (588 bytes)
#include <cstdint>
#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ThingReceivers.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "InterfaceScrollBar.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "TPEngine.h"
#include "World.h"
#include "WindowScripts.inc"

// 0x1039599c (bss) and 0x102acef0 (the window script).
CThing* gRideStatusShowThing;
int16_t gRideStatusScript[] = {TPW_WINDOW_SCRIPT_102ACEF0};   // 0x102acef0: the image's words (generated, devtools/gen_game_tables.py)

namespace allitems_post_callee {
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace allitems_post_callee

namespace {

// The ride record's value of a slot (the Mac's offset is 0xc + 4 * slot).
int32_t RideValue(RideInfo* info, int32_t slot) { return info->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

// gMouse is made on first use (the same code is inlined everywhere in the original).
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;   // 1 byte on the Mac; new never answers NULL
        CMMDefault* mode = new CMMDefault();
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
}

}  // namespace

// 0x101bbb88. `flag` is the Mac's byte, compared with 1.
int32_t RideStatusPanel::Upgrade(int32_t what, bool flag) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    CBankAccount* account = gGameWorld->GetBankAccount();
    int32_t balance = account->GetBalance();
    if (flag) {
        object->RequestUpgrade(0);
        return 1;
    }
    if (what < 0) {
        RideInfo* info = object->GetDBInfo();
        // the record's CostOfUpgrade of level -what (slot 107, 16 slots a level)
        if (balance > RideValue(info, 107 + 16 * (-what))) {
            object->RequestUpgrade(1);
            return 0;
        }
        return 0;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type = {static_cast<uint16_t>(what)};
    ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(type, 0);
    if (balance >= control->mUpgradeCost) {
        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(4), type);
        EnsureMouse();
        gMouse->AssignMouseMode(draw);
        UI_SetSelectedObject(object);
        LLCM_SetCurrentObject(true);
        return 1;
    }
    UIMain_NotEnoughDough();
    return 0;
}

// 0x101bbacc.
void RideStatusPanel::ApplyChanges() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->SetOperatingSpeed(static_cast<uint32_t>(mSpeed));
    object->SetOperatingCapacity(static_cast<uint8_t>(mCapacity));
    if (RideValue(object->GetDBInfo(), 25) != 0) object->SetOperatingDuration(static_cast<uint8_t>(mDuration));
}

// 0x101bb9b8. The Mac's table slot at +0x1c (UpdateStatusWindow) after each knob.
void RideStatusPanel::DoDial(int32_t a, int32_t b) {
    reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
    if (a == 0x3e2f) {
        mDuration = b;
        UIStatusPanel_CallSlot(this, 0x1c);
        UpdateKnobsTitles(4);
    } else if (a < 0x3e2f) {
        if (a == 0x3e2d) {
            mCapacity = b;
            UIStatusPanel_CallSlot(this, 0x1c);
            UpdateKnobsTitles(2);
        }
    } else if (a < 0x3e31) {
        mSpeed = b;
        UIStatusPanel_CallSlot(this, 0x1c);
        UpdateKnobsTitles(1);
    }
}

// 0x101bb71c.
void RideStatusPanel::Edit(int32_t on) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    UI_SetSelectedObject(object);
    LLCM_SetCurrentObject(true);
    if (on == 0) {
        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(0x14));
        EnsureMouse();
        gMouse->AssignMouseMode(draw);
        LLCM_LeftMouseButtonUp(0, 0, 0, 0);
        UIStatusPanel_CallSlot(this, 0x34);
        return;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type = {object->mObjectType};
    RideInfo* info = gRideDatabase.GetRideInfo(type);
    int32_t track = RideValue(info, 36);
    int32_t mode;
    if (track == 2) {
        mode = 0x13;
    } else if (track < 2) {
        mode = track >= 1 ? 0xe : 0;
    } else if (track < 4) {
        CoasterEditMode(object);
        UIStatusPanel_CallSlot(this, 0x34);
        return;
    } else {
        mode = 0;
    }
    CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(mode));
    LLCC_SystemSetRideTrack(RideValue(info, 36));
    EnsureMouse();
    gMouse->AssignMouseMode(draw);
    LLCM_LeftMouseButtonUp(0, 0, 0, 0);
    UIStatusPanel_CallSlot(this, 0x34);
}

// 0x101bc0bc.
void RideStatusPanel::Show(CThing* thing) {
    gRideStatusShowThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gRideStatusScript, RideStatusMsg, 0x3e24, 0x3e15, 0x3e25);
    mUpgradeWindow = nullptr;
    // the condition as an int, its low byte: 1 to 0x14 is the "needs attention" message
    uint32_t condition = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(reinterpret_cast<CObject*>(thing)->mF40))) & 0xffu;
    if (condition <= 0x14 && condition != 0) {
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x129));
        return;
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xcf));
}

// 0x101bb494.
void RideStatusPanel::UpdateKnobsWindow() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    RideInfo* info = object->GetDBInfo();
    if (mWindow == nullptr) return;
    InterfaceWindow* window = mWindow->GetWindow(0x3e30);
    if (window != nullptr) {   // the speed bar
        int32_t low = RideValue(info, 74);
        int32_t high = RideValue(info, 75);
        static_cast<InterfaceScrollBar*>(window)->SetMinMax(low, high);
        allitems_post_callee::WindowSetValue(window, static_cast<int32_t>(object->mF54));
        uint8_t level = object->mF4C;
        InterfaceWindow* marker = window->GetWindow(0x3e2e);
        allitems_post_callee::WindowSetValue(marker, (RideValue(info, 104 + 16 * level) << 10) / (high - low));
    }
    window = mWindow->GetWindow(0x3e2f);
    if (window != nullptr) {   // the duration bar
        int32_t unit = RideValue(info, 25);
        *reinterpret_cast<int32_t*>(mF28) = unit;
        window->Show(unit == 0 ? 0 : 1);
        static_cast<InterfaceScrollBar*>(window)->SetMinMax(RideValue(info, 72), RideValue(info, 73));
        allitems_post_callee::WindowSetValue(window, object->mF58);
    }
    window = mWindow->GetWindow(0x3e2d);
    if (window != nullptr) {   // the capacity bar
        int32_t low = RideValue(info, 70);
        int32_t high = RideValue(info, 71);
        if (low == high) {
            window->Show(0);
            mWindow->GetWindow(0x3e31)->Show(0);
        } else {
            window->Show(1);
            mWindow->GetWindow(0x3e31)->Show(1);
            uint8_t level = object->mF4C;
            InterfaceWindow* marker = window->GetWindow(0x3e2e);
            allitems_post_callee::WindowSetValue(marker, (RideValue(info, 100 + 16 * level) << 10) / (high - low));
        }
        static_cast<InterfaceScrollBar*>(window)->SetMinMax(low, high);
        allitems_post_callee::WindowSetValue(window, object->mF59);
    }
    UpdateKnobsTitles(7);
}
