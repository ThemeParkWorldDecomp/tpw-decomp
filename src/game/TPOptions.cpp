// TPOptions.cpp: name proven by __sinit_TPOptions_cpp in the main application.

#include "OptionsBalance.h"
#include "TimeSig.h"
#include "TPDialog.h"
#include "TPOptions.h"
#include "World.h"

COptions gOptions; // __sinit_TPOptions_cpp at 0x1015f40c, object at 0x1038070c
COptions::~COptions() {}

int32_t COptions::GetNumKidSprites() const {
    if (GetOptionsSetting() == 0x4000) {
        switch (::GetNumKidSprites()) {
            case 8: return 8;
            case 16: return 6;
            case 32: return 4;
            default: return 0;
        }
    }
    switch (gOptionsBalance.mValues[24].mInteger) { // original object +0x68, NUMKIDS
        case 0: return 2;
        case 1: return 4;
        case 2: return 6;
        default: return 8;
    }
}

int32_t COptions::GetNumStaffSprites() const {
    if (GetOptionsSetting() == 0x4000) {
        switch (::GetNumKidSprites()) {
            case 8: case 16: return 2;
            case 32: return 1;
            default: return 0;
        }
    }
    return gOptionsBalance.mValues[24].mInteger == 0 ? 1 : 2;
}

// CTimeSig::Serialise (0x1015ef5c) and CTimeSig::SecondsSinceLastUpdate (0x1015f124).
// Both are defined inline in include/TimeSig.h (the class's inline members); this file includes the header so the
// unit library compiles them, and holds no other code.
