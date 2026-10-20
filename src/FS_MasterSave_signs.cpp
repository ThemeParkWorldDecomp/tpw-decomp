// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3), fourth part (unit w89): the park record's sign texts, the two
// lines of its gate sign. The player may name the sign (two lines of up to 0x21 characters); otherwise the lines are the ride
// database's sign text for the park's gate object, whose number the park's balance table holds.
#include "FS_MasterSave.h"

#include "TPEngine.h"
#include "platform/msl.h"

// The work area of GetSignNameA (0x10380a08, 0x21 characters; the pointer to it is the global at 0x102637f8). GetFullName's own
// is the next one at 0x10380a4a.
char16_t gParkSignNameScratch[0x21];

// The player's text, at most 0x21 characters (not terminated when the text has that many or more, in which case the name runs on
// into the second line's array), and the park has sign names from now on.
void CFSGlobalParkData::SetSignNameA(const char16_t* text) {
    wcsncpy(mSignNameA, text, 0x21);
    mHasSignNames = 1;
}

void CFSGlobalParkData::SetSignNameB(const char16_t* text) {
    wcsncpy(mSignNameB, text, 0x21);
    mHasSignNames = 1;
}

// Without sign names the first line is the ride database's for the gate object; a balance file that cannot be read gives an
// empty text (written with the caller's size, unlike GetFullName's). With sign names it is the player's first line (an empty text
// when it is empty); a name of two or more characters that ends in two hyphens loses its last character (cut in the work area
// from a copy of at most 31 characters).
void CFSGlobalParkData::GetSignNameA(char16_t* buffer, uint32_t size) {
    if (mHasSignNames == 0) {
        if (CheckBalanceFile()) gRideDatabase.GetSignNameA(static_cast<uint16_t>(mBalance->mValues[4].mInteger), buffer, size);
        else swprintf(buffer, size, u"");
        return;
    }
    const uint32_t length = wcslen(mSignNameA);
    if (length == 0) {
        swprintf(buffer, size, u"");
        return;
    }
    if (static_cast<int32_t>(length) > 1 && mSignNameA[length - 1] == u'-' && mSignNameA[length - 2] == u'-') {
        swprintf(gParkSignNameScratch, 0x20, u"%ls", mSignNameA);
        gParkSignNameScratch[length - 1] = 0;
        swprintf(buffer, size, u"%ls", gParkSignNameScratch);
        return;
    }
    swprintf(buffer, size, u"%ls", mSignNameA);
}

// The second line: the ride database's, or the player's as it is.
void CFSGlobalParkData::GetSignNameB(char16_t* buffer, uint32_t size) {
    if (mHasSignNames == 0) {
        if (CheckBalanceFile()) gRideDatabase.GetSignNameB(static_cast<uint16_t>(mBalance->mValues[4].mInteger), buffer, size);
        else swprintf(buffer, size, u"");
        return;
    }
    swprintf(buffer, size, u"%ls", mSignNameB);
}
