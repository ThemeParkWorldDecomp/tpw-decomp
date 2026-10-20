#include <cstdint>
#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_main.h"
#include "ui_iterators.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "UI_Base.h"
#include "InterfaceScrollBar.h"

// 0x101bf32c (620 bytes): the next thing of the kind the location switch chose (1 handymen, 2 mechanics, 3 guards, 4 entertainers,
// 5 researchers, 6 visitors; any other value clears the thing). After the current one; when there is none, the first.
void GetNextStaffMember() {
    UIIteratorTypes type;
    switch (gStaffPanelLocation) {
        case 1: type = kUIIterHandymen; break;
        case 2: type = kUIIterMechanics; break;
        case 3: type = kUIIterGuards; break;
        case 4: type = kUIIterEntertainers; break;
        case 5: type = kUIIterResearchers; break;
        case 6: type = kUIIterVisitors; break;
        default: gStaffPanelThing = nullptr; return;
    }
    gStaffPanelThing = UI_IterateThingNext(type, gStaffPanelThing);
    if (gStaffPanelThing != nullptr) return;
    gStaffPanelThing = UI_IterateThingNext(type, nullptr);
}

namespace showstatus_callee {
__attribute__((weak)) InterfaceDrawObject* NewCashText() { return new InterfaceCashTextObject(nullptr); }
__attribute__((weak)) InterfaceDrawObject* NewTextSprite() { return new InterfaceTextObjectSprite(nullptr); }
}  // namespace showstatus_callee

namespace {

void SetUpText(InterfaceWindow* window) {
    window->SetFontJust(1, 1);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontColour(0, 0, 0, 0xff);
}

void SetUpCash(InterfaceWindow* window, int32_t maximum) {
    window->SetClientDrawObject(showstatus_callee::NewCashText());
    int32_t value = 0;
    int32_t high = maximum;
    static_cast<InterfaceVaryBox*>(window)->SetMinMaxValues(&value, &high);
    value = 1;
    static_cast<InterfaceVaryBox*>(window)->SetInc(&value);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontJust(1, 1);
    window->SetFontColour(0, 0, 0, 0xff);
}

void SetUpLabel(InterfaceWindow* window, int32_t string) {
    window->SetClientDrawObject(showstatus_callee::NewTextSprite());
    SetUpText(window);
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
}

}  // namespace

// 0x101c1c90 (1224 bytes)
void ShowStatusPanel::InitWindow() {
    UIStatusPanel* panel = reinterpret_cast<UIStatusPanel*>(this);
    InterfaceWindow* parent = reinterpret_cast<UIStatusPanelFields*>(this)->mWindow.get();
    if (panel->FillObjectList(gShowStatusThing) < 2) {
        parent->GetWindow(0xa08c)->Show(0);
        parent->GetWindow(0xa08d)->Show(0);
    }
    CoolRect rect;
    InterfaceWindow* window = parent->GetWindow(0xa08e);
    if (window != nullptr) static_cast<InterfaceScrollBar*>(window)->SetMinMax(0, 100);
    window = parent->GetWindow(0xa090);
    if (window != nullptr) SetUpCash(window, 10000);
    window = parent->GetWindow(0xa08f);
    if (window != nullptr) SetUpCash(window, 500);
    window = parent->GetWindow(0xa0a7);
    if (window != nullptr) SetUpLabel(window, 0x31);
    window = parent->GetWindow(0xa0a6);
    if (window != nullptr) SetUpLabel(window, 0x32);
    window = parent->GetWindow(0xa0a5);
    if (window != nullptr) SetUpLabel(window, 0x33);
    panel->InitEditBox(0xa0a4);
}
