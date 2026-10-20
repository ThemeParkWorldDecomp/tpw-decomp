// ShopStatusPanel::InitStatusWindow (0x101bc6c4) and ShopStatusPanel::UpdateWindow (0x101bd730), main app (ui_shopStatus.cpp).
#include <cstdint>
#include <cstring>
#include <new>
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceScrollBar.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

// 0x1024e580: the mesh names of the two gradients (the second at +0x10).
const char gShopStatusMeshNames[0x20] = "ridestatbar.wct\0happygrad.wct";

namespace {

// A figure label: sprite text object (0x19c bytes), the font table's entry 6 and the string (NULL: no text).
void TextWindow(InterfaceWindow* panelWindow, int32_t id, int32_t stringId, bool hasString) {
    InterfaceWindow* window = panelWindow->GetWindow(id);
    void* mem = lender_callee::OperatorNew(0x19c);
    lender_callee::SpriteCtor(mem);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(mem));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(hasString ? const_cast<char16_t*>(UIMain_GetString(stringId)) : nullptr);
}

// A cash figure: cash text object, font 6, the value 0 through the window's virtual SetValue.
void CashWindow(InterfaceWindow* panelWindow, int32_t id) {
    InterfaceWindow* window = panelWindow->GetWindow(id);
    void* mem = lender_callee::OperatorNew(0x19c);
    lender_callee::CashCtor(mem);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(mem));
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, 0);
}

// A gradient bar: indented client, the gradient of the mesh, the value.
void GradientWindow(InterfaceWindow* panelWindow, int32_t id, const char* mesh, int32_t value) {
    InterfaceWindow* window = panelWindow->GetWindow(id);
    UIMain_IndentClient(window, 0, 10);
    void* mem = lender_callee::OperatorNew(0x124);
    parkstatus_callee::GradientCtor(mem, mesh);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(mem));
    lender_callee::WindowSetValue(window, value);
}

}  // namespace

// 0x101bc6c4 (1892 bytes)
void ShopStatusPanel::InitStatusWindow() {
    if (mStatusWindow == nullptr) return;
    InterfaceWindow* parent = mStatusWindow;
    TextWindow(parent, 0xc075, 0x20, true);
    TextWindow(parent, 0xc071, 0x21, true);
    TextWindow(parent, 0xc072, 0x22, true);
    TextWindow(parent, 0xc073, 0x23, true);
    TextWindow(parent, 0xc074, 0x24, true);
    TextWindow(parent, 0xc07a, 0x25, true);
    TextWindow(parent, 0xc07b, 0x26, true);
    TextWindow(parent, 0xc070, 0, false);
    CashWindow(parent, 0xc076);
    CashWindow(parent, 0xc077);
    CashWindow(parent, 0xc078);
    GradientWindow(parent, 0xc079, gShopStatusMeshNames, 0x32);
    CashWindow(parent, 0xc07d);
    GradientWindow(parent, 0xc07c, gShopStatusMeshNames + 0x10, 100);
}

// 0x101bd730 (600 bytes)
void ShopStatusPanel::UpdateWindow() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    const char16_t* answer = mOps->Slot13(this);
    InterfaceWindow* window1c;
    std::memcpy(&window1c, &mF14[4], sizeof(window1c));   // the Mac's pointer at 0x1c
    parkstatus_callee::WindowSetText(window1c, answer);
    mOps->Slot6();
    mOps->Slot5();

    InterfaceWindow* window = mWindow->GetWindow(0xc082);
    if (window != nullptr) {
        InterfaceButton* button = static_cast<InterfaceButton*>(window);
        if (object->IsClosed()) button->SetState(1, 1);
        else button->SetState(0, 1);
    }

    window = mWindow->GetWindow(0xc06b);
    if (window != nullptr) {
        uint32_t quality = object->GetProductQuality();
        lender_callee::WindowSetValue(window, static_cast<int32_t>(quality / 0x32));
    }

    window = mWindow->GetWindow(0xc086);
    if (window != nullptr) lender_callee::WindowSetValue(window, object->GetAmountOfSpecialIngredient());

    InterfaceWindow* amountWindow = mWindow->GetWindow(0xc086);
    InterfaceWindow* nameWindow = mWindow->GetWindow(0xc084);
    char16_t* text = UIMain_ConvertMulti(object->GetSpecialIngredientName());
    if (text != nullptr) {
        amountWindow->Show(1);
        nameWindow->Show(1);
        nameWindow->SetClientDrawText(text);
    } else {
        amountWindow->Show(0);
        nameWindow->Show(0);
    }

    window = mWindow->GetWindow(0xc085);
    if (window != nullptr) lender_callee::WindowSetValue(window, static_cast<int32_t>(object->GetPricePerUse()));
}
