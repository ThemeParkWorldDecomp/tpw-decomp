// keyboard.cpp (main app), unit w50: CKeyTable::KeyDown 0x1014fdec and KeyUp 0x1014fef0 (216 bytes each).
#include "keyboard.h"

// The index of the first entry whose key and modifiers match, -1 when none does. The Mac keeps it in a 16-bit register
// (extsh) and stops the loop at the first hit, so a later entry of the same key never wins.
static int32_t FindEntry(const CKeyTable* table, int16_t key, int16_t modifiers) {
    int16_t found = -1;
    for (int32_t index = 0; index < table->mCount && found < 0; ++index) {
        const CKeyTableEntry& entry = table->mEntries[index];
        if (entry.mKey == key && entry.mModifiers == modifiers) found = static_cast<int16_t>(index);
    }
    return found;
}

// A disabled table answers 0 and touches nothing. For the first entry that matches: its down action (when it has one) is
// called with 0, then the entry's state becomes 1 (re-read through the table, so an action that moves the table is
// followed). The answer is the action's, 0 when there is no action or no entry.
int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    if (mEnabled == 0) return 0;
    const int32_t found = FindEntry(this, key, modifiers);
    if (found <= -1) return 0;
    int32_t answer = 0;
    if (mEntries[found].mAction != nullptr) answer = reinterpret_cast<CKeyAction>(mEntries[found].mAction)(0);
    mEntries[found].mState = 1;
    return answer;
}

// The same for a key released: the second action (the word at entry +0x0c) and state 0.
int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    if (mEnabled == 0) return 0;
    const int32_t found = FindEntry(this, key, modifiers);
    if (found <= -1) return 0;
    int32_t answer = 0;
    if (mEntries[found].mUpAction != nullptr) answer = reinterpret_cast<CKeyAction>(mEntries[found].mUpAction)(0);
    mEntries[found].mState = 0;
    return answer;
}
