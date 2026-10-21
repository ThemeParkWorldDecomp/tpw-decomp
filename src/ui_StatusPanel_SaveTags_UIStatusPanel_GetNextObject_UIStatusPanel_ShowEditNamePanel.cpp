// UIStatusPanel::ShowEditNamePanel 0x10194208 (900 bytes): opens the two-edit-box name panel for the sign object the status panel shows.
#include <cstdint>

#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

int16_t gEditNamePanelScript[] = {TPW_WINDOW_SCRIPT_102A8530};   // 0x102a8530: the image's words (generated, devtools/gen_game_tables.py)

void UIStatusPanel::ShowEditNamePanel() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    Ptr32<InterfaceWindow>& windowField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 8);
    Ptr32<InterfaceWindow>& parentField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 0xc);
    if (windowField.mOffset != 0) return;
    CThing* thing = gStaffRoomPanelThing;
    if (thing == nullptr) return;
    if (reinterpret_cast<uint8_t*>(thing)[2] != 3) return;
    CObject* object = reinterpret_cast<CObject*>(thing);

    InterfaceWindow* parent = parentField.get();
    int16_t z = parent->mZ;   // the Mac's +0xdc, by name: the host window is laid out differently
    InterfaceWindow* window = parent->Build(gEditNamePanelScript, TwoEditsMsg);
    windowField.set(window);
    window->setZ(static_cast<int16_t>(z + 0x11));
    window->setExtraParam(0, thing);
    window->setExtraParam(1, this);

    InterfaceTitleTextObject* titleObject = new InterfaceTitleTextObject(nullptr);
    window->GetWindow(0x3943e)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    window->GetWindow(0x3943e)->SetFont(titleFont);
    window->GetWindow(0x3943e)->SetFontJust(1, 1);
    const char16_t* title = UIMain_GetString(0xc2);
    window->GetWindow(0x3943e)->SetText(const_cast<char16_t*>(title));
    UI_TITLE_COLOUR(window->GetWindow(0x3943e));

    InterfaceEditBox* editA = static_cast<InterfaceEditBox*>(window->GetWindow(0x3943c));
    editA->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editA->SetFont(GetFontTableEntry(7));
    editA->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editA->SetStaticTextArea(gEditSignBufA, 0xe);
    editA->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    object->GetSignNameA(gUIMainConvertBuffer, 0x3fff);
    editA->SetText(gUIMainConvertBuffer);
    editA->SelectAll();

    InterfaceEditBox* editB = static_cast<InterfaceEditBox*>(window->GetWindow(0x3943d));
    editB->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editB->SetFont(GetFontTableEntry(7));
    editB->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editB->SetStaticTextArea(gEditSignBufB, 0xe);
    editB->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    object->GetSignNameB(gUIMainConvertBuffer, 0x3fff);
    editB->SetText(gUIMainConvertBuffer);
    UIMain_DisableAllKeyTables();
    window->GetWindow(0x3943c)->CaptureKb(0);
}
