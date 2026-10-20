// ui_allKidsPanel.cpp (main): UI_ClearSelected 0x10194838 (32 B).
#include "ui_main.h"
void* gUISelectedThing;    // 0x103947b8
void* gUISelectedPeep;     // 0x103947b4
void* gUISelectedObject;   // 0x103947b0
void UI_ClearSelected() {
    gUISelectedThing = nullptr;
    gUISelectedPeep = nullptr;
    gUISelectedObject = nullptr;
}
