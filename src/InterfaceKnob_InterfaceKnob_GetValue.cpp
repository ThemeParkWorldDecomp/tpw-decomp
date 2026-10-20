// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

// ---- the knob
// The Mac's destructor stores the vtable and calls the base destructor (then deletes when the hidden flag is above 0: the
// compiler's deleting destructor). Also the key function of the knob's vtable.
InterfaceKnob::~InterfaceKnob() {}

int32_t InterfaceKnob::SetValue(int32_t value) {
    ChangeValue(value);
    mDragAngle = mAngle;
    return 1;
}

int32_t InterfaceKnob::GetValue() { return mValue; }

// ---- the image
int32_t InterfaceImage::GetValue() { return mValue; }

// ---- the light panel
InterfaceLightPanel::~InterfaceLightPanel() {}   // key function of the light panel's vtable

int32_t InterfaceLightPanel::GetValue() { return mValue; }

// ---- the help text: the first halfword of the handler (its enabled flag)
bool UI_HelpTextEnabled() { return gUIHelpText->mEnabled != 0; }

// ---- the bar
int32_t InterfaceBar::GetValue() { return mValue; }

// ---- the text box (also the key function of its vtable)
void InterfaceTextBox::DrawFunction() {}

// ---- the vary box
InterfaceVaryBox::~InterfaceVaryBox() {}   // key function of the vary box's vtable

void InterfaceVaryBox::SetInc(int32_t* increment) {
    if (increment == nullptr) return;
    mIncrement = *increment;
}

int32_t InterfaceVaryBox::SetValue(int32_t value) { return ChangeValue(value); }

int32_t InterfaceVaryBox::GetValue() { return mValue; }

// The difference of the values when both are vary boxes (the same kind number), else 0.
int32_t InterfaceVaryBox::operator-(InterfaceWindow& other) {
    const int32_t mine = WhatAmI();
    const int32_t theirs = other.WhatAmI();
    if (theirs != mine) return 0;
    return mValue - static_cast<InterfaceVaryBox&>(other).mValue;
}

// ---- the year end panel (a strong definition: the draft in another file is a weak empty body)
void RemoveYearEndPanel() { UI_SendMessage(gYearEndPanel, 4, 0, 0); }

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

void UpgradeStatusPanel::DeInitWindow() {}
void UpgradeStatusPanel::DeInitStatusWindow() {}

// ---- the window
// 0 when this window holds the keyboard already; else the key list and the window are kept and the answer is 1.
int32_t InterfaceWindow::CaptureKeys(captureKeyList* keys) {
    if (gUIKeyboardWindow == this) return 0;
    gUIKeyCapture.mWindow = this;
    gUIKeyCapture.mKeys = keys;
    return 1;
}

// The id of a timer is the plain number or the number plus 0x80080 (both bits of the mask set); no range test (as the Mac).
int32_t InterfaceWindow::stopAnimTimer(int32_t id) {
    if ((id & 0x80080) == 0x80080) id -= 0x80080;
    InterfaceTimer* timer = mAnimTimers[id];
    if (timer == nullptr) return 0;
    timer->Stop();
    return 1;
}

// ---- the progress bar's message function: the destroy message (0x14) forgets the bar, then the default handler.
int32_t ProgBarMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gUIProgressBar = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}
