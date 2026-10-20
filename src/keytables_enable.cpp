// keyboard.cpp (main app), unit w43: Enable 0x101500b8, Disable 0x101500ec and EnableAllKeytables 0x10151010.
#include "keyboard.h"

void CKeyTable::Enable() { mEnabled = 1; }

void CKeyTable::Disable() {
    mEnabled = 0;
    for (int32_t index = 0; index < mCount; ++index) mEntries[index].mState = 0;
}

// The coaster table keeps its enabled word and every entry keeps its state.
void EnableAllKeytables() {
    gKeyTable->mEnabled = 1;
    gKeyTableGame->mEnabled = 1;
    gKeyTableCamera->mEnabled = 1;
    gKeyTableCheat->mEnabled = 1;
    gKeyTableShortcuts->mEnabled = 1;
    gKeyTableDebug->mEnabled = 1;
}

// unit w53: Search 0x1014fd68. The first match wins; count is signed.
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    for (int32_t index = 0; index < mCount; ++index) {
        if (mEntries[index].mKey == key && mEntries[index].mModifiers == modifiers)
            return static_cast<int16_t>(index);
    }
    return -1;
}
