// keyboard.cpp (main app): CKeyTable::GetState 0x1014fff0 (156 bytes). Translated by the maintainer 2026-10-14 from the listing
//; module test_CameraMessages.
//
// The state of the entry with this id (KeyDown sets 1, KeyUp 0); the table's enabled word is not looked at. The entry at the id's
// own index is tried first, then the first entry with that id; an id out of the table or with no entry answers 0.
#include "keyboard.h"

int16_t CKeyTable::GetState(int16_t id) {
    if (id < 0 || id >= mCount) return 0;
    if (mEntries[id].mId == id) return mEntries[id].mState;
    int16_t found = -1;
    for (int32_t index = 0; index < mCount && found < 0; ++index) {
        if (mEntries[index].mId == id) found = static_cast<int16_t>(index);
    }
    if (found > -1) return mEntries[found].mState;
    return 0;
}
