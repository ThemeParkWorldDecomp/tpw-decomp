// ui_RideStatus.cpp static initialiser (0x101bc3ec, 80 bytes).
#include "ui_controls.h"
#include "ui_main.h"

// UIStatusPanel::UIStatusPanel (0x101929e8) is not translated yet: an empty weak stand-in; the unit test
// defines its own recording fake.
__attribute__((weak)) void UIStatusPanel_Construct(UIStatusPanel*) {}

// 0x101bc3ec (80 bytes)
void __sinit_ui_RideStatus_cpp() {
    RideStatusPanel* panel = &gRideStatusPanel;
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(panel));
    panel->mOps = &gRideStatusPanelOps;
    panel->mF00[0] = 0; panel->mF00[1] = 0; panel->mF00[2] = 0; panel->mF00[3] = 0;
    panel->mF28[0] = 0; panel->mF28[1] = 0; panel->mF28[2] = 0; panel->mF28[3] = 0;
}
