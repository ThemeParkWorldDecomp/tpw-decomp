// UI_BankruptAfterLoad 0x1018710c, 232 bytes.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_Postcard.h"
#include "ui_main.h"

void UI_BankruptAfterLoad() {
    gBankruptWindow = nullptr;
    if (GameWindowToggle_Hidden() == 0) ToggleGameWindow();
    InterfaceWindow* window = gBumholeWindow->Build(gBankruptScript, BankruptWindowMsg);
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gBankruptAnim, 0x23);
    gBumholeWindow->CaptureKb(0);
}
