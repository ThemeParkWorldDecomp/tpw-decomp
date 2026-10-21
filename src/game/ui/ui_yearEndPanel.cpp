// ui_yearEndPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>

// Unit v51007: ShowStatusPanel::UpdateWindow (0x101c21c4), __sinit_ui_upgradeStatus_cpp (0x101c6738), YearEndPanelMsg (0x101c67b4).

int32_t YearEndPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message != 0x15) {
        if (message < 0x15) {
            if (message == 5) {
                RemoveYearEndPanel();
                return 1;
            }
            if (message > 4 && message >= 0x14) gYearEndPanel = nullptr;
        } else if (message == 0x100) {
            if (a == 0xadb4) {
                CreateParkStatusPanel();
                return 0;
            }
            if (a < 0xadb4) {
                if (a == -2) {
                    RemoveYearEndPanel();
                    return 0;
                }
            } else if (a < 0xadb6) {
                CreateFinancePanel();
                return 0;
            }
        }
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// CreateYearEndPanel 0x101c68d0 (2792 bytes): builds the year end panel (script gYearEndScript, message function YearEndPanelMsg) and
// gives its windows their draw objects, fonts, texts and the park analyser's cash and number histories.

InterfaceWindow* gYearEndPanel = nullptr;           // 0x10395b70
int16_t gYearEndScript[] = {TPW_WINDOW_SCRIPT_102AF524};   // 0x102af524: the image's 478 words (generated, unit v5L42)

namespace {

// A text window: the sprite text object, the font of the table, the string.
void TextWindow(InterfaceWindow* parent, int32_t id, uint32_t font, int32_t string) {
    InterfaceWindow* window = parent->GetWindow(id);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::SpriteCtor(object);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(font));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
}

// A cash window showing the history's value `back` places back.
void CashWindow(InterfaceWindow* parent, int32_t id, CHistory<int, 144>* history, int32_t back) {
    InterfaceWindow* window = parent->GetWindow(id);
    int32_t value;
    history->GetValue(value, back);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::CashCtor(object);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, value);
}

// A number window, right justified, first set to 0.
void NumberWindow(InterfaceWindow* parent, int32_t id, CHistory<int, 144>* history, int32_t back) {
    InterfaceWindow* window = parent->GetWindow(id);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::NumberCtor(object);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, 0);
    window->SetFontJust(2, 1);
    int32_t value;
    history->GetValue(value, back);
    lender_callee::WindowSetValue(window, value);
}

}  // namespace

void CreateYearEndPanel() {
    if (gYearEndPanel != nullptr) return;
    UIMain_CheckPanel();
    InterfaceWindow* built = gGameWindow->Build(gYearEndScript, YearEndPanelMsg);
    if (built == nullptr) return;
    gYearEndPanel = built;
    UIMain_SetPanel(gYearEndPanel);
    UIMain_SetTitleText(gYearEndPanel, 0xadb3, const_cast<char16_t*>(UIMain_GetString(0xba)));
    InterfaceWindow* inner = gYearEndPanel->GetWindow(0xada1);
    if (inner == nullptr) return;
    TextWindow(inner, 0xadae, 8, 0xbc);
    TextWindow(inner, 0xadaf, 8, 0xbb);
    TextWindow(inner, 0xada9, 6, 0xbd);
    TextWindow(inner, 0xada3, 6, 0xbe);
    TextWindow(inner, 0xada4, 6, 0xbf);
    TextWindow(inner, 0xada5, 6, 0xc0);
    TextWindow(inner, 0xadb0, 6, 0xc1);
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    CashWindow(inner, 0xada8, &analyser->mHistory1F100[1], 0);
    CashWindow(inner, 0xadac, &analyser->mHistory1F100[1], 0xc);
    NumberWindow(inner, 0xada2, &analyser->mHistory21160[1], 0);
    NumberWindow(inner, 0xadad, &analyser->mHistory21160[1], 0xc);
    CashWindow(inner, 0xada7, &analyser->mHistory1F100[5], 0);
    CashWindow(inner, 0xadaa, &analyser->mHistory1F100[5], 0xc);
    CashWindow(inner, 0xada6, &analyser->mHistory1F100[2], 0);
    CashWindow(inner, 0xadab, &analyser->mHistory1F100[2], 0xc);
    CashWindow(inner, 0xadb1, &analyser->mHistory1F100[0], 0);
    CashWindow(inner, 0xadb2, &analyser->mHistory1F100[0], 0xc);
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the year end panel (a strong definition: the draft in another file is a weak empty body)
void RemoveYearEndPanel() { UI_SendMessage(gYearEndPanel, 4, 0, 0); }
