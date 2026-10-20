// CreateFinancePanel 0x101a001c (3140 bytes, unit v51026): builds the finance panel under the game window, sets the label texts and the
// starting state of the graph buttons.
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

int16_t gFinanceScript[] = {TPW_WINDOW_SCRIPT_102A9BEC};   // 0x102a9bec: the image's words (generated, devtools/gen_game_tables.py)

namespace {

// The label set-up the original repeats for every text window: a text sprite as the client draw object, centred, font 0xb, black,
// no text of its own; then the string through the virtual SetText (slot 0x2c of the table at +0x118).
void SetupLabel(InterfaceWindow* window, int32_t stringId) {
    void* mem = parkstatus_callee::OperatorNew(0x19c);
    if (mem != nullptr) parkstatus_callee::SpriteCtor(mem);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(mem));
    window->SetFontJust(1, 1);
    window->SetFont(GetFontTableEntry(0xb));
    window->SetFontColour(0, 0, 0, 0xff);
    window->SetClientDrawText(nullptr);
    parkstatus_callee::WindowSetText(window, UIMain_GetString(stringId));
}

void SetButton(InterfaceWindow* panel, uint32_t bit, int32_t id) {
    if ((gFinanceGraphFlags & bit) != 0) static_cast<InterfaceButton*>(panel->GetWindow(id))->SetState(1, 1);
}

}  // namespace

void CreateFinancePanel() {
    if (gFinanceGraphPanel != nullptr) return;
    UIMain_CheckPanel();
    SetLastPanel(3, 7);
    InterfaceWindow* built = gGameWindow->Build(gFinanceScript, FinancePanelPanelMsg);
    if (built == nullptr) return;
    gFinanceGraphPanel = built;
    UIMain_SetPanel(gFinanceGraphPanel);
    InterfaceWindow* panel = gFinanceGraphPanel->GetWindow(0x12272);
    if (panel != nullptr) {
        void* mem = parkstatus_callee::OperatorNew(0xdc);
        if (mem != nullptr) parkstatus_callee::DrawSpriteCtor(mem);
        panel->SetClientDrawObject(static_cast<InterfaceDrawObject*>(mem));
        panel->SetCallbackFunction(FinanceGraphPanelMsg);
        InterfaceWindow* group = panel->GetWindow(0x12273);
        if (group != nullptr) {
            SetupLabel(group->GetWindow(0x12275), 0x16a);
            SetupLabel(group->GetWindow(0x12276), 0x16b);
            SetupLabel(group->GetWindow(0x12274), 0x16c);
            int32_t mode = gFinanceGraphMode;
            InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(group);
            if (mode == 1) {
                checks->SetCurrent(0x12276);
            } else if (mode < 1) {
                if (mode >= 0) checks->SetCurrent(0x12275);
            } else if (mode < 3) {
                checks->SetCurrent(0x12274);
            }
        }
        SetupLabel(panel->GetWindow(0x12277), 0xa2);
        SetupLabel(panel->GetWindow(0x1227b), 0xa3);
        SetupLabel(panel->GetWindow(0x12279), 0xa4);
        SetupLabel(panel->GetWindow(0x12278), 0xa5);
        SetupLabel(panel->GetWindow(0x1227c), 0xa6);
        SetupLabel(panel->GetWindow(0x1227d), 0xa7);
        SetupLabel(panel->GetWindow(0x1227a), 0xa8);
        SetupLabel(panel->GetWindow(0x1227e), 0xa9);
        SetButton(panel, 1, 0x12277);
        SetButton(panel, 2, 0x1227b);
        SetButton(panel, 4, 0x12279);
        SetButton(panel, 8, 0x12278);
        SetButton(panel, 0x10, 0x1227c);
        SetButton(panel, 0x20, 0x1227d);
        SetButton(panel, 0x40, 0x1227a);
        SetButton(panel, 0x80, 0x1227e);
        if (gLoansGameTypeGuard == 0) {
            new (gLoansGameTypeStorage) CGameType();
            gLoansGameTypeGuard = 1;
        }
        if (reinterpret_cast<CGameType*>(gLoansGameTypeStorage)->mType == kGameTypeEasy) {
            entry_callee::WindowEnable(gFinanceGraphPanel->GetWindow(0x1227f), 0);
        }
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc5));
}
