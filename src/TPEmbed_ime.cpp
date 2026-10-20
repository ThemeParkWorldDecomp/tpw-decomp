// TPEmbed.cpp (main app), unit w90: the three IME::CIME functions that are empty on the Mac (each is a single blr, 4 bytes):
// Activate 0x101feab0, UnActivate 0x101feae8 and SetMaxCompositionLength 0x101feb18. The class has no input method editor behind
// it in this build. Unit w92 adds the constructor 0x101fe934, the virtual destructor 0x101fe9ec and the candidate vector's
// tear_down 0x101feb58 (the lifetime section at the end).
#include "ui_ime.h"

#include <new>

// The Mac writes nothing into the result record its caller passes (the caller's stack words stay as they were, and
// CUIIME::Activate then reads the first one); the host answers a zeroed record, which CUIIME::Activate takes as "active".
IME::ActivationResult IME::CIME::Activate(GrafPort*) {
    return ActivationResult{{0, 0, 0, 0}};
}

void IME::CIME::UnActivate() {}

void IME::CIME::SetMaxCompositionLength(int32_t) {}

// ---- the lifetime of IME::CIME (unit w92)

// 0x101fe934. Stores the vtable, constructs the three strings in order (+0x08, +0x0c, +0x10 through the string library's
// constructor), then zeroes the candidate vector and the fields +0x20, +0x24, +0x28 and +0x34 and sets +0x2c to -1. The fields
// +0x04, +0x30 and +0x38..+0x137 are NOT written (they are left as they were).
IME::CIME::CIME()
    : mString08(),
      mString0C(),
      mString10(),
      mCandidates{0, 0, nullptr} {
    mF20 = 0;
    mF24 = 0;
    mF28 = 0;
    mF2C = -1;
    mF34 = 0;
}

// 0x101fe9ec. The candidate vector goes first (each list's strings, the string blocks, then the vector's block), then the strings
// +0x10, +0x0c and +0x08 (the members' destruction in reverse order of declaration does the strings).
IME::CIME::~CIME() {
    mCandidates.tear_down();
}

// 0x101feb58. When the vector has a block: for each of the `size` lists (0x20 bytes each) the lists's strings are torn down (the
// loop calls the string vector's tear_down directly, not the list's whole destructor), then the block is freed. The capacity is not
// read; a vector with no block does nothing even when its size is not zero.
template <>
void MslVector<IME::list_cand_data_t>::tear_down() {
    if (mData != nullptr) {
        for (IME::list_cand_data_t* list = mData; list < mData + mSize; ++list) list->mStrings.tear_down();
        ::operator delete(mData);
    }
}
