// Unit v5231: the ride list panel's row functions. RideListLCHandler 0x101b7d94 (the four category buttons), RideListLCEntryHandler
// 0x101b7f30 (the row text colour), RideListLCFormat 0x101b811c (the row window) and RideListLCPostFormat 0x101b827c (its draw objects).
// The names LCFormat and LCPostFormat exist at other addresses, so these carry the panel's prefix.
#include <cstdint>
#include <new>

#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

uint32_t gRideListFontIndex = 7;   // 0x102aced8
int32_t gRideListRowHeight;        // 0x10395984

int32_t RideListLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        int32_t type = -1;
        int32_t string = 0;
        if (a == 0x1fc) {
            type = 3;
            string = 0x7a;
        } else if (a < 0x1fc) {
            if (a == 0x1fa) {
                type = 2;
                string = 0x79;
            } else if (a > 0x1fa) {
                type = 0;
                string = 0x77;
            }
        } else if (a < 0x1fe) {
            type = 1;
            string = 0x78;
        }
        if (type >= 0) {
            ChangeInfoWindow(static_cast<UIObjType>(type));
            FillRideList(static_cast<UIObjType>(type), 1);
            UIMain_SetTitleText(gRideListPanel, 0x1fe, const_cast<char16_t*>(UIMain_GetString(string)));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t RideListLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        if (static_cast<int16_t>(b) < 0) {
            window->SetFontColour(0xe7, 0xef, 0x4b, 0xff);
        } else {
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id{static_cast<uint16_t>(b)};
            ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(id, 0);
            int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
            RideInfo* ride = gRideDatabase.GetRideInfo(id);
            CGoldenTicketControl* tickets = reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<char*>(gGameWorld) + 0x2c0);
            if (ride->Value(kRideSlot_GoldenTicketCost).mInteger != 0 && !tickets->HasBeenBought(id)) {   // the ride's slot 46, +0xc4
                if (!tickets->CanIAfford(id)) {
                    window->SetFontColour(0xff, 0, 0, 0xff);
                } else {
                    window->SetFontColour(0xff, 0xe0, 0, 0xff);
                }
            } else if (control->mUpgradeCost > balance) {
                window->SetFontColour(0xff, 0, 0, 0xff);
            } else {
                window->SetFontColour(0xff, 0xff, 0xff, 0xff);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

InterfaceWindow* RideListLCFormat() {
    if (gRideListRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(gRideListFontIndex);
        int16_t height = static_cast<int16_t>(font->GetHeight());   // the Mac calls the font's virtual at +0xc of the table at +0x10
        gRideListRowHeight = height * 0x600 / gUIRealScreenHeight + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(gRideListRowHeight));
    if (window != nullptr) {
        window->SetCallbackFunction(RideListLCEntryHandler);
        InterfaceWindow* created = window->CreateObject(1, 1, 1, 100, 0, 200, 0x32);
        created->SetCallbackFunction(RideListLCEntryHandler);
        window->CreateObject(6, 1, 2, 100, 0, 200, 0x32);
    }
    return window;
}

void RideListLCPostFormat(InterfaceWindow* window) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(memory)));
    window->SetFont(GetFontTableEntry(gRideListFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    InterfaceWindow* child = window->GetChild(0);
    memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) new (memory) InterfaceCashAndGTickTextObject(nullptr);   // 0x1018e190
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceCashAndGTickTextObject*>(memory)));
    child->SetFont(GetFontTableEntry(gRideListFontIndex));
    child->SetFontJust(2, 1);
    lender_callee::WindowSetValue(child, 0);
    InterfaceWindow* next = child->GetNext();
    next->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("i_boxtick")));
    lender_callee::WindowSetValue(next, 0);
}
