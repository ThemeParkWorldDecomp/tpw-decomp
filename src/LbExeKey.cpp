// LbExeKey.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions
// at 0x10014bf0..0x10014c50; original file name unknown, see LbExeKey.h).
#include "LbExeKey.h"

TbExeKey gLbExeKey = {0x890cd511u, 0, 0xe92a80c5u};
bool gLbExeKeyInitialised;
uint32_t gLbExeKeyState;

uint32_t LbExeKey_ReferenceNumber() {
    return gLbExeKey.mReferenceNumber;
}

// The first call clears a word and sets a guard byte: the code CodeWarrior emits for the
// dynamic initialisation of a function-local static (presumably an object whose
// constructor zeroes its one field). Nothing in the library reads the word.
void LbExeKey_Initialise() {
    if (gLbExeKeyInitialised) return;
    gLbExeKeyState = 0;
    gLbExeKeyInitialised = true;
}
