// keyboard.cpp (main app): key tables and their static entries.
#pragma once
#include <cstdint>

// What a key's action is, once cast from the entry's pointer: a function the key calls with 0 (a TVector on the Mac) and whose answer
// KeyDown and KeyUp pass on. The entries keep it as void* (the unit libraries of other tests fill them with plain numbers).
typedef int32_t (*CKeyAction)(int32_t);

// One key of a table (0x14 bytes on the Mac; the host's is larger, its two action pointers are native). Search, KeyDown and KeyUp
// match the key and the modifiers of an entry (0x1014fd68, 0x1014fdec, 0x1014fef0); only the fields the translated code reads are named.
struct CKeyTableEntry {
    int16_t mId;          // 0x00  the entry's number (GetState looks for it)
    int16_t mKey;         // 0x02  the key
    int16_t mModifiers;   // 0x04  the modifier keys
    int16_t mState;       // 0x06  KeyDown sets 1, KeyUp sets 0; DisableAllKeytables clears it in every entry of a table it disables
    uint8_t mF08[4];      // 0x08  not read by the translated code
    void* mUpAction;      // 0x0c  KeyUp calls it when it is not NULL (unit w50)
    void* mAction;        // 0x10  KeyDown calls it when it is not NULL
    // unit w53: native pointer for the help identifier at Mac +0x08. The opaque mF08
    // bytes stay in place for existing callers that build their own entries.
    const char* mHelpText = nullptr;
};

// A table of keys (0x18 bytes on the Mac, made by InitialiseKeytables). The seven tables are named by their own name strings.
class CKeyTable {
public:
    // unit w43
    void Enable();   // 0x101500b8
    void Disable();  // 0x101500ec
    // unit w50 (src/game/keyboard.cpp): the first entry of the key and modifiers (a disabled table has none) has its action called
    // with 0 and its state set (KeyDown 1, KeyUp 0); the answer is the action's, 0 when there was none to call or no entry.
    int32_t KeyDown(int16_t key, int16_t modifiers);   // 0x1014fdec
    int32_t KeyUp(int16_t key, int16_t modifiers);     // 0x1014fef0
    int32_t Search(int16_t key, int16_t modifiers);    // 0x1014fd68, unit w53
    // lead 2026-10-14 (src/game/keyboard.cpp; a fake in the camera test): the state of the entry with this id (nonzero: held), 0 for none.
    int16_t GetState(int16_t id);   // 0x1014fff0

    CKeyTableEntry* mEntries;   // 0x00
    int16_t mCount;             // 0x04  the entries (signed: a negative count has no entries)
    int16_t mEnabled;           // 0x06  0 while the table is disabled (KeyDown then answers 0)
    char mName[16];             // 0x08  "system", "game", ... (strncpy of 15 characters)
};
extern CKeyTable* gKeyTable;          // 0x1037f234 "system" (original name unknown; the six below are named by their tables' names)
extern CKeyTable* gKeyTableGame;      // 0x1037f230 "game"
extern CKeyTable* gKeyTableCamera;    // 0x1037f22c "camera"
extern CKeyTable* gKeyTableCheat;     // 0x1037f228 "cheat"
extern CKeyTable* gKeyTableCoaster;   // 0x1037f224 "coaster" (DisableAllKeytables leaves it alone)
extern CKeyTable* gKeyTableShortcuts; // 0x1037f220 "shortcuts"
extern CKeyTable* gKeyTableDebug;     // 0x1037f21c "debug"

void InitialiseKeytables();     // 0x1015014c
void DeInitialiseKeyTables();   // 0x1015120c

// unit w53: the mutable arrays InitialiseKeytables installs (Mac addresses).
extern CKeyTableEntry gKeyEntriesSystem[12];      // 0x102a4e58
extern CKeyTableEntry gKeyEntriesGame[15];        // 0x102a4f48
extern CKeyTableEntry gKeyEntriesCamera[25];      // 0x102a5074
extern CKeyTableEntry gKeyEntriesCheat[42];       // 0x102a5998
extern CKeyTableEntry gKeyEntriesCoaster[4];      // 0x102a5268
extern CKeyTableEntry gKeyEntriesShortcuts[17];   // 0x102a52b8
extern CKeyTableEntry gKeyEntriesDebug[71];       // 0x102a540c
// 0x1015108c (src/game/keyboard.cpp): clears the enabled word of six of the seven tables (not the coaster's) and the state word of
// every entry of each; EnableAllKeytables (0x10151010, untranslated) is the other way.
void DisableAllKeytables();

// unit w43
void EnableAllKeytables();   // 0x10151010
