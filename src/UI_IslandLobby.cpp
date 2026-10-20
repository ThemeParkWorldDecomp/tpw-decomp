// ui_islandLobbyController.cpp of the main app (SimTheme Park 2.04b3), unit w32: the island lobby's controller and the golden
// key display, plus the mail icon (UpdateOutboxMailIcon, which the inventory files under ui_onlineOutbox.cpp). The controller is a
// small window tree built under the lobby's window; it shows the golden keys the player holds, the keys the current island
// costs, the island's name and two buttons. The island screen creates, shows and updates it (src/lobby_islands.cpp).
#include "Advisor.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "rsse.h"
#include "ui_main.h"
#include "ui_textmanager.h"

InterfaceWindow* gIslandLobbyController;   // 0x10395cbc
int32_t gIslandLobbyKeyEmitter;            // 0x10395cb4
int32_t gIslandLobbyKeysShown;             // 0x10395cb8

// The description of the controller's windows, for InterfaceWindow::Build (the format is described in src/InterfaceWindow_script.cpp;
// the lobby's own is in src/lobby.cpp). The controller is the panel 0x1e0e7 (28, 855, 476, 1521), built under the lobby's panel and
// given IslandLobbyControllerMsg by Build. Under it: two buttons (0x1e0e8 and 0x1e0e9, with help texts 0x159 and 0x15a), the key
// cost panel 0x1e0ea with its digit 0x1e0eb, the key count panel 0x1e0ec with the key icon 0x1e0ed and the count's text window
// 0x1e0ee, the park name 0x1e0ef, the previous and next island buttons 0x1e0f0 and 0x1e0f1 (help texts 0x157 and 0x158), then the
// panel's own region (a 23 point polygon round the buttons and the park name), client rectangle and client draw object. The names
// in the comments are invented. The words are the ones in the program image at 0x102b006e (the test compares all 269).
int16_t gIslandLobbyWindowData[] = {
    0, 1, 1, 0, -7961, 1, 28, 855, 476, 1521,   // 0: child, kind 1 (window), attributes 0x1, id 0x1e0e7 (the controller panel), rect (28, 855, 476, 1521)
      17, -1, -1,   // 17: help text number -1
      18, -1, -1,   // 18: word at +0x90 is -1
      0, 2, 1, 0, -7960, 1, 68, 1114, 221, 1267,   // 0: child, kind 2 (button), attributes 0x1, id 0x1e0e8 (enter button, help 0x159), rect (68, 1114, 221, 1267)
        17, 345, 0,   // 17: help text number 345 (0x159)
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -23348, -28604,   // 1: draw object 0x9044a4cc
        5,   // 5: end of this level
      0, 2, 1, 0, -7959, 1, 68, 946, 221, 1099,   // 0: child, kind 2 (button), attributes 0x1, id 0x1e0e9 (globe button, help 0x15a), rect (68, 946, 221, 1099)
        17, 346, 0,   // 17: help text number 346 (0x15a)
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 9752, 25777,   // 1: draw object 0x64b12618
        5,   // 5: end of this level
      1, 29926, -24074,   // 1: draw object 0xa1f674e6
      0, 1, 3, 0, -7958, 1, 554, 170, 990, 437,   // 0: child, kind 1 (window), attributes 0x3, id 0x1e0ea (key cost panel), rect (554, 170, 990, 437)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 3, 0, -7957, 1, 1045, 191, 1405, 416,   // 0: child, kind 1 (window), attributes 0x3, id 0x1e0eb (key cost digit), rect (1045, 191, 1405, 416)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, -23003, 8622,   // 1: draw object 0x21aea625
          5,   // 5: end of this level
        1, 22293, 12119,   // 1: draw object 0x2f575715
        5,   // 5: end of this level
      0, 1, 3, 0, -7956, 1, 1688, 68, 1963, 186,   // 0: child, kind 1 (window), attributes 0x3, id 0x1e0ec (key count panel), rect (1688, 68, 1963, 186)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 6, 1, 0, -7955, 1, 1853, 76, 1955, 178,   // 0: child, kind 6 (image), attributes 0x1, id 0x1e0ed (key icon), rect (1853, 76, 1955, 178)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, -13548, 7504,   // 1: draw object 0x1d50cb14
          5,   // 5: end of this level
        0, 1, 1, 0, -7954, 1, 1688, 82, 1829, 172,   // 0: child, kind 1 (window), attributes 0x1, id 0x1e0ee (key count text), rect (1688, 82, 1829, 172)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          5,   // 5: end of this level
        5,   // 5: end of this level
      0, 1, 3, 0, -7953, 1, 477, 1303, 1571, 1437,   // 0: child, kind 1 (window), attributes 0x3, id 0x1e0ef (park name), rect (477, 1303, 1571, 1437)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        5,   // 5: end of this level
      4, 4, 23,   // 4: region, a polygon of 23 points
          29, 1003, 29, 1364, 45, 1432, 87, 1486, 153, 1513, 254, 1521, 353, 1513, 419, 1485,
          461, 1433, 476, 1376, 459, 1316, 417, 1268, 356, 1242, 265, 1236, 263, 961, 245, 906,
          203, 869, 157, 855, 110, 860, 87, 867, 66, 882, 46, 907, 33, 944,
      0, 2, 1, 0, -7952, 1, 97, 1299, 250, 1453,   // 0: child, kind 2 (button), attributes 0x1, id 0x1e0f0 (previous island button, help 0x157), rect (97, 1299, 250, 1453)
        17, 343, 0,   // 17: help text number 343 (0x157)
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -22303, 31069,   // 1: draw object 0x795da8e1
        5,   // 5: end of this level
      0, 2, 1, 0, -7951, 1, 260, 1299, 413, 1453,   // 0: child, kind 2 (button), attributes 0x1, id 0x1e0f1 (next island button, help 0x158), rect (260, 1299, 413, 1453)
        17, 344, 0,   // 17: help text number 344 (0x158)
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 21546, -3632,   // 1: draw object 0xf1d0542a
        5,   // 5: end of this level
      3, 67, 1273, 438, 1484,   // 3: client rectangle (67, 1273, 438, 1484)
      2, -29701, -13736,   // 2: client draw object 0xca588bfb
      5,   // 5: end of this level
    5,   // 5: end of this level
};
static_assert(sizeof(gIslandLobbyWindowData) == 269 * sizeof(int16_t), "gIslandLobbyWindowData has 269 words");

namespace {
// The controller's windows (names invented). The key cost panel shows how many keys the current island costs (its picture 0 or
// 1 says whether the player has enough, the digit window below it shows the number); the key count panel shows the keys the
// player holds ("3 x"); the park name is the island's title; the buttons go to the previous and the next island.
constexpr int32_t kKeyCostPanel = 0x1e0ea;
constexpr int32_t kKeyCostDigit = 0x1e0eb;
constexpr int32_t kKeyCountPanel = 0x1e0ec;
constexpr int32_t kKeyCountIcon = 0x1e0ed;
constexpr int32_t kKeyCountText = 0x1e0ee;
constexpr int32_t kParkNameText = 0x1e0ef;
constexpr int32_t kButtonA = 0x1e0f0;
constexpr int32_t kButtonB = 0x1e0f1;
constexpr int32_t kMailIcon = 0xbf432;   // the lobby window's mail icon

// Wide texts of the string area at 0x1024ee1e.
const char16_t kParkNameFirstText[] = u"Park Name !";   // +0x00
const char16_t kKeyCountFormat[] = u"%d x";               // +0x18

// The particle that sparkles over the key panel: library emitter 0x61 at x 50000.
constexpr int32_t kKeyEmitterType = 0x61;

// The master save and the game type object are made on first use (the game type's constructor reads the game options;
// TheGameType is that inline accessor).
CFSGlobalMasterSave* TheMasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

// The emitter's height: the middle of the key panel's rectangle (taken without the window's animation offsets) in the emitter's
// units, 75000 / 1536 per pixel. The product wraps at 32 bits and the quotient truncates toward zero.
int32_t KeyEmitterHeight(const CoolRect& rect) {
    const int32_t middle = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
    return static_cast<int32_t>(static_cast<uint32_t>(middle) * 75000u) / 0x600;
}

// Shows the key cost panel's picture for an island the player can afford (picture 0, the digit window showing the cost less
// one, and the emitter started if it is not running) or cannot (picture 1, the digit showing the cost plus four, and the
// emitter killed). Both screens (the full update and the key display) do the same with the panel.
void ShowKeyCostPanel(InterfaceWindow* panel, CFSGlobalParkData* park, bool affordable) {
    if (affordable) {
        panel->SetFrame(0);
        const int32_t cost = park->KeyCostToEnter();
        InterfaceWindow* digit = panel->GetWindow(kKeyCostDigit);
        digit->SetFrame(cost - 1);
        CoolRect rect;
        panel->GetPosition(&rect, 1);
        const int32_t height = KeyEmitterHeight(rect);
        if (gIslandLobbyKeyEmitter == 0) gIslandLobbyKeyEmitter = static_cast<int32_t>(PAR_InitEmitter(kKeyEmitterType, 50000, 0, height));
    } else {
        panel->SetFrame(1);
        const int32_t cost = park->KeyCostToEnter();
        InterfaceWindow* digit = panel->GetWindow(kKeyCostDigit);
        digit->SetFrame(cost + 4);
        if (gIslandLobbyKeyEmitter != 0) {
            PAR_KillEmitter(gIslandLobbyKeyEmitter);
            gIslandLobbyKeyEmitter = 0;
        }
    }
}
}  // namespace

// Builds the controller once, if the lobby has a window: the windows, the key count's text object (a right-aligned
// font 2 text, hidden at first), the pictures of the key icon, the cost panel and its digit, the key display, and the park name's
// title text object (font 0, centred, gold, with its first text).
void CreateIslandLobbyController() {
    if (gIslandLobbyController != nullptr) return;
    if (gLobby->mWindow == nullptr) return;
    gIslandLobbyController = gLobby->mWindow->Build(gIslandLobbyWindowData, IslandLobbyControllerMsg);
    InterfaceWindow* keyCount = gIslandLobbyController->GetWindow(kKeyCountPanel);
    InterfaceWindow* keyCountText = keyCount->GetWindow(kKeyCountText);
    keyCountText->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    keyCountText->SetFont(GetFontTableEntry(2));
    keyCountText->SetClientDrawText(nullptr);
    keyCountText->SetFontJust(2, 1);
    keyCount->GetWindow(kKeyCountIcon)->SetFrame(3);
    keyCount->Show(0);
    gIslandLobbyController->GetWindow(kKeyCostPanel)->SetFrame(1);
    gIslandLobbyController->GetWindow(kKeyCostPanel)->GetWindow(kKeyCostDigit)->SetFrame(5);
    gIslandLobbyKeysShown = 0;
    gIslandLobbyKeyEmitter = 0;
    UpdateIslandLobbyKeyDisplay();
    InterfaceWindow* parkName = gIslandLobbyController->GetWindow(kParkNameText);
    parkName->SetClientDrawObject(new InterfaceTitleTextObject(nullptr));
    parkName->SetFont(GetFontTableEntry(0));
    parkName->SetFontJust(1, 1);
    parkName->SetText(const_cast<char16_t*>(kParkNameFirstText));
    parkName->SetFontColour(0xfc, 0xe8, 7, 0xff);
    UpdateOutboxMailIcon();
}

// Message 6 (show) with 1, then the mail icon.
void ShowIslandLobbyController() {
    UI_SendMessage(gIslandLobbyController, 6, 1, 0);
    UpdateOutboxMailIcon();
}

// The key cost panel and the park name for the island the camera is on: the cost against the keys the display last saw (not
// the keys now: the key display updates that word) and the island's name, which is the park's full name when the master save
// has the park and the island's own title otherwise. In the easy game type there is no key cost.
void UpdateIslandLobbyUI() {
    if (gIslandLobbyController == nullptr) return;
    CLBIsland* island = gLobbyScreens->mThemeIslands->mCurrent;
    const char16_t* title = island->mTitle.mData;
    CFSGlobalParkData* park;
    {
        const TbDynamicStringTemplate<char> name(island->mName.mData);
        park = TheMasterSave()->GetInfoOn(name);
    }
    InterfaceWindow* keyCost = gIslandLobbyController->GetWindow(kKeyCostPanel);
    if (park != nullptr) {
        if (TheGameType().mType == kGameTypeEasy) {
            keyCost->Show(0);
        } else {
            const int32_t cost = park->KeyCostToEnter();
            ShowKeyCostPanel(keyCost, park, !(gIslandLobbyKeysShown < cost));
        }
        park->GetFullName(gUIMainConvertBuffer, 0x3fff);
        title = gUIMainConvertBuffer;
    }
    gIslandLobbyController->GetWindow(kParkNameText)->SetText(const_cast<char16_t*>(title));
}

// What the key display shows (called when the keys change, and by the controller's creation): for the island the camera is
// on, the key cost panel as above, comparing the cost with the keys the player holds now; then, for the whole screen, the key
// count ("N x") when the player holds any keys, hidden when none; and the number of keys is remembered. In the easy game
// type both panels are hidden and the buttons disabled.
void UpdateIslandLobbyKeyDisplay() {
    if (gIslandLobbyController == nullptr) return;
    if (gLobbyScreens != nullptr && gLobbyScreens->mThemeIslands != nullptr && gLobbyScreens->mThemeIslands->mCurrent != nullptr) {
        CFSGlobalParkData* park;
        {
            const TbDynamicStringTemplate<char> name(gLobbyScreens->mThemeIslands->mCurrent->mName.mData);
            park = TheMasterSave()->GetInfoOn(name);
        }
        InterfaceWindow* keyCost = gIslandLobbyController->GetWindow(kKeyCostPanel);
        if (park != nullptr) {
            if (TheGameType().mType == kGameTypeEasy) {
                keyCost->Show(0);
                EnableIslandLobbyButtons(false);
            } else {
                keyCost->Show(1);
                EnableIslandLobbyButtons(true);
                CFSGlobalMasterSave* save = TheMasterSave();
                const int32_t cost = park->KeyCostToEnter();
                const int32_t keys = save->HowManyKeys();
                ShowKeyCostPanel(keyCost, park, !(keys < cost));
            }
        }
    }
    InterfaceWindow* keyCount = gIslandLobbyController->GetWindow(kKeyCountPanel);
    if (TheGameType().mType == kGameTypeEasy) {
        keyCount->Show(0);
        EnableIslandLobbyButtons(false);
    } else if (TheMasterSave()->HowManyKeys() > 0) {
        keyCount->Show(1);
        EnableIslandLobbyButtons(true);
        swprintf(gUIMainConvertBuffer, 0x3fff, kKeyCountFormat, TheMasterSave()->HowManyKeys());
        keyCount->GetWindow(kKeyCountText)->SetText(gUIMainConvertBuffer);
    } else {
        keyCount->Show(0);
    }
    gIslandLobbyKeysShown = TheMasterSave()->HowManyKeys();
}

void EnableIslandLobbyButtons(bool on) {
    if (gIslandLobbyController == nullptr) return;
    gIslandLobbyController->GetWindow(kButtonA)->Enable(on);
    gIslandLobbyController->GetWindow(kButtonB)->Enable(on);
    gIslandLobbyController->GetWindow(kKeyCostPanel)->Show(on);
    gIslandLobbyController->GetWindow(kKeyCountPanel)->Show(on);
}

// The lobby window's mail icon is shown while the player's outbox holds mail (a word of the outbox: the same test as the lobby's
// start). The user info is made on first use.
void UpdateOutboxMailIcon() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    if (gUserInfo->mOutbox.mF28.empty()) gLobby->mWindow->GetWindow(kMailIcon)->Show(0);
    else gLobby->mWindow->GetWindow(kMailIcon)->Show(1);
}
