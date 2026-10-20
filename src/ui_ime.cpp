// ui_IME.cpp, main app 0x101f6c84..0x101f6e84.
#include "ui_ime.h"
#include "TPMain.h"
#include "ui_controls.h"

CUIIME gUIIME;

void CUIIME::Activate(InterfaceWindow* window) {
    const IME::ActivationResult result = mIME.Activate(gMainWindow);
    if (result.mWords[0] < 0) {
        mActive = 0;
    } else {
        mActive = 1;
        mEditBox = static_cast<InterfaceEditBox*>(window);
        const int16_t capacity = static_cast<int16_t>(mEditBox->mMaxLength);
        mIME.SetMaxCompositionLength(capacity - static_cast<int16_t>(mEditBox->GetCurrentLen()));
        InvalidateStoredString(mEditBox);
    }
}

void CUIIME::DeActivate() {
    mIME.UnActivate();
    if (mEditBox != nullptr) mEditBox->LockOutMouse(false);
    mEditBox = nullptr;
    if (mOwner != nullptr) UI_SendMessage(mOwner, 4, 0, 0);
    mActive = 0;
}

void CUIIME::InvalidateStoredString(InterfaceWindow* window) {
    if (window == mEditBox) {
        mStoredLength = 0;
        mStoredInvalid = 1;
        const int16_t capacity = static_cast<int16_t>(mEditBox->mMaxLength);
        mIME.SetMaxCompositionLength(capacity - static_cast<int16_t>(mEditBox->GetCurrentLen()));
    }
}
