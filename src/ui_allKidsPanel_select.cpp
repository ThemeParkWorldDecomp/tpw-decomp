// ui_allKidsPanel.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: the selection words' accessors,
// UI_GetSelectedPeep (0x101946f8), UI_GetSelectedObject (0x10194730), UI_SetSelectedObject (0x10194768), 12 bytes each, and
// UI_CheckSelected (0x101947e4, 44 bytes). Unit v5L16: UI_SetSelectedPeep (0x101947a8, 12 bytes).
#include "intrface.h"
#include "ui_main.h"

CThing* UI_GetSelectedPeep() { return static_cast<CThing*>(gUISelectedPeep); }

void* UI_GetSelectedObject() { return gUISelectedObject; }

void UI_SetSelectedObject(void* object) { gUISelectedObject = object; }

void UI_SetSelectedPeep(CPeep* peep) { gUISelectedPeep = peep; }

// Takes the interface's three pointer words as the selection.
void UI_CheckSelected() {
    gUISelectedThing = gIntrface.mPointedThing;
    gUISelectedPeep = gIntrface.mPointedStaffOrGuest;
    gUISelectedObject = gIntrface.mThingUnderPointer;
}
