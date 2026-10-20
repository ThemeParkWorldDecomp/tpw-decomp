// LbExeKey.h: bullfrog library, functions at 0x10014bf0..0x10014c50. The library names
// no file here; LbExeKey.cpp follows the functions' prefix.
//
// The key is three words of initialised data at 0x10072674: the reference number between
// two magic words, so a mastering tool can presumably find and patch it in the shipped
// binary (it is 0 in this build). The names are invented.
#pragma once
#include <cstdint>

struct TbExeKey {
    uint32_t mMagic0;            // 0x00  0x890cd511
    uint32_t mReferenceNumber;   // 0x04  0
    uint32_t mMagic1;            // 0x08  0xe92a80c5
};

extern TbExeKey gLbExeKey;            // 0x10072674
extern bool gLbExeKeyInitialised;     // 0x10073e7c (guard byte)
extern uint32_t gLbExeKeyState;       // 0x10073e80

uint32_t LbExeKey_ReferenceNumber();  // 0x10014bf0
void LbExeKey_Initialise();           // 0x10014c2c
