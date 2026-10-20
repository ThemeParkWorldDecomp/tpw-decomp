// keyboard.cpp (main app), unit w53: InitialiseKeytables 0x1015014c,
// DeInitialiseKeyTables 0x1015120c. Table objects have native entry pointers.
#include "keyboard.h"
#include "Text.h"
#include "TbITable.h"
#include <cstring>
#include <new>

namespace {
CKeyTable* MakeTable(CKeyTableEntry* entries, int16_t count, const char* name) {
    auto* table = static_cast<CKeyTable*>(::operator new(sizeof(CKeyTable), std::nothrow));
    if (table != nullptr) {
        table->mEntries = entries;
        table->mCount = count;
        table->mEnabled = 1;
        std::strncpy(table->mName, name, 15);
    }
    return table;
}

// The indexed entry is preferred even when an earlier entry has the same id.
// FormatString may change the table's fields, so read the entries after the call.
void NameKey(CKeyTable* table, int16_t id, uint32_t textId) {
    if (table->mCount <= id) return;
    char16_t buffer[2];
    static_cast<TbITable*>(gText.mResourcePointers[2])->FormatString(buffer, 2, textId);
    CKeyTableEntry* entries = table->mEntries;
    if (entries[id].mId == id) {
        entries[id].mKey = static_cast<int16_t>(buffer[0]);
        return;
    }
    for (int32_t index = 0; index < table->mCount; ++index) {
        if (entries[index].mId == id) {
            entries[static_cast<int16_t>(index)].mKey = static_cast<int16_t>(buffer[0]);
            return;
        }
    }
}
}

void InitialiseKeytables() {
    gKeyTable = MakeTable(gKeyEntriesSystem, 12, "system");
    gKeyTableGame = MakeTable(gKeyEntriesGame, 15, "game");
    gKeyTableCamera = MakeTable(gKeyEntriesCamera, 25, "camera");
    gKeyTableCheat = MakeTable(gKeyEntriesCheat, 42, "cheat");
    gKeyTableCoaster = MakeTable(gKeyEntriesCoaster, 4, "coaster");
    gKeyTableShortcuts = MakeTable(gKeyEntriesShortcuts, 17, "shortcuts");
    for (int16_t id = 1; id <= 11; ++id) NameKey(gKeyTableShortcuts, id, id - 1);
    NameKey(gKeyTableShortcuts, 16, 11);
    NameKey(gKeyTableShortcuts, 13, 12);
    NameKey(gKeyTableShortcuts, 14, 13);
    NameKey(gKeyTableShortcuts, 15, 14);
    NameKey(gKeyTableGame, 1, 18);
    NameKey(gKeyTableGame, 2, 19);
    NameKey(gKeyTableGame, 9, 16);
    NameKey(gKeyTableGame, 10, 17);
    NameKey(gKeyTable, 0, 20);
    NameKey(gKeyTable, 1, 15);
    gKeyTableDebug = MakeTable(gKeyEntriesDebug, 71, "debug");
}

// The globals deliberately retain their freed pointers.
void DeInitialiseKeyTables() {
    if (gKeyTable != nullptr) ::operator delete(gKeyTable);
    if (gKeyTableGame != nullptr) ::operator delete(gKeyTableGame);
    if (gKeyTableCamera != nullptr) ::operator delete(gKeyTableCamera);
    if (gKeyTableCheat != nullptr) ::operator delete(gKeyTableCheat);
    if (gKeyTableCoaster != nullptr) ::operator delete(gKeyTableCoaster);
    if (gKeyTableShortcuts != nullptr) ::operator delete(gKeyTableShortcuts);
    if (gKeyTableDebug != nullptr) ::operator delete(gKeyTableDebug);
}
