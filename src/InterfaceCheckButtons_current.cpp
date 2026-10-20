// InterfaceCheckButtons::SetCurrent (main app, SimTheme Park 2.04b3, 0x101cc148, 252 bytes; the inventory files it under
// ui_cityLobbyController.cpp): selects one of the check buttons. Unit w43b.
#include "ui_controls.h"

namespace {

constexpr int8_t kOff = 0;
constexpr int8_t kOn = 1;

}  // namespace

// The buttons are the window's children, found by id. A button that is not there is skipped (the id -1 can be a child's too). QUIRKS: the
// answer is the old id cut to a short; with no child of the new id nothing changes, not even for the old button.
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    if (id != mCurrent) {
        if (id == -1) {
            InterfaceButton* old = static_cast<InterfaceButton*>(GetWindow(mCurrent));
            if (old != nullptr) old->SetState(kOff, 1);
            mCurrent = -1;
        } else if (GetWindow(id) != nullptr) {
            const int32_t previous = mCurrent;
            InterfaceButton* old = static_cast<InterfaceButton*>(GetWindow(previous));
            if (old != nullptr) old->SetState(kOff, 1);
            mCurrent = id;
            InterfaceButton* current = static_cast<InterfaceButton*>(GetWindow(mCurrent));
            if (current != nullptr) current->SetState(kOn, 1);
            return static_cast<int16_t>(previous);
        }
    }
    return -1;
}

// unit w47. 0x101cc108, 8 bytes: the word at 0x138, with no test of any kind (the check buttons' current id, -1 for none).
int32_t InterfaceCheckButtons::GetCurrent() { return mCurrent; }
