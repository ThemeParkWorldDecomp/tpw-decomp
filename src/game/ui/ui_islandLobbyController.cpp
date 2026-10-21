// ui_islandLobbyController.cpp: name proven by __sinit_ui_islandLobbyController_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "lobby.h"
#include "MapWho.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "rsse.h"
#include "std_basic_ifstream.h"
#include "TbDynamicStringTemplate.h"
#include "TPCamera.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "ui_textmanager.h"
#include "UIInterface.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// ui_islandLobbyController.cpp of the main app (SimTheme Park 2.04b3), second part, unit w36: the island lobby controller's message
// function, the two messages the island screen sends it (Remove, Hide) and the key emitter's start and stop. The controller itself,
// its key display and its description (gIslandLobbyWindowData) are src/UI_IslandLobby.cpp's (unit w32).
//
// The controller is the panel InterfaceWindow::Build makes from that description under the lobby's window, with
// IslandLobbyControllerMsg as its message function. The pump (src/UI_pump.cpp) sends it the messages: 0x14 when the panel goes (the
// global is cleared), 0x11 when it is shown or hidden (the key display, or the emitter killed), and 0x100 when a button of it was
// pressed (the button's window id is the first argument). The two buttons of the panel that are not the arrows go to the screens:
// the enter button selects the island the camera is on, the globe button hides the island screen and shows the download globe, the
// arrows move to the previous or the next island.

namespace {
// The window ids of the controller (src/UI_IslandLobby.cpp has the others; names invented).
constexpr int32_t kEnterButton = 0x1e0e8;
constexpr int32_t kGlobeButton = 0x1e0e9;
constexpr int32_t kPreviousButton = 0x1e0f0;
constexpr int32_t kNextButton = 0x1e0f1;

// The messages the controller answers (the pump's numbers; names invented).
constexpr int32_t kMessageShown = 0x11;
constexpr int32_t kMessageButton = 0x100;
constexpr int32_t kMessageDeleted = 0x14;

// The key emitter: library emitter 0x61 at x 50000 (the same numbers as src/UI_IslandLobby.cpp's, which inlines this function).
constexpr int32_t kKeyEmitterType = 0x61;
}  // namespace

// 0x101e0d98 (396 bytes). Everything but the globe button goes on to the window's default handler, whose answer is the answer; the
// globe button answers 0 itself.
int32_t IslandLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageDeleted) {
        gIslandLobbyController = nullptr;
        UpdateKeyParticles(nullptr, false);
    } else if (message == kMessageShown) {
        if (a == 0) UpdateKeyParticles(nullptr, false);
        else UpdateIslandLobbyKeyDisplay();
    } else if (message == kMessageButton) {
        if (a == kPreviousButton) {
            gLobbyScreens->mThemeIslands->GotoPreviousIsland();
        } else if (a == kGlobeButton) {
            gLobbyScreens->mThemeIslands->Hide();
            gLobbyScreens->mDownloadGlobe->Show();
            return 0;
        } else if (a == kEnterButton) {
            gLobbyScreens->mThemeIslands->SelectCurrentIsland();
        } else if (a == kNextButton) {
            gLobbyScreens->mThemeIslands->GotoNextIsland();
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// ui_islandLobbyController.cpp of the main app (SimTheme Park 2.04b3), unit w32: the island lobby's controller and the golden
// key display, plus the mail icon (UpdateOutboxMailIcon, which the inventory files under ui_onlineOutbox.cpp). The controller is a
// small window tree built under the lobby's window; it shows the golden keys the player holds, the keys the current island
// costs, the island's name and two buttons. The island screen creates, shows and updates it (src/lobby_islands.cpp).

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

// 0x101e11f8 (56 bytes) and 0x101e12d4 (56 bytes): messages 4 and 6 with no arguments to the controller (a NULL window is ignored by
// UI_SendMessage).
void RemoveIslandLobbyController() { UI_SendMessage(gIslandLobbyController, 4, 0, 0); }

// Message 6 (show) with 1, then the mail icon.
void ShowIslandLobbyController() {
    UI_SendMessage(gIslandLobbyController, 6, 1, 0);
    UpdateOutboxMailIcon();
}

void HideIslandLobbyController() { UI_SendMessage(gIslandLobbyController, 6, 0, 0); }

// 0x101e133c (232 bytes): with `on`, the emitter over the window's middle is started unless it is running (the window's rectangle
// less its animation offsets: the middle of top and bottom, times 75000 / 1536 in 32 bits, the product wrapping and the quotient
// truncating toward zero; the window is asked first even when the emitter runs); without, a running emitter is killed and its handle
// cleared. NULL `window` is only for off.
void UpdateKeyParticles(InterfaceWindow* window, bool on) {
    if (on) {
        CoolRect rect;
        window->GetPosition(&rect, 1);
        const int32_t middle = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
        const int32_t height = static_cast<int32_t>(static_cast<uint32_t>(middle) * 75000u) / 0x600;
        if (gIslandLobbyKeyEmitter == 0) gIslandLobbyKeyEmitter = static_cast<int32_t>(PAR_InitEmitter(kKeyEmitterType, 50000, 0, height));
    } else if (gIslandLobbyKeyEmitter != 0) {
        PAR_KillEmitter(gIslandLobbyKeyEmitter);
        gIslandLobbyKeyEmitter = 0;
    }
}

// The way out of the lobby (main app): the key sparkle when the player enters an island (ui_islandLobbyController.cpp
// 0x101e1460), the interface's lobby tear-down (ui_main.cpp 0x10186434, ui_ErrorDisplay.cpp 0x101ef7a4) and the first-person
// ride test (TPCamera.cpp 0x100329cc).

// A burst of sparkles over the middle of the controller's key panel (window 0x1e0ea) and the thunder bank's event 4, unless the
// island screen is not up or the game is the easy one. The emitter's x is taken from the window's TOP and BOTTOM edges (the
// listing reads the rectangle's words at +6 and +0xa, not +4 and +8): (top + (bottom - top) / 2) * 75000 / 0x600, the product
// wrapping in 32 bits and the division truncating. The window is not tested for NULL (GetPosition then runs on NULL). The rectangle
// is made only on the path that uses it.
void KeyParticlePOUFFE() {
    if (gIslandLobbyController == nullptr) return;
    if (TheGameType().mType == kGameTypeEasy) return;
    CoolRect rect;
    InterfaceWindow* window = gIslandLobbyController->GetWindow(0x1e0ea);
    window->GetPosition(&rect, 1);
    const int32_t middle = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
    const int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(middle) * 75000u);
    PAR_InitEmitter(0x62, 50000, 0, scaled / 0x600);
    SOUND_SendEvent(0, gLobbyThunderBank, 4, 0, 0, 0);
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

void UpdateOutboxMailIcon();

// Unit v5981: InterfaceWindow::link (0x101ddf20), the island lobby controller's static initialiser (0x101e1e3c), the online login panel's
// message function (0x101e26fc), the outbox address panel's message function (0x101e4b00) and the JPEG loader (0x101e538c).

int32_t InterfaceWindow::linkChecked(InterfaceWindow* child, char a, char b) {
    if (child == nullptr || child->mParent != nullptr || child->mNext != nullptr || child->mPrevious != nullptr) return 0;
    child->mParent = this;
    InterfaceWindow* last = nullptr;
    if (mChild == nullptr) {
        mChild = child;
    } else {
        InterfaceWindow* walk = mChild;
        do {
            last = walk;
            walk = walk->mNext;
        } while (walk != nullptr);
        last->mNext = child;
    }
    child->mPrevious = last;
    if (a != 0) child->setZ(static_cast<int16_t>(mZ + 2));
    if (b != 0) child->setPivot(mPivotX, mPivotY);
    UI_UpdatePointerIn();
    if (gUIKeyboardWindow == this) {
        UI_PostMessage(gUIKeyboardWindow, 0x20, 0, 0);
        gUIKeyboardWindow = nullptr;
        UI_UpdatePointerIn();
    }
    return 1;
}

// The two wide texts of the string area at 0x1024ee1e (+0x22 and +0x30).
static const char16_t kIslandLobbyText1[] = u"upload";
static const char16_t kIslandLobbyText2[] = u"gms.dat";

alignas(16) uint8_t gIslandLobbyText1Storage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395ce0 (0x14 bytes on the Mac)
uint8_t gIslandLobbyText1Node[24];                                                         // 0x10395cf4
alignas(16) uint8_t gIslandLobbyText2Storage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395cc0
uint8_t gIslandLobbyText2Node[24];                                                         // 0x10395cd4

// 0x101e1e3c: makes the two global wide strings and registers their destructors.
void __sinit_ui_islandLobbyController_cpp() {
    TbDynamicStringTemplate<char16_t>* first = new (gIslandLobbyText1Storage) TbDynamicStringTemplate<char16_t>(kIslandLobbyText1);
    __register_global_object(first, reinterpret_cast<void*>(&UIMain_DestroyWString), gIslandLobbyText1Node);
    TbDynamicStringTemplate<char16_t>* second = new (gIslandLobbyText2Storage) TbDynamicStringTemplate<char16_t>(kIslandLobbyText2);
    __register_global_object(second, reinterpret_cast<void*>(&UIMain_DestroyWString), gIslandLobbyText2Node);
}
