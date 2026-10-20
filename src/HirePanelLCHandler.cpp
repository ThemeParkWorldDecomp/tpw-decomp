// The hire panel's list functions (ui_mainpanel2.cpp): HirePanelLCPostFormat 0x101a18a4 (the row attach function) and
// HirePanelLCHandler 0x101a19f8 (the message function of the staff type buttons). Unit v5554.
// Note: src/LCPostFormat.cpp is the LCPostFormat at 0x101c346c, a different function.
#include "LoadPanelMsg.h"

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "analyser.h"
#include "ui_main.h"

uint32_t gHireLCFontIndex = 7;       // 0x102aa31c (name invented): the font table index of the hire list rows
int32_t gHireListScript = 0;         // 0x1039490c (name invented): the script handle the handler kills
int32_t gHireListSelA = -1;          // 0x102aa328 (name invented): reset to -1 by the handler
int32_t gHireListSelB = -1;          // 0x102aa32c (name invented): reset to -1 by the handler

void HirePanelLCPostFormat(InterfaceWindow* window) {
    using namespace lender_callee;
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    window->SetFont(GetFontTableEntry(gHireLCFontIndex));
    InterfaceWindow* child = window->GetChild(0);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(gHireLCFontIndex));
    child->SetFontJust(2, 1);
    WindowSetValue(child, 0);
}

static void HireSelect(int32_t type, int32_t stringId) {
    FillHireList(static_cast<StaffType>(type), 1);
    UIMain_SetTitleText(gHirePanelWindow, 0x2480, const_cast<char16_t*>(UIMain_GetString(stringId)));
    if (gHireListScript != 0) SCR_KillScript(gHireListScript);
    gHireListScript = 0;
    gHireListSelA = -1;
    gHireListSelB = -1;
}

int32_t HirePanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        switch (a) {
            case 0x2490: HireSelect(0, 0x8b); break;
            case 0x2491: HireSelect(4, 0x8f); break;
            case 0x2492: HireSelect(2, 0x8d); break;
            case 0x2493: HireSelect(1, 0x8c); break;
            case 0x2494: HireSelect(3, 0x8e); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
