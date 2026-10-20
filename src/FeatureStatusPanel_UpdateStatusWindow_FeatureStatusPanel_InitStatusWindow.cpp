#include <cstdint>
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

// 0x1019e454 (596 bytes): the window at +0x10 has four children: 0x1587 and 0x1585 get a text sprite and a string (0x35, 0x36),
// 0x1586 gets a number object and 0x1584 a cash object, whose virtual SetValue (slot 0x24 of the table at +0x118) is called with 0.
// Every child gets font 6. The allocation, constructors and the virtual call are the allitems_post_callee functions.
void FeatureStatusPanel::InitStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (!fields->mF10) return;

    InterfaceWindow* window = fields->mF10->GetWindow(0x1587);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::SpriteCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x35)));
    }
    window = fields->mF10->GetWindow(0x1586);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::NumberCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        allitems_post_callee::WindowSetValue(window, 0);
    }
    window = fields->mF10->GetWindow(0x1585);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::SpriteCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x36)));
    }
    window = fields->mF10->GetWindow(0x1584);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::CashCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        allitems_post_callee::WindowSetValue(window, 0);
    }
}
