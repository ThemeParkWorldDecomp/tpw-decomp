// ui_staffPanel.cpp (main): InitStaffPanel 0x101c02fc (688 bytes) (unit w118; DeInitStaffPanel is in src/ui_staffPanel.cpp, unit w116).
//
// Builds the two staff scripts under the root window and leaves them ready for the staff screens: the staff panels 0x95c and 0x963 and
// the location panel 0x146 are taken out of the root's list of windows (unlink) and hidden. Every window global is reloaded before each use
// and nothing the builds answer is tested for NULL; gStaffPanelState is zeroed last.
#include <cstdint>
#include <new>

#include "ui_DrawObject.h"
#include "ui_main.h"

int32_t gStaffPanelState;   // 0x10395a38

void InitStaffPanel() {
    gUIRootWindow->Build(gStaffPanelScriptA, nullptr);   // (the answer is dropped)
    gStaffPanelA = gUIRootWindow->GetWindow(0x95c);
    gStaffPanelB = gUIRootWindow->GetWindow(0x963);
    gStaffPanelA->SetCallbackFunction(StaffPanelMsg);
    gStaffPanelB->SetCallbackFunction(StaffPanelMsg);
    gStaffPanelA->unlink();
    gStaffPanelB->unlink();
    gStaffPanelA->setExtraParam(0, reinterpret_cast<void*>(3));
    gStaffPanelB->setExtraParam(0, reinterpret_cast<void*>(3));
    gStaffPanelA->GetWindow(0x95d)->SetCallbackFunction(ModifiedCheckButtonsCallback);
    gStaffPanelB->GetWindow(0x95d)->SetCallbackFunction(ModifiedCheckButtonsCallback);
    gStaffPanelA->Show(0);
    gStaffPanelB->Show(0);
    gStaffPanelC = gUIRootWindow->Build(gStaffPanelScriptB, StaffLocMsg);
    gStaffPanelC->setCursor(2);
    gStaffPanelC->unlink();
    // the location panel's text 0x147: its own text sprite, white, font 11, no text
    InterfaceWindow* text = gStaffPanelC->GetWindow(0x147);
    InterfaceTextObjectSprite* sprite = new (std::nothrow) InterfaceTextObjectSprite(nullptr);
    text->SetClientDrawObject(sprite);
    text->SetFontColour(255, 255, 255, 255);
    text->SetFont(GetFontTableEntry(0xb));
    text->SetClientDrawText(nullptr);
    text->Enable(0);
    gStaffPanelC->GetWindow(0x148)->Enable(0);
    // the highlight 0x149 draws the library's "hilight" object
    InterfaceWindow* highlight = gStaffPanelC->GetWindow(0x149);
    char hilight[] = "hilight";   // 0x1024e642
    InterfaceDrawObject* object = DrawObjectLibrary::FindDrawObject(hilight);
    highlight->SetDrawObject(object);
    highlight->Enable(0);
    highlight->Show(0);
    gStaffPanelC->Show(0);
    gStaffPanelState = 0;
}
