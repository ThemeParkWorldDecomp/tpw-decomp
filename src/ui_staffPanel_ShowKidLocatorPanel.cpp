// Unit v5343: ShowStaffPanel 0x101c0690 and ShowKidLocatorPanel 0x101c0a38 (the staff and kid locator slide-out panels).
#include <cstdint>

#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

namespace {

// The mouse block both functions end with: make the global mouse when there is none (mode CMMDefault, words 1E0/1E4 set to -1).
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    int32_t current = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
    if (current == 9) {
        CMMDefault* mode = new CMMDefault;
        if (gMouse == nullptr) {
            CMouse* mouse = new CMouse;
            CMMDefault* inner = new CMMDefault;
            mouse->AssignMouseMode(inner);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
            gMouse = mouse;
        }
        gMouse->AssignMouseMode(mode);
    }
}

}  // namespace

void ShowStaffPanel(CThing* thing) {
    InterfaceWindow* panel;
    if (thing != nullptr && thing->mKind == 1) {
        panel = gStaffPanelB;
        gStaffPanelState = 2;
    } else {
        panel = gStaffPanelA;
        gStaffPanelState = 1;
    }
    LinkSlideoutPanel(panel, 0);
    InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(panel->GetWindow(0x95d));
    if (thing != nullptr) {
        gStaffPanelThing = thing;
        int32_t location = -1;
        int32_t id = 0;
        switch (thing->mKind) {
            // the jump table at 0x102ae3c4 (kinds 0, 2, 3 and above 8 fall to the common code)
            case 1: location = 6; id = 0x964; break;
            case 4: location = 2; id = 0x95f; break;
            case 5: location = 1; id = 0x95e; break;
            case 6: location = 4; id = 0x961; break;
            case 7: location = 3; id = 0x962; break;
            case 8: location = 5; id = 0x960; break;
            default: break;
        }
        if (location >= 0) {
            gStaffPanelLocation = location;
            buttons->SetCurrent(id);
        }
        gMainPanel->link(gStaffPanelC, 1, 1);
        gStaffPanelC->Show(1);
        UpdateLocator();
        gFirstPersonFlags |= 0x1000;
    } else {
        gStaffPanelThing = nullptr;
        gStaffPanelLocation = 0;
        buttons->SetCurrent(-1);
    }
    if (gStaffPanelLocation == 6) {
        gStaffPanelC->GetWindow(0x148)->Show(0);
        gStaffPanelC->GetWindow(0x14a)->Show(1);
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11f));
    } else {
        gStaffPanelC->GetWindow(0x148)->Show(1);
        gStaffPanelC->GetWindow(0x14a)->Show(0);
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc6));
    }
    UIMain_CheckPanel();
    EnsureMouse();
}

void ShowKidLocatorPanel() {
    LinkSlideoutPanel(gStaffPanelB, 0);
    gStaffPanelState = 2;
    gStaffPanelLocation = 6;
    static_cast<InterfaceCheckButtons*>(gStaffPanelB->GetWindow(0x95d))->SetCurrent(-1);
    gStaffPanelThing = nullptr;
    gStaffPanelC->GetWindow(0x148)->Show(0);
    gStaffPanelC->GetWindow(0x14a)->Show(1);
    UIMain_CheckPanel();
    EnsureMouse();
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11f));
}
