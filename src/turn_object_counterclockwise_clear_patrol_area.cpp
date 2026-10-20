// keyboard.cpp key actions (unit v5341): start_strike, clear_patrol_area, wheres_my_script, abort_all_requesters.
#include <cstdint>

#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "DateMessages.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "keytable_actions.h"

// The person the UI has selected is a staff member of kind 4 to 8 (mechanic, handyman, entertainer, guard, researcher).
int32_t clear_patrol_area() {
    if (gUIInterface.GetSelectedPeep() != nullptr) {
        uint8_t kind = reinterpret_cast<uint8_t*>(gUIInterface.GetSelectedPeep())[2];
        if (kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7) {
            reinterpret_cast<CStaff*>(gUIInterface.GetSelectedPeep())->CancelPatrolArea();
        }
    }
    return 1;
}

// Prints where the pointed thing's script is (kinds 1, 4 to 8 and 0x12).
int32_t wheres_my_script() {
    uint8_t* thing = reinterpret_cast<uint8_t*>(gIntrface.mPointedThing);
    if (thing != nullptr) {
        uint8_t kind = thing[2];
        if (kind == 1 || kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7 || kind == 0x12) {
            float x, y, z;
            int32_t mode;
            SCR_GetScriptPosition(*reinterpret_cast<int32_t*>(thing + 0xc), &x, &y, &z, &mode);
            CTRACE(0x1c, "SP: (%g, %g, %g, %d)\n", static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), mode);
        }
    }
    return 1;
}

// All five staff types strike, then the end of month message goes out (the message centre is made on first use: operator new(0x1d0)
// and __construct_array of 0x1d headers on the Mac).
int32_t start_strike() {
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(0));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(1));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(2));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(3));
    gGameWorld->GetStaffHQ()->ForceStrike(static_cast<StaffType>(4));
    CMsgEndOfMonth message;
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre();
    gMessageCentre->PostMessage(message);
    return 1;
}

// The mouse is made on first use (the Mac inlines this at three places; the second and third are dead after the first).
static void MakeMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
}

// 1 when locators were removed; 0 when the mode is 0 or 1 already; otherwise the mouse goes back to the default mode and the cells too.
int32_t abort_all_requesters() {
    if (gUIInterface.RemoveLocators() != 0) return 1;
    MakeMouse();
    int32_t mode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
    if (mode == 0) return 0;
    MakeMouse();
    mode = gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
    if (mode == 1) return 0;
    CMMDefault* fresh = new CMMDefault;
    MakeMouse();
    gMouse->AssignMouseMode(fresh);
    LLCC_SystemSetMode(0, true);
    return 1;
}
