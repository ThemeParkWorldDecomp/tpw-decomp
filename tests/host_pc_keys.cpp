// host_pc_keys.cpp: the keys each build gets (the port's fix pc-keys, src/game/keyboard.cpp). Each system keeps its own original's
// controls: the Windows and Linux builds with the port's fixes have the PC original's, as its table and its Key Command Card give
// them, and the Mac build, or any build without the fixes, has the Mac original's. The two differ in one key: the PC clears land
// with Delete, the Mac with F7. Checked against the seven real static tables the game is built with:
//   every key of every table is the Mac original's, but for the one row of the build's own system;
//   every row of the Key Command Card that is a table's key reaches its action, with the key and modifiers HandleTyping makes of it;
//   the PC build's Delete is in no other table (the debug and camera tables are asked before the game's) and its F7 in none
//   of the four the park's window asks, the Mac build's the other way; Ctrl-F7 (delete a ride, the debug table) stays on both;
//   the keys get there: SDL's Delete is the extended VK_DELETE (0x2e00), never the period (46), which turns an object.
// The program is linked as tpw_launcher is, with its own stand-ins, so it needs no slicer and is made wherever the launcher is,
// Windows CI among them. Run by ctest ("pc_keys"). Exits nonzero on a failure.
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

#include "keyboard.h"
#include "keytable_actions.h"
#include "platform/macdoze.h"

// platform/platform.h's key translation (platform/keys.cpp). That header is not included: it brings the C library's pause(), which
// is not the game's pause key.
namespace tpw {
uint8_t MacKeyCodeFromScancode(int32_t sdlScancode);
uint16_t MacModifiersFromSDL(uint16_t sdlModifiers);
}

// TPW_HOST_PC_KEYS_FIXED is 1 when the game's sources are built with the port's fixes (CMakeLists.txt sets it from the
// option). It is not the fixes' own macro, which the published translation's copy of this file would lose.
#if TPW_HOST_PC_KEYS_FIXED && !defined(__APPLE__)
constexpr bool kPcKeys = true;    // Windows and Linux with the port's fixes: the PC original's keys
#else
constexpr bool kPcKeys = false;   // the Mac, or a build without the fixes: the Mac original's keys
#endif

static int gStandIns = 0;

// What the stand-ins report, launcher/main.cpp's in the game. None is reached here.
void tpw_stub_called(const char* name, bool platform, bool first) {
    (void)platform;
    if (first) std::printf("a stand-in was called: %s\n", name);
    gStandIns++;
}

namespace {

int gChecks = 0;
int gFailures = 0;

void Check(bool ok, const std::string& what) {
    gChecks++;
    if (ok) return;
    gFailures++;
    std::printf("FAILED: %s\n", what.c_str());
}

// The Mac original's tables (id, key, modifiers), in their order. The PC original's are the same in the tables the PC has,
// but for the game table's row 3.
struct Row {
    int16_t id, key, mods;
};
const Row kSystem[12] = {
    {0, 80, 0}, {1, 72, 12}, {2, 119, 0}, {3, 119, 63}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0}, {0, 0, 0},
};
const Row kGame[15] = {
    {0, 27, 0}, {1, 44, 0}, {2, 46, 0}, {3, 118, 0}, {4, 114, 0}, {5, 8, 0}, {6, 27904, 0}, {7, 27392, 0}, {8, 27136, 0},
    {9, 67, 12}, {10, 79, 12}, {11, 76, 48}, {12, 83, 48}, {13, 112, 0}, {14, 86, 48},
};
const Row kCamera[25] = {
    {0, 9728, 0}, {1, 10240, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}, {5, 0, 0}, {6, 9472, 0}, {7, 9984, 0}, {8, 0, 0}, {9, 0, 0},
    {10, 0, 0}, {11, 0, 0}, {12, 0, 0}, {13, 104, 0}, {14, 98, 0}, {15, 100, 0}, {17, 32, 63}, {18, 105, 0}, {19, 103, 0},
    {20, 101, 0}, {21, 97, 0}, {22, 99, 0}, {16, 102, 0}, {23, 116, 12}, {24, 9216, 0},
};
const Row kCheat[42] = {
    {0, 122, 48}, {1, 123, 48}, {2, 122, 3}, {3, 123, 3}, {4, 123, 12}, {5, 123, 15}, {6, 123, 63}, {7, 122, 63}, {8, 76, 3},
    {9, 86, 3}, {10, 89, 3}, {11, 82, 48}, {12, 82, 51}, {13, 49, 3}, {14, 49, 15}, {15, 49, 63}, {16, 49, 51}, {17, 48, 51},
    {18, 53, 3}, {19, 54, 3}, {20, 55, 3}, {21, 56, 3}, {22, 57, 3}, {23, 48, 3}, {24, 97, 12}, {25, 98, 12}, {26, 99, 12},
    {27, 100, 12}, {28, 49, 48}, {29, 71, 48}, {30, 97, 48}, {31, 98, 48}, {32, 99, 48}, {33, 100, 48}, {34, 101, 48},
    {35, 102, 48}, {36, 103, 48}, {37, 104, 48}, {38, 105, 48}, {39, 66, 3}, {40, 66, 51}, {41, 83, 60},
};
const Row kCoaster[4] = {
    {0, 67, 48}, {1, 85, 48}, {2, 27, 0}, {3, 8, 0},
};
const Row kShortcuts[17] = {
    {0, 27, 0}, {1, 66, 0}, {2, 72, 0}, {3, 73, 0}, {4, 65, 0}, {5, 83, 0}, {6, 86, 0}, {7, 70, 0}, {8, 76, 0}, {9, 84, 0},
    {10, 71, 0}, {11, 82, 0}, {12, 32, 0}, {13, 83, 12}, {14, 86, 12}, {15, 80, 12}, {16, 67, 0},
};
const Row kDebug[71] = {
    {0, 8, 48}, {1, 11520, 0}, {2, 11520, 48}, {3, 13, 3}, {4, 66, 15}, {6, 70, 48}, {7, 66, 48}, {8, 66, 12}, {9, 68, 0},
    {10, 68, 12}, {11, 0, 0}, {12, 78, 0}, {13, 78, 12}, {14, 78, 3}, {15, 85, 0}, {16, 85, 3}, {17, 0, 0}, {20, 0, 0},
    {18, 72, 3}, {19, 72, 15}, {21, 71, 3}, {23, 73, 12}, {22, 74, 0}, {24, 0, 0}, {25, 48, 12}, {26, 49, 12}, {27, 50, 12},
    {28, 51, 12}, {29, 52, 12}, {30, 53, 12}, {31, 54, 12}, {32, 55, 12}, {33, 56, 12}, {34, 57, 12}, {35, 116, 3}, {36, 91, 0},
    {37, 91, 3}, {38, 93, 0}, {39, 93, 3}, {41, 118, 12}, {42, 11520, 12}, {43, 8960, 0}, {44, 8960, 12}, {45, 8960, 3},
    {46, 8960, 15}, {47, 67, 12}, {48, 79, 12}, {49, 67, 63}, {50, 76, 15}, {51, 76, 63}, {52, 76, 51}, {53, 83, 15},
    {54, 80, 3}, {55, 87, 0}, {56, 87, 48}, {57, 13, 48}, {58, 86, 48}, {59, 71, 48}, {60, 97, 48}, {61, 98, 48}, {62, 99, 48},
    {63, 100, 48}, {64, 101, 48}, {65, 102, 48}, {66, 103, 48}, {67, 104, 48}, {68, 105, 48}, {69, 120, 12}, {70, 117, 12},
    {0, 0, 0}, {0, 0, 0},
};

constexpr int16_t kF7 = 118;          // VK_F7
constexpr int16_t kDelete = 0x2e00;   // VK_DELETE, an extended key, in the high byte
constexpr int16_t kPeriod = 46;       // '.', the same number as VK_DELETE itself
constexpr int16_t kNone = 0, kCtrl = 12;

CKeyTable gSystem{gKeyEntriesSystem, 12, 1, "system"};
CKeyTable gGame{gKeyEntriesGame, 15, 1, "game"};
CKeyTable gCamera{gKeyEntriesCamera, 25, 1, "camera"};
CKeyTable gCheat{gKeyEntriesCheat, 42, 1, "cheat"};
CKeyTable gCoaster{gKeyEntriesCoaster, 4, 1, "coaster"};
CKeyTable gShortcuts{gKeyEntriesShortcuts, 17, 1, "shortcuts"};
CKeyTable gDebug{gKeyEntriesDebug, 71, 1, "debug"};

// The entry KeyDown would take for the key (the first that matches), null for none.
const CKeyTableEntry* Find(CKeyTable& table, int16_t key, int16_t mods) {
    const int32_t index = table.Search(key, mods);
    return index < 0 ? nullptr : &table.mEntries[index];
}

void CheckTable(CKeyTable& table, const Row* rows, int16_t count) {
    for (int16_t index = 0; index < count; ++index) {
        Row want = rows[index];
        if (kPcKeys && &table == &gGame && want.id == 3) want.key = kDelete;   // the PC's one key of its own
        const CKeyTableEntry& entry = table.mEntries[index];
        Check(entry.mId == want.id && entry.mKey == want.key && entry.mModifiers == want.mods,
              std::string(table.mName) + " row " + std::to_string(index) + " is {" + std::to_string(want.id) + ", " +
                  std::to_string(want.key) + ", " + std::to_string(want.mods) + "}, not {" + std::to_string(entry.mId) + ", " +
                  std::to_string(entry.mKey) + ", " + std::to_string(entry.mModifiers) + "}");
    }
}

// A key of the card reaches its action in that table. The tables' actions are their entries' word at +0x0c, which KeyUp calls:
// the game acts when the key is let go.
void Card(const char* row, CKeyTable& table, int16_t key, int16_t mods, int32_t (*action)()) {
    const CKeyTableEntry* entry = Find(table, key, mods);
    Check(entry != nullptr && entry->mUpAction == reinterpret_cast<void*>(action),
          std::string("card \"") + row + "\": " + table.mName + " key " + std::to_string(key) + " modifiers " + std::to_string(mods));
}

// A camera key of the card: the camera reads the state of the row's id (GetState), the row has no action.
void CameraCard(const char* row, int16_t id, int16_t key) {
    const CKeyTableEntry* entry = Find(gCamera, key, kNone);
    Check(entry != nullptr && entry->mId == id, std::string("card \"") + row + "\": camera key " + std::to_string(key) + " is row id " +
                                                     std::to_string(id));
}

// The key the tables get for a Mac key code and the character typed, as HandleTyping makes it (src/game/TPMain.cpp): the
// virtual key, the extended ones (the shim's flag 0x100, and the keypad operators) in the high byte; for none, or the space bar,
// the character, upper-cased.
int16_t TableKey(uint8_t macKey, char character) {
    uint16_t flags = 0;
    const uint32_t vk = NS_MacDoze::MacToWindowsVirtualKey(macKey, flags) & 0xffu;
    int16_t key = static_cast<int16_t>(vk);
    const bool keypadOperator = (vk >= 0x6a && vk <= 0x6b) || (vk >= 0x6d && vk <= 0x6f);
    if (vk == 0x20) {
        key = 0;
    } else if (keypadOperator || (flags & 0x100)) {
        key = static_cast<int16_t>(static_cast<uint16_t>(vk << 8));
    }
    if (key == 0) key = static_cast<int16_t>(character >= 'a' && character <= 'z' ? character - 'a' + 'A' : character);
    return key;
}

int16_t SdlKey(SDL_Scancode scancode, char character = 0) {
    const uint8_t macKey = tpw::MacKeyCodeFromScancode(scancode);
    Check(macKey != 0xff, std::string("SDL scancode ") + std::to_string(scancode) + " is a Mac key");
    return TableKey(macKey, character);
}

// The modifiers the tables get, as HandleTyping makes them: 3 Shift, 0xc Command or Control, 0x30 Option or Alt.
int16_t TableMods(SDL_Keymod sdl) {
    const uint16_t mac = tpw::MacModifiersFromSDL(sdl);
    int16_t mods = 0;
    if (mac & 0x200) mods |= 3;
    if (mac & 0x1100) mods |= 0xc;
    if (mac & 0x800) mods |= 0x30;
    return mods;
}

void TestTables() {
    CheckTable(gSystem, kSystem, 12);
    CheckTable(gGame, kGame, 15);
    CheckTable(gCamera, kCamera, 25);
    CheckTable(gCheat, kCheat, 42);
    CheckTable(gCoaster, kCoaster, 4);
    CheckTable(gShortcuts, kShortcuts, 17);
    CheckTable(gDebug, kDebug, 71);
}

void TestRoute() {
    Check(SdlKey(SDL_SCANCODE_DELETE) == kDelete, "SDL's Delete reaches the tables as 0x2e00");
    Check(SdlKey(SDL_SCANCODE_PERIOD, '.') == kPeriod, "SDL's period reaches the tables as 46");
    Check(SdlKey(SDL_SCANCODE_COMMA, ',') == 44, "SDL's comma reaches the tables as 44");
    Check(SdlKey(SDL_SCANCODE_F7) == kF7, "SDL's F7 reaches the tables as 118");
    Check(SdlKey(SDL_SCANCODE_BACKSPACE) == 8, "SDL's Backspace reaches the tables as 8");
    Check(SdlKey(SDL_SCANCODE_HOME) == 0x2400, "SDL's Home reaches the tables as 0x2400");
    Check(SdlKey(SDL_SCANCODE_KP_PLUS) == 0x6b00, "SDL's keypad + reaches the tables as 0x6b00");
    Check(SdlKey(SDL_SCANCODE_KP_MINUS) == 0x6d00, "SDL's keypad - reaches the tables as 0x6d00");
    Check(SdlKey(SDL_SCANCODE_KP_MULTIPLY) == 0x6a00, "SDL's keypad * reaches the tables as 0x6a00");
    Check(SdlKey(SDL_SCANCODE_SPACE, ' ') == 32, "SDL's space bar reaches the tables as 32");
    Check(TableMods(SDL_KMOD_LCTRL) == kCtrl, "Ctrl is the tables' 12");
    Check(TableMods(SDL_KMOD_RCTRL) == kCtrl, "the right Ctrl is the tables' 12");
    Check(TableMods(SDL_KMOD_LGUI) == kCtrl, "Command (the Windows key) is the tables' 12 too");
    Check(TableMods(SDL_KMOD_LALT) == 48, "Alt (Option) is the tables' 48");
}

// The Key Command Card's rows that are keys of a table, in its order (Chat's F4, Tab and Enter are the online panel's own,
// src/game/ui/ui_onlinePanel.cpp; the mouse rows are ui_main.cpp's).
void TestCard() {
    Card("Show/hide help bar: Ctrl+H", gSystem, SdlKey(SDL_SCANCODE_H, 'h'), kCtrl, ToggleHelpText);
    Card("Main menu/cancel: Esc", gShortcuts, SdlKey(SDL_SCANCODE_ESCAPE), kNone, mainmenu);
    Card("Main menu/cancel: Esc (closes the windows first)", gGame, SdlKey(SDL_SCANCODE_ESCAPE), kNone, abort_all_requesters);
    Card("Hide/unhide control panel: F3", gGame, SdlKey(SDL_SCANCODE_F3), kNone, togglegamewindow);
    Card("Open park: Ctrl+O", gGame, SdlKey(SDL_SCANCODE_O, 'o'), kCtrl, open_park);
    Card("Close park: Ctrl+C", gGame, SdlKey(SDL_SCANCODE_C, 'c'), kCtrl, close_park);
    Card("Buy: B", gShortcuts, SdlKey(SDL_SCANCODE_B, 'b'), kNone, Screen_BuyObject);
    Card("Hire: H", gShortcuts, SdlKey(SDL_SCANCODE_H, 'h'), kNone, Screen_BuyStaff);
    Card("Park info: I", gShortcuts, SdlKey(SDL_SCANCODE_I, 'i'), kNone, Screen_ParkStatus);
    Card("All attractions: A", gShortcuts, SdlKey(SDL_SCANCODE_A, 'a'), kNone, Screen_AllItems);
    Card("All staff: S", gShortcuts, SdlKey(SDL_SCANCODE_S, 's'), kNone, Screen_AllStaff);
    Card("All visitors: V", gShortcuts, SdlKey(SDL_SCANCODE_V, 'v'), kNone, Screen_AllPeeps);
    Card("Financial: F", gShortcuts, SdlKey(SDL_SCANCODE_F, 'f'), kNone, Screen_Finance);
    Card("Loans: L", gShortcuts, SdlKey(SDL_SCANCODE_L, 'l'), kNone, Screen_Loans);
    Card("Staff training budgets: T", gShortcuts, SdlKey(SDL_SCANCODE_T, 't'), kNone, Screen_StaffCosts);
    Card("Entry price: G", gShortcuts, SdlKey(SDL_SCANCODE_G, 'g'), kNone, Screen_EntryPrice);
    Card("Research: R", gShortcuts, SdlKey(SDL_SCANCODE_R, 'r'), kNone, Screen_Research);
    Card("Map: Space", gShortcuts, SdlKey(SDL_SCANCODE_SPACE, ' '), kNone, Screen_Map);
    Card("Staff locator: Ctrl+S", gShortcuts, SdlKey(SDL_SCANCODE_S, 's'), kCtrl, Screen_StaffLoc);
    Card("Visitor locator: Ctrl+V", gShortcuts, SdlKey(SDL_SCANCODE_V, 'v'), kCtrl, Screen_PeepLoc);
    Card("Postcard: Ctrl+P", gShortcuts, SdlKey(SDL_SCANCODE_P, 'p'), kCtrl, TakePostCard);
    Card("Camcorder: C", gShortcuts, SdlKey(SDL_SCANCODE_C, 'c'), kNone, CamcorderMode);
    CameraCard("Rotate: left arrow", 6, SdlKey(SDL_SCANCODE_LEFT));
    CameraCard("Rotate: right arrow", 7, SdlKey(SDL_SCANCODE_RIGHT));
    CameraCard("Zoom: up arrow", 0, SdlKey(SDL_SCANCODE_UP));
    CameraCard("Zoom: down arrow", 1, SdlKey(SDL_SCANCODE_DOWN));
    CameraCard("Scroll left: numpad 4", 15, SdlKey(SDL_SCANCODE_KP_4));
    CameraCard("Scroll right: numpad 6", 16, SdlKey(SDL_SCANCODE_KP_6));
    CameraCard("Scroll up: numpad 8", 13, SdlKey(SDL_SCANCODE_KP_8));
    CameraCard("Scroll down: numpad 2", 14, SdlKey(SDL_SCANCODE_KP_2));
    Card("Park gates: Home", gCamera, SdlKey(SDL_SCANCODE_HOME), kNone, ResetCameraPosition);
    Card("Turn clockwise: comma", gGame, SdlKey(SDL_SCANCODE_COMMA, ','), kNone, turn_object_clockwise);
    Card("Turn anticlockwise: full stop", gGame, SdlKey(SDL_SCANCODE_PERIOD, '.'), kNone, turn_object_counterclockwise);
    Card("Delete queues/tracks, delete path tile: Backspace", gGame, SdlKey(SDL_SCANCODE_BACKSPACE), kNone, backtrack);
    Card("Delete track (coaster editor): Backspace", gCoaster, SdlKey(SDL_SCANCODE_BACKSPACE), kNone, backtrack);
    Card("Speed up: numpad +", gGame, SdlKey(SDL_SCANCODE_KP_PLUS), kNone, speed_up_time);
    Card("Slow down: numpad -", gGame, SdlKey(SDL_SCANCODE_KP_MINUS), kNone, slow_down_time);
    Card("Reset speed: numpad *", gGame, SdlKey(SDL_SCANCODE_KP_MULTIPLY), kNone, normal_speed_time);
    Card("Pause: P", gSystem, SdlKey(SDL_SCANCODE_P, 'p'), kNone, pause);
    Card("Advisor: F1", gGame, SdlKey(SDL_SCANCODE_F1), kNone, context_help);
}

// Clear land: the one key the two originals differ in.
void TestClearLand() {
    CKeyTable* const park[] = {&gDebug, &gCamera, &gGame, &gShortcuts};   // what the park's window asks, in its order
    const int16_t own = kPcKeys ? kDelete : kF7;
    const int16_t other = kPcKeys ? kF7 : kDelete;
    std::printf("pc_keys: %s build, clear land is %s\n", kPcKeys ? "a Windows or Linux" : "a Mac (or no fixes)",
                kPcKeys ? "Delete" : "F7");
    Card(kPcKeys ? "Clear land: Delete" : "Clear land: F7 (the Mac's Easy Guide)", gGame, own, kNone, enter_area_delete_mode);
    for (CKeyTable* table : park) {
        if (table != &gGame) Check(Find(*table, own, kNone) == nullptr, std::string("the ") + table->mName + " table has no plain " +
                                                                          (kPcKeys ? "Delete" : "F7"));
        Check(Find(*table, other, kNone) == nullptr, std::string("the ") + table->mName + " table has no plain " +
                                                         (kPcKeys ? "F7" : "Delete"));
    }
    Card("the period still turns an object, not clears land", gGame, kPeriod, kNone, turn_object_counterclockwise);
    Card("Ctrl+F7 deletes a ride (debug, both)", gDebug, kF7, kCtrl, delete_ride);
}

}  // namespace

int main(int, char**) {
    TestTables();
    TestRoute();
    TestCard();
    TestClearLand();
    Check(gStandIns == 0, "no stand-in was called");
    std::printf("pc_keys: %d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
