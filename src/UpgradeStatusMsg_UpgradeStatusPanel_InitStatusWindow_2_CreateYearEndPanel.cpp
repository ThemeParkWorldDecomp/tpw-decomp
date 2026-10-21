// CreateYearEndPanel 0x101c68d0 (2792 bytes): builds the year end panel (script gYearEndScript, message function YearEndPanelMsg) and
// gives its windows their draw objects, fonts, texts and the park analyser's cash and number histories.
#include <cstdint>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "WindowScripts.inc"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

InterfaceWindow* gYearEndPanel = nullptr;           // 0x10395b70
int16_t gYearEndScript[] = {TPW_WINDOW_SCRIPT_102AF524};   // 0x102af524: the image's 478 words (generated, unit v5L42)

// Untranslated (0x101c67b4): a weak stand-in so that the program links.
__attribute__((weak)) int32_t YearEndPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    (void)window; (void)message; (void)a; (void)b;
    return 0;
}

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
