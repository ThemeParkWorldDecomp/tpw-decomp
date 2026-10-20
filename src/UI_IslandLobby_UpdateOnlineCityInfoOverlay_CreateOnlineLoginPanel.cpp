// CreateOnlineLoginPanel (0x101e27bc): makes the login dialog and fits its name and password edit boxes and the labels and title.
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "FS_OnlineLoginPanel.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

int16_t gOnlineLoginPanelScript[2] = {0, 1};     // 0x102b02fc
char16_t gOnlineLoginEmptyText[2] = {0, 0x75};   // 0x1024ee76
void (*gOnlineLoginFailureFn)() = nullptr;       // 0x10395d28

namespace {

}  // namespace

void CreateOnlineLoginPanel(void (*onFailure)()) {
    InterfaceWindow* dialog = Dialog(gOnlineLoginPanelScript, OnlineLoginPanelDlgMsg, 0);
    gOnlineLoginPanelFlag = static_cast<int32_t>(reinterpret_cast<uintptr_t>(dialog));
    gOnlineLoginFailureFn = onFailure;
    // The Mac reloads the dialog from the flag (0x10395d2c) for the three direct children; nothing changes the flag in between, and a host
    // pointer does not fit the 32-bit flag, so the local is used.
    auto PanelWindow = [dialog]() { return dialog; };

    // ---- the name edit box (0x49) and its label (0x4a)
    InterfaceEditBox* nameBox = static_cast<InterfaceEditBox*>(PanelWindow()->GetWindow(0x49));
    nameBox->SetStaticTextArea(gOnlineLoginName, 0x10);
    void* memory = lender_callee::OperatorNew(0x19c);
    InterfaceCursorPosTextObjectSprite* cursorSprite = static_cast<InterfaceCursorPosTextObjectSprite*>(memory);
    if (memory != nullptr) new (memory) InterfaceCursorPosTextObjectSprite(nullptr);
    nameBox->SetClientDrawObject(cursorSprite);
    nameBox->SetFont(GetFontTableEntry(7));
    nameBox->SetFontColour(0xff, 0xff, 0xff, 0xff);
    nameBox->SetFontJust(0, 0);
    nameBox->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    nameBox->SetText(gOnlineLoginEmptyText);

    memory = lender_callee::OperatorNew(0x19c);
    InterfaceTextObjectSprite* labelSprite = static_cast<InterfaceTextObjectSprite*>(memory);
    if (memory != nullptr) new (memory) InterfaceTextObjectSprite(nullptr);
    nameBox->GetWindow(0x4a)->SetClientDrawObject(labelSprite);
    InterfaceFont* font = GetFontTableEntry(7);
    nameBox->GetWindow(0x4a)->SetFont(font);
    char16_t* text = const_cast<char16_t*>(UIMain_GetString(0xed));
    nameBox->GetWindow(0x4a)->SetClientDrawText(text);

    // ---- the password edit box (0x4b) and its label (0x4c)
    InterfaceEditBox* passBox = static_cast<InterfaceEditBox*>(PanelWindow()->GetWindow(0x4b));
    passBox->SetStaticTextArea(gOnlineLoginPassword, 0x10);
    memory = lender_callee::OperatorNew(0x19c);
    InterfacePasswordTextObject* passSprite = static_cast<InterfacePasswordTextObject*>(memory);
    if (memory != nullptr) new (memory) InterfacePasswordTextObject(nullptr);
    passBox->SetClientDrawObject(passSprite);
    passBox->SetFont(GetFontTableEntry(7));
    passBox->SetFontColour(0xff, 0xff, 0xff, 0xff);
    passBox->SetFontJust(0, 0);
    passBox->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    passBox->SetText(gOnlineLoginEmptyText);

    memory = lender_callee::OperatorNew(0x19c);
    labelSprite = static_cast<InterfaceTextObjectSprite*>(memory);
    if (memory != nullptr) new (memory) InterfaceTextObjectSprite(nullptr);
    passBox->GetWindow(0x4c)->SetClientDrawObject(labelSprite);
    font = GetFontTableEntry(7);
    passBox->GetWindow(0x4c)->SetFont(font);
    text = const_cast<char16_t*>(UIMain_GetString(0xee));
    passBox->GetWindow(0x4c)->SetClientDrawText(text);
    nameBox->CaptureKb(0);

    // ---- the title (0x4d)
    InterfaceWindow* title = PanelWindow()->GetWindow(0x4d);
    memory = lender_callee::OperatorNew(0x19c);
    InterfaceTitleTextObject* titleSprite = static_cast<InterfaceTitleTextObject*>(memory);
    if (memory != nullptr) new (memory) InterfaceTitleTextObject(nullptr);
    title->SetClientDrawObject(titleSprite);
    title->SetFont(GetFontTableEntry(5));
    title->SetFontJust(1, 1);
    title->SetText(const_cast<char16_t*>(UIMain_GetString(0xec)));
    UI_TITLE_COLOUR(title);
}
