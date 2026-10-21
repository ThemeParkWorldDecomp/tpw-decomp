// ui_IME.cpp: name proven by __sinit_ui_IME_cpp in the main application.

#include "AdvisorScoring.h"
#include "Debug.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "TPMain.h"
#include "ui_controls.h"
#include "ui_ime.h"
#include "ui_iterators.h"
#include "World.h"
#include <cstring>

// ui_IME.cpp, main app 0x101f6c84..0x101f6e84.

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

uint32_t gUIIMESinitDtorTV[2];   // stand-in for the TVector of CUIIME::~CUIIME, 0x10268600
uint32_t gUIIMESinitNode[3];     // 0x10396a40: the __register_global_object record

void __sinit_ui_IME_cpp() {
    CUIIME* self = &gUIIME;
    new (&self->mIME) IME::CIME();
    // +0x154 (mIME +0x138): first word of the candidate list; +0x164..+0x16c: its vector (capacity, size, data)
    std::memset(self->mCandidates.mF00, 0, 4);
    self->mCandidates.mStrings.mCapacity = 0;
    self->mCandidates.mStrings.mSize = 0;
    self->mCandidates.mStrings.mData = nullptr;
    uint32_t zero = 0;
    uint32_t minusOne = 0xffffffffu;
    std::memcpy(self->mF08, &zero, 4);        // 0x08
    self->mOwner = nullptr;                   // 0x00
    self->mActive = 0;                        // 0x0c
    std::memcpy(self->mF0D + 3, &minusOne, 4);    // 0x10
    __register_global_object(self, gUIIMESinitDtorTV, gUIIMESinitNode);
}
