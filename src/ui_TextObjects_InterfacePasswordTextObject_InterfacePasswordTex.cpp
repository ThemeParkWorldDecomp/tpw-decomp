// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.
#define TPW_UIMENU_CTOR_EXTERNAL
#include <cstdint>
#include <cstring>
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "FS_OnlineLoginPanel.h"
#include "FillLenderList.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_menu.h"
#include "ui_mainmenu.h"
#include "MainWindowCallback.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"

// 0x1018dc38: the cursor position text object's constructor, then its own vtable (0x102a81ac).
InterfacePasswordTextObject::InterfacePasswordTextObject(void* data) : InterfaceCursorPosTextObjectSprite(data) {}

// 0x1018de60 (unit v5L16), 0x1018e594, 0x1018e874, 0x1018eb38, 0x1018edf4, 0x1018f93c: the text sprite's constructor (sprite
// constructor, formatter, justification 0, 1, wrap 0, offsets 0, font -1, colour mode 0, colour 0xff 0xff 0xff), then each class's
// vtable.
InterfaceCashTextObject::InterfaceCashTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceNullCashTextObject::InterfaceNullCashTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfacePercentTextObject::InterfacePercentTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceVisitorNumTextObject::InterfaceVisitorNumTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceVisitorTimeTextObject::InterfaceVisitorTimeTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceRankTextObject::InterfaceRankTextObject(void* data) : InterfaceTextObjectSprite(data) {}

// 0x1018df3c, 0x1018e674, 0x1018e950, 0x1018ec18, 0x1018eed4: the vtables stepped down, the formatter, the sprite list, the engine data and
// the draw object destroyed in that order (the host's members and bases are destroyed in the same order); the delete flag is the caller's.
InterfaceCashTextObject::~InterfaceCashTextObject() {}
InterfaceNullCashTextObject::~InterfaceNullCashTextObject() {}
InterfacePercentTextObject::~InterfacePercentTextObject() {}
InterfaceVisitorNumTextObject::~InterfaceVisitorNumTextObject() {}
InterfaceVisitorTimeTextObject::~InterfaceVisitorTimeTextObject() {}

// 0x1018ac44: word 0 the default item handler (the host keeps a handle), 4 zero, 8 -1, 0xc zero, 0x10 zero.
UIMenu::UIMenu() {
    mWindow = nullptr;
    mF00 = UIMenuHandlerHandle(UIMenu_DefaultItemCallback);
    mF04 = 0;
    mF08 = 0xffffffffu;
    mShown = 0;
}

// 0x1018a8fc, 0x1018a8c4: nothing.
void InterfaceTextBox::ClientDraw() {}
void InterfaceBar::DrawFunction() {}

// 0x10189df8: the vertical gradient's Show (the mesh render follows the state).
void InterfaceDrawEngineHGradient::Show(int16_t shown) {
    if (shown != mShown) {
        if (shown != 0) {
            DMEnableMeshRender(&mMeshSet);
        } else {
            DMDisableMeshRender(&mMeshSet);
        }
        mShown = shown;
    }
}

// 0x10199ff4, 0x1019a4c4, 0x1019a894, 0x1019a978.
void BogStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }
void BogStatusPanel::DeInitStatusWindow() {}
void BogStatusPanel::DeInitWindow() {}
void BogStatusPanel::ApplyChanges() {}

// 0x10180fc0: the colour of an object state (no bounds check; the Mac answers through a hidden pointer).
uint32_t UIMain_GetObjectStateTextColour(int32_t state) { return gUIObjectStateColours[state]; }
