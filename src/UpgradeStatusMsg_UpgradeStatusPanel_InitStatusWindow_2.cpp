#include <cstdint>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

CThing* gUpgradeStatusThing = nullptr;       // 0x10395b48
int16_t gUpgradeStatusScript[2] = {1, 0};    // 0x102af2ac

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

void UpgradeStatusPanel::Show(CThing* thing) {
    gUpgradeStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gUpgradeStatusScript, UpgradeStatusMsg, 0x520c, 0x520d, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12d));
}
