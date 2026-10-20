// highestZ, main 0x101ca6dc (360 bytes), unit w50: the depth a new dialog needs to land above every visible window that
// overlaps it. UI_GetHighestZ (src/UI_pump.cpp) is the caller; Dialog::PlaceDepth reaches it through that.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"

// The three static words the original keeps (original names unknown; declared in include/UI_Base.h so the test can set and read
// them). Only highestZ uses them.
int32_t gUIHighestZ = 0;        // 0x10395b80  the running answer: set from the root's Z, raised by each child that is higher
int32_t gUIHighestZAttr = 0;    // 0x10395b78  the attribute word of the child last looked at
int32_t gUIHighestZChildZ = 0;  // 0x10395b7c  the Z of the child last looked at

// Walks the window's children in order. A child counts when attribute bit 1 is set and bit 2 is clear (its descendants are
// visited only then too). Its Z is the first item's for a list control (the type query answers 7) and its own otherwise.
// When that is above the answer so far: with no rectangle the answer becomes it plus 1; with a rectangle, only if the
// child's absolute position (GetPosition with 1) intersects it. The recursion's own answer is ignored; the static word
// carries it. Only the root resets the answer (to its own Z), so any other window starts from what the last call left,
// including from a different tree (UI_GetHighestZ passes the root when given NULL).
int32_t highestZ(InterfaceWindow* window, CoolRect* rect) {
    if (window == gUIRootWindow) gUIHighestZ = window->mZ;
    for (InterfaceWindow* child = window->GetChild(0); child != nullptr; child = child->GetNext()) {
        gUIHighestZAttr = static_cast<int32_t>(child->GetAttr());
        if ((gUIHighestZAttr & 1) != 0 && (gUIHighestZAttr & 2) == 0) {
            if (child->WhatAmI() == kUIKindListControl) {
                gUIHighestZChildZ = static_cast<InterfaceListControl2*>(child)->GetZOfEntries();
            } else {
                gUIHighestZChildZ = child->mZ;
            }
            if (gUIHighestZ < gUIHighestZChildZ) {
                if (rect == nullptr) {
                    gUIHighestZ = gUIHighestZChildZ + 1;
                } else {
                    CoolRect position;
                    child->GetPosition(&position, 1);
                    if (static_cast<int16_t>(position.RectIntersects(rect)) != 0) gUIHighestZ = gUIHighestZChildZ + 1;
                }
            }
            highestZ(child, rect);
        }
    }
    return gUIHighestZ;
}
