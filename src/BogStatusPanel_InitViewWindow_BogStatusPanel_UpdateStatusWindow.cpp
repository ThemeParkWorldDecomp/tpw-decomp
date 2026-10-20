// Unit v5519: BogStatusPanel::InitStatusWindow (0x1019a12c) and UpdateStatusWindow (0x1019a504).
#include <cstdint>
#include <cstring>
#include <new>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "ppc_math.h"

// 0x10237cd8 (untranslated runtime function): a double turned into an integer.
int32_t FloatToCell(double value);

namespace bog_callee {
// 0x101898c8 InterfaceDrawEngineHGradient(mesh), the complete object constructor, named by its symbol so that the test library (which
// replaces HGradientCtor below with a recording fake) links without it.
extern "C++" void HGradientRaw(void* memory, void* mesh) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");

// The constructor built in the memory given (weak: the test library defines its own, recording one).
__attribute__((weak)) void HGradientCtor(void* memory, const char* mesh) { HGradientRaw(memory, const_cast<char*>(mesh)); }
}  // namespace bog_callee

namespace {

using PanelSlot = uint64_t (*)(void*);

// The panel's virtual call: the table at +0x20 holds the arena offset of a block holding the function.
void CallPanel(UIStatusPanelFields* fields, void* panel, uint32_t offset) {
    Ptr32<PanelSlot> block;
    block.mOffset = fields->mVtable.get()[offset / 4];
    (*block.get())(panel);
}

// A text sprite as the client draw object of a window of the status window, font table entry 6, and its text.
void SetUpLabel(UIStatusPanelFields* fields, int32_t id, int32_t stringId) {
    InterfaceWindow* window = fields->mF10->GetWindow(id);
    void* sprite = lender_callee::OperatorNew(0x19c);
    if (sprite != nullptr) lender_callee::SpriteCtor(sprite);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

}  // namespace

// 0x1019a12c (864 bytes): with a status window (+0x10): three labelled texts (strings 0x3c, 0x3d, 0x3f), a number text, the gradient
// bar and the cash text; the number and cash windows start at 0, the bar at 50.
void BogStatusPanel::InitStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (fields->mF10.mOffset == 0) return;
    SetUpLabel(fields, 0x15bbe, 0x3c);
    SetUpLabel(fields, 0x15bbc, 0x3d);
    SetUpLabel(fields, 0x15bbf, 0x3f);

    InterfaceWindow* window = fields->mF10->GetWindow(0x15bbd);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::NumberCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetValue(0);

    window = fields->mF10->GetWindow(0x15bbb);
    UIMain_IndentClient(window, 0, 10);
    object = lender_callee::OperatorNew(0x124);
    if (object != nullptr) bog_callee::HGradientCtor(object, "ridestatbar.wct");
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetValue(0x32);

    window = fields->mF10->GetWindow(0x15bc0);
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::CashCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetValue(0);
}

// 0x1019a504 (648 bytes): the ride of gStaffRoomPanelThing in the status window (customers, condition, residual value) and the
// open/close and state buttons of the panel window (+0x0c); the panel is told when the object's state changed.
void BogStatusPanel::UpdateStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->GetDBInfo();
    if (fields->mF10.mOffset != 0) {
        InterfaceWindow* window = fields->mF10->GetWindow(0x15bbd);
        if (window != nullptr) {
            double sum;
            object->mHistory1A4.GetSumOfLast(sum, 0x1e);
            window->SetValue(FloatToCell(sum));
        }
        window = fields->mF10->GetWindow(0x15bbb);
        if (window != nullptr) {
            int32_t condition = ppc_fctiwz(static_cast<double>(object->mF40));
            // rlwinm 10, 14, 21 keeps the low byte shifted by 10; the divide by 100 is the mulhw / srawi pair.
            int32_t scaled = static_cast<int32_t>((static_cast<uint32_t>(condition) & 0xffu) << 10);
            window->SetValue(scaled / 100);
        }
        // No test for NULL here: the Mac reads the window's table at once.
        window = fields->mF10->GetWindow(0x15bc0);
        uint32_t residual = object->GetCurrentResidualValue();
        window->SetValue(static_cast<int32_t>(residual));
    }
    InterfaceWindow* openButton = fields->mWindow->GetWindow(0x15bc4);
    if (object->IsOpenable() == 0) {
        openButton->Enable(0);
    } else {
        openButton->Enable(1);
        if (object->IsClosed())
            static_cast<InterfaceButton*>(openButton)->SetState(1, 1);
        else
            static_cast<InterfaceButton*>(openButton)->SetState(0, 1);
    }
    InterfaceWindow* stateButton = fields->mWindow->GetWindow(0x15bb4);
    int32_t state = UIMain_GetObjectState(object, 2);
    if (static_cast<uint32_t>(state - 0xb) <= 4u)
        static_cast<InterfaceButton*>(stateButton)->SetState(1, 1);
    else
        static_cast<InterfaceButton*>(stateButton)->SetState(0, 1);
    if (state != fields->mF04) {
        fields->mF04 = state;
        fields->mWindow->GetWindow(0x15bb4);
        CallPanel(fields, this, 0x2c);
    }
}
