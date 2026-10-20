#include "ui_iterators.h"
#include "Debug.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_ime.h"
#include "AdvisorScoring.h"

#include <cstring>

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
