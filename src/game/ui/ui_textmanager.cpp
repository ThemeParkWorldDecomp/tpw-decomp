// ui_textmanager.cpp: name proven by __sinit_ui_textmanager_cpp in the main application.

#define TPW_UIMENU_CTOR_EXTERNAL
#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_OnlineLoginPanel.h"
#include "helpers/InterfaceTextObjectSpriteHelpers.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LbPalette.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbDynamicStringTemplate.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "UI_pump.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.

InterfaceRankTextObject::InterfaceRankTextObject(void* data) : InterfaceTextObjectSprite(data) {}

// The text draw objects (main app of SimTheme Park 2.04b3, 0x1018d11c..0x101902a8; the inventory files the range under
// ui_textmanager.cpp): InterfaceTextObjectSprite (constructor, destructor, UpdateTextures, Change), InterfaceCursorPosTextObjectSprite
// (constructor, destructor, Query) and InterfaceTitleTextObject (constructor, destructor, UpdateTextures). See include/ui_DrawObject.h for
// the classes. The other text objects of the game (cash, number, rank, percent...) are further subclasses made elsewhere.

// ---- InterfaceTitleTextObject

// The text object's constructor, inline (the vtable pointer goes to the text object's, then to this class's).
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite(data) {}

InterfaceTitleTextObject::~InterfaceTitleTextObject() {}

// The text object's drawing with a shadow: the text in mid grey (0x80 all over) in the box moved two pixels left and down (the right and
// bottom edges too), then in white in the box itself; the font colour of the object is not used. The formatter's rectangle out is
// the same variable for both: the second draw is given what the first reported.
void InterfaceTitleTextObject::UpdateTextures(char16_t* text) {
    TbRenderArea area;
    TbRect bounds = {0, 0, 0, 0};
    if (text == nullptr) return;
    const TextBox box = Justify(*this);
    gText.mRenderParams.SetFont(FontOf(*this));
    PrepareSurface(*this, area);
    gText.mRenderParams.UseReColour(MakeColour(0x80, 0x80, 0x80, 0x80, 0));
    const TbRect shadow = {box.mLeft - 2, box.mTop + 2, box.mRight - 2, box.mBottom + 2};
    mFormatter.Draw(shadow, UIMain_ConvertString(text), gText.mRenderParams, &bounds);
    gText.mRenderParams.UseReColour(MakeColour(0xff, 0xff, 0xff, 0xff, 0));
    const TbRect rect = {box.mLeft, box.mTop, box.mRight, box.mBottom};
    mFormatter.Draw(rect, UIMain_ConvertString(text), gText.mRenderParams, &bounds);
    FinishSurface(*this, area, bounds);
}

uint8_t gCoolRectDtor[8];        // stands for the destructor word 0x102680a0
uint8_t gCoolRectNode[16];       // 0x1038cd68 (exit record of gUIQueryRect)
// 0x101902e8 (56 bytes)
void __sinit_ui_textmanager_cpp() {
    new (&gUIQueryRect) CoolRect();
    __register_global_object(&gUIQueryRect, gCoolRectDtor, gCoolRectNode);
}
