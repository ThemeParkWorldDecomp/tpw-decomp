// Unit v51007: ShowStatusPanel::UpdateWindow (0x101c21c4), __sinit_ui_upgradeStatus_cpp (0x101c6738), YearEndPanelMsg (0x101c67b4).
#include <cstdint>
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "Text.h"
#include "TbITable.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "ui_controls.h"

// The year end panel's remover (0x101c73e4, 56 bytes): not translated, a weak stand-in.
__attribute__((weak)) void RemoveYearEndPanel() {}

namespace {

// The panel's table at +0x20 holds, on the host, the arena offset of a block whose first 8 bytes are a function pointer (see CallNotify
// in ShowStatusPanel_UpdateStatusWindow.cpp). The Mac calls the entry through __ptr_glue with the panel in r3.
void* TableEntry(UIStatusPanelFields* f, uint32_t slotOffset) {
    Ptr32<uint32_t> entry;
    entry.mOffset = f->mVtable.get()[slotOffset / 4];
    return entry.get();
}

void CallVoid(UIStatusPanelFields* f, uint32_t slotOffset, void* self) {
    typedef void (*Fn)(void*);
    (*reinterpret_cast<Fn*>(TableEntry(f, slotOffset)))(self);
}

}  // namespace

void ShowStatusPanel::UpdateWindow() {
    UIStatusPanelFields* f = reinterpret_cast<UIStatusPanelFields*>(this);
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);

    typedef const char16_t* (*TextFn)(void*);
    const char16_t* answer = (*reinterpret_cast<TextFn*>(TableEntry(f, 0x3c)))(this);
    Ptr32<InterfaceWindow> window1c;
    window1c.mOffset = f->mF1C;
    parkstatus_callee::WindowSetText(window1c.get(), answer);
    CallVoid(f, 0x20, this);
    CallVoid(f, 0x1c, this);

    InterfaceWindow* parent = f->mWindow.get();
    InterfaceWindow* window = parent->GetWindow(0xa08a);
    if (window != nullptr) {
        InterfaceButton* button = static_cast<InterfaceButton*>(window);
        if (object->IsClosed()) button->SetState(1, 1);
        else button->SetState(0, 1);
    }

    window = f->mWindow.get()->GetWindow(0xa08e);
    parkstatus_callee::WindowSetValue(window, static_cast<int32_t>(object->GetChanceOfWinning()));

    window = f->mWindow.get()->GetWindow(0xa08f);
    parkstatus_callee::WindowSetValue(window, static_cast<int32_t>(object->GetPricePerUse()));

    window = f->mWindow.get()->GetWindow(0xa090);
    parkstatus_callee::WindowSetValue(window, static_cast<int32_t>(object->GetPrizeValue()));

    int32_t show = object->GetDBInfo()->Value(kRideSlot_NonWinnableSideshow).mInteger != 0 ? 0 : 1;
    f->mWindow.get()->GetWindow(0xa090)->Show(show);
    f->mWindow.get()->GetWindow(0xa08e)->Show(show);
    f->mWindow.get()->GetWindow(0xa0a6)->Show(show);
    f->mWindow.get()->GetWindow(0xa0a7)->Show(show);
    const char16_t* text = UIMain_GetString(show ? 0x33 : 0x34);
    parkstatus_callee::WindowSetText(f->mWindow.get()->GetWindow(0xa0a5), text);
}

// gUpgradeStatusPanel's constructor: the base constructor, the table at +0x20 (0x102af4dc) and the mode 0x3ec.
void __sinit_ui_upgradeStatus_cpp() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gUpgradeStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gUpgradeStatusPanel));
    fields->mVtable.mOffset = 0x102af4dc;
    fields->mMode = 0x3ec;
}

int32_t YearEndPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message != 0x15) {
        if (message < 0x15) {
            if (message == 5) {
                RemoveYearEndPanel();
                return 1;
            }
            if (message > 4 && message >= 0x14) gYearEndPanel = nullptr;
        } else if (message == 0x100) {
            if (a == 0xadb4) {
                CreateParkStatusPanel();
                return 0;
            }
            if (a < 0xadb4) {
                if (a == -2) {
                    RemoveYearEndPanel();
                    return 0;
                }
            } else if (a < 0xadb6) {
                CreateFinancePanel();
                return 0;
            }
        }
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}
