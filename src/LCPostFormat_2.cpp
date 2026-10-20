// The online outbox panel (ui_onlineOutbox.cpp, main app; unit v5096): CreateOnlineOutboxPanel 0x101e4f28 and its list's row functions,
// LCFormat 0x101e4bdc and LCPostFormat 0x101e4d20 (named OutboxLCFormat and OutboxLCPostFormat here: the same names exist at other addresses).
#include <cstdint>

#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

// 0x101e4bdc: a row window as high as the font (in the 1536 units, plus 6) and 100 wide, with three objects made in it.
// The font's height is a virtual call on the Mac (slot at 0xc of the vtable at 0x10), answered as a short.
InterfaceWindow* OutboxLCFormat() {
    InterfaceFont* font = GetFontTableEntry(7);
    int32_t height = static_cast<int16_t>(font->GetHeight());
    int32_t bottom = height * 0x600 / gUIRealScreenHeight + 6;
    InterfaceWindow* window = new InterfaceWindow(nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(bottom));
    if (window != nullptr) {
        window->CreateObject(1, 1, 1, 100, 0, 200, 50);
        window->CreateObject(6, 1, 2, 100, 0, 200, 50);
        window->CreateObject(6, 1, 2, 100, 0, 200, 50);
    }
    return window;
}

// 0x101e4d20: the row gets a text object, white, font 7; its first child a centred wrapped text object; the next two children
// the "i_boxtick" picture.
void OutboxLCPostFormat(InterfaceWindow* window) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    window->SetFont(GetFontTableEntry(7));
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    sprite = new InterfaceTextObjectSprite(nullptr);
    child->SetClientDrawObject(sprite);
    child->SetFontJust(1, 1);
    child->SetFontWrap(1, 1);
    child->SetFont(GetFontTableEntry(7));
    child->SetFontColour(0xff, 0xff, 0xff, 0xff);
    child->SetClientDrawText(nullptr);
    child = child->GetNext();
    child->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("i_boxtick")));
    child = child->GetNext();
    child->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("i_boxtick")));
}

// 0x101e4f28: see lobby.h.
int32_t CreateOnlineOutboxPanel() {
    int32_t result = 0;
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
    if (gUserInfo->mOutbox.mF28.size() != 0) {   // the Mac's word at 0x8c
        gOutboxWindow = Dialog(gOutboxPanelData, OutboxPanelMsg, 1);
        InterfaceWindow* window = gOutboxWindow->GetWindow(0xee2);
        window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(nullptr);
        window->SetFontWrap(1, 0);
        window->SetFontJust(0, 0);
        window->SetCallbackFunction(AdressesTextPanelMsg);
        window = gOutboxWindow->GetWindow(0xee4);
        window->SetClientDrawObject(new InterfaceDrawObjectSprite(nullptr));
        TbRenderArea area(TbRenderArea::kCleared);   // the Mac's is not constructed; its address is all AllocMemory gets
        area.AllocMemory();
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
        list->SetColumnFormatsAll(OutboxLCFormat, OutboxLCPostFormat);
        list->SetColumnType(0, kLCText);
        list->SetColumnType(1, kLCText);
        list->SetColumnType(2, kLCNumber);
        list->SetColumnType(3, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        MAKECOLUMNBUTTON(list->GetWindow(0x10), const_cast<char16_t*>(UIMain_GetString(0x136)), 8, 0xff, 0xef, 0, 0x187);
        MAKECOLUMNBUTTON(list->GetWindow(0x11), const_cast<char16_t*>(UIMain_GetString(0x137)), 8, 0xff, 0xef, 0, 0x188);
        MAKECOLUMNBUTTON(list->GetWindow(0x12), const_cast<char16_t*>(UIMain_GetString(0x138)), 8, 0xff, 0xef, 0, 0x189);
        MAKECOLUMNBUTTON(list->GetWindow(0x13), const_cast<char16_t*>(UIMain_GetString(0x139)), 8, 0xff, 0xef, 0, 0x18a);
        UIMain_SetTitleText(gOutboxWindow, 0xee9, const_cast<char16_t*>(UIMain_GetString(0x135)));
        FillOutboxList();
        gOutboxWindow->CaptureKb(0);
        result = 1;
        gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x255}, 1);
    }
    return result;
}
