// ui_upgradeStatus.cpp: name proven by __sinit_ui_upgradeStatus_cpp in the main application.

#include "analyser.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "helpers/ui_showStatusHelpers.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>

// The staff, show and upgrade status panels' view-window functions and the small free functions around them (unit v51051), main app.

// ---- UpgradeStatusPanel
void UpgradeStatusPanel::InitViewWindow() {}   // 0x101c5c80
void UpgradeStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101c5cc0
void UpgradeStatusPanel::UpdateViewWindow() { UpdateRidePreview(this); }   // 0x101c5d20

CThing* gUpgradeStatusThing = nullptr;       // 0x10395b48
int16_t gUpgradeStatusScript[] = {TPW_WINDOW_SCRIPT_102AF2AC};   // 0x102af2ac: the image's words (generated, devtools/gen_game_tables.py)

namespace {

Ptr32<InterfaceWindow>& PanelField(UpgradeStatusPanel* panel, int offset) {
    return *reinterpret_cast<Ptr32<InterfaceWindow>*>(reinterpret_cast<uint8_t*>(panel) + offset);
}

// The three text/number children are set up alike: draw object, font 6 (and text or the value 0).
void SetUpText(InterfaceWindow* window, int32_t stringId) {
    void* memory = lender_callee::OperatorNew(0x19c);
    lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(memory)));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

}  // namespace

void UpgradeStatusPanel::InitStatusWindow() {
    Ptr32<InterfaceWindow>& field = PanelField(this, 0x10);
    if (field.mOffset == 0) return;
    InterfaceWindow* window = field.get()->GetWindow(0x5211);
    if (window != nullptr) SetUpText(window, 0x37);

    window = field.get()->GetWindow(0x5210);
    if (window != nullptr) {
        void* memory = lender_callee::OperatorNew(0x19c);
        lender_callee::NumberCtor(memory);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceNumberTextObject*>(memory)));
        window->SetFont(GetFontTableEntry(6));
        lender_callee::WindowSetValue(window, 0);
    }

    window = field.get()->GetWindow(0x520f);
    if (window != nullptr) SetUpText(window, 0x38);

    window = field.get()->GetWindow(0x520e);
    if (window != nullptr) {
        void* memory = lender_callee::OperatorNew(0x19c);
        lender_callee::CashCtor(memory);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceCashTextObject*>(memory)));
        window->SetFont(GetFontTableEntry(6));
        lender_callee::WindowSetValue(window, 0);
    }
}

void UpgradeStatusPanel::UpdateStatusWindow() {
    CThing* thing = gStaffRoomPanelThing;
    CObject* object = reinterpret_cast<CObject*>(thing);
    object->GetDBInfo();
    Ptr32<InterfaceWindow>& field = PanelField(this, 0x10);
    if (field.mOffset == 0) return;
    InterfaceWindow* window = field.get()->GetWindow(0x5210);
    if (window != nullptr) {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(thing) + 0xe);
        int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
        lender_callee::WindowSetValue(window, count);
    }
    // QUIRK: no NULL check on this one.
    window = field.get()->GetWindow(0x520e);
    uint32_t value = object->GetCurrentResidualValue();
    lender_callee::WindowSetValue(window, static_cast<int32_t>(value));
}

void UpgradeStatusPanel::InitWindow() {
    int32_t listed = reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gUpgradeStatusThing);
    if (listed < 2) {
        PanelField(this, 0xc).get()->GetWindow(0x520a)->Show(0);
        PanelField(this, 0xc).get()->GetWindow(0x520b)->Show(0);
    }
    reinterpret_cast<UIStatusPanel*>(this)->InitEditBox(0x5212);
}

// ui_upgradeStatus.cpp: the upgrade status panel's window handler (UpgradeStatusMsg, 0x101c63b4) and ui_staffStatus.cpp's static
// initialiser (__sinit_ui_staffStatus_cpp, 0x101c5bec). Unit v5548.

int32_t UpgradeStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    UpgradeStatusPanel* panel = &gUpgradeStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message < 0x15) {
        if (message == 0x10) {
            if (static_cast<uint32_t>(a) == 0x80080u) {
                CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
                object->GetDBInfo();
                if (fields->mF10.mOffset != 0) {
                    InterfaceWindow* countWindow = fields->mF10->GetWindow(0x5210);
                    if (countWindow != nullptr) {
                        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
                        id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0xe);
                        int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
                        lender_callee::WindowSetValue(countWindow, count);
                    }
                    InterfaceWindow* valueWindow = fields->mF10->GetWindow(0x520e);
                    lender_callee::WindowSetValue(valueWindow, static_cast<int32_t>(object->GetCurrentResidualValue()));
                }
            }
        } else if (message < 0x10) {
            if (message == 5) {
                panel->Remove();
                return 1;
            }
        } else if (message >= 0x14) {
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12d));
        }
    } else if (message == 0x100) {
        if (a == 0x520b) {
            base->GetNextObject();
        } else if (a < 0x520b) {
            if (a == 0x5209) {
                base->EraseObject();
                return 0;
            }
            if (a < 0x5209) {
                if (a == -1) {
                    panel->Remove();
                    return 0;
                }
            } else {
                base->GetPreviousObject();
            }
        } else {
            if (a == 0x5214) {
                base->Move();
                return 0;
            }
            if (a < 0x5214 && a >= 0x5213) {
                CreateAllItemsPanel(kUIObjRide, nullptr);
                return 0;
            }
        }
    } else if (message < 0x100 && message == 0x1e) {
        UIMain_TickRidePreview();
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

void UpgradeStatusPanel::Show(CThing* thing) {
    gUpgradeStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gUpgradeStatusScript, UpgradeStatusMsg, 0x520c, 0x520d, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12d));
}

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the upgrade status panel
void UpgradeStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }

namespace {

using UpgradeSlotFn = uint64_t (*)(void*);

// The function in the panel's table at +0x20: the table holds, at this byte offset, the arena offset of a block whose first 8
// bytes are the function (the host's way of a virtual call on a Mac-layout object, see UIStatusPanelFields).
UpgradeSlotFn UpgradeSlot(Ptr32<uint32_t> table, uint32_t offset) {
    Ptr32<UpgradeSlotFn> block;
    block.mOffset = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(table.get()) + offset);
    return *block.get();
}

}  // namespace

void UpgradeStatusPanel::DeInitStatusWindow() {}
void UpgradeStatusPanel::DeInitWindow() {}
// The text of the panel (slot 0x3c) goes to the window at +0x1c (its SetText), then slots 0x20 and 0x1c run.
void UpgradeStatusPanel::UpdateWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    uint64_t text = UpgradeSlot(fields->mVtable, 0x3c)(this);
    Ptr32<InterfaceWindow> textWindow;
    textWindow.mOffset = fields->mF1C;
    textWindow.get()->SetText(reinterpret_cast<char16_t*>(text));
    UpgradeSlot(fields->mVtable, 0x20)(this);
    UpgradeSlot(fields->mVtable, 0x1c)(this);
}

// Unit v51007: ShowStatusPanel::UpdateWindow (0x101c21c4), __sinit_ui_upgradeStatus_cpp (0x101c6738), YearEndPanelMsg (0x101c67b4).

// gUpgradeStatusPanel's constructor: the base constructor, the table at +0x20 (0x102af4dc) and the mode 0x3ec.
void __sinit_ui_upgradeStatus_cpp() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gUpgradeStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gUpgradeStatusPanel));
    fields->mVtable.set(UpgradeStatusPanelTable());
    fields->mMode = 0x3ec;
}
