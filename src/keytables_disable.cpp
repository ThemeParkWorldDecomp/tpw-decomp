// DisableAllKeytables (main app, keyboard.cpp, 0x1015108c, 340 bytes). Unit w39.
//
// Disables six of the seven key tables: the enabled word of each is cleared and so is the state word of each of its entries (the
// original inlines CKeyTable::Disable, 0x101500ec, six times). The coaster table (0x1037f224) is not in the list: it stays as it is.
// The seven table pointers are made by InitialiseKeytables (0x1015014c, untranslated), which defines them too when it is translated.
#include "keyboard.h"

CKeyTable* gKeyTable;            // 0x1037f234
CKeyTable* gKeyTableGame;        // 0x1037f230
CKeyTable* gKeyTableCamera;      // 0x1037f22c
CKeyTable* gKeyTableCheat;       // 0x1037f228
CKeyTable* gKeyTableCoaster;     // 0x1037f224
CKeyTable* gKeyTableShortcuts;   // 0x1037f220
CKeyTable* gKeyTableDebug;       // 0x1037f21c

namespace {

// Clears the table's enabled word and then every entry's state word; the count is a signed short, so a negative one has no entries.
// (A table that does not exist is not tested for: the original writes through it.)
void DisableTable(CKeyTable* table) {
    table->mEnabled = 0;
    for (int32_t index = 0; index < table->mCount; ++index) table->mEntries[index].mState = 0;
}

}  // namespace

void DisableAllKeytables() {
    DisableTable(gKeyTable);
    DisableTable(gKeyTableGame);
    DisableTable(gKeyTableCamera);
    DisableTable(gKeyTableCheat);
    DisableTable(gKeyTableShortcuts);
    DisableTable(gKeyTableDebug);
}
