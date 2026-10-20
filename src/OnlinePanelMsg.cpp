// OnlinePanelMsg 0x101e62d0 (3,552 bytes, Mac file ui_onlineParkFindPanel.cpp): the message function of the online park panel (gOnlinePanel) and of
// its full screen window (gOnlineWindow).
//   0x14 (destroy)     the panel forgets itself, the help text goes and the three other windows are told 4; the window forgets itself
//   0x11 (shown)       a == 1: the panel being shown sets the "panel shown" byte; a != 1: the panel being hidden clears it
//   0x10005 / 0x10004  mouse down / up of the pointer capture (a == 1 captures / releases the pointer)
//   then Will_HandleCameraMessages, then by message: key and mouse messages go to the camera key table, the shortcuts table, the mouse; the tick
//   (0x1e) works out what is under the pointer (ride of the "upgrades what" kind) and sets the help text and the cursor.
// The window's default handler answers last.
#include <cstdint>

#include "InterfaceWindow.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_Postcard.h"
#include "ui_main.h"
#include "ui_tooltip.h"
#include "LLCellSystem.h"
#include "Debug.h"

// The globals nobody else names (Mac addresses in ui_main.h).
uint16_t gOnlineNoCellId;          // 0x102b0832
uint16_t gOnlineRideUpgradeId;     // 0x102b0c88
uint16_t gOnlineNoRideId;          // 0x102b0c8a
CObject* gOnlineTargetRide;        // 0x10395e60
int32_t gOnlineCursorState;        // 0x10395e64
uint8_t gOnlineWindowKeyHeld;      // 0x10395e68
uint8_t gOnlinePanelKeyHeld;       // 0x10395e69
uint8_t gOnlineBlockToggle;        // 0x10395e79
uint8_t gOnlinePanelShown;         // 0x10395e7a
InterfaceWindow* gOnlineWindowE8c; // 0x10395e8c
InterfaceWindow* gOnlineWindowE90; // 0x10395e90
char16_t gOnlineEmptyHelp[2] = {0, 0x2173};   // 0x1024ef54

extern int16_t gUIRealScreenWidth;
extern int16_t gUIRealScreenHeight;

namespace {

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024eede

// The Mac builds the mouse (and its default mode) inline wherever it needs one.
CMouse* MouseOrNew() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    return gMouse;
}

int32_t CurrentMouseMode() {
    return gMouseMode == nullptr ? 0 : gMouseMode->GetMode();
}

InterfaceWindow* KeyboardChild(InterfaceWindow* window) {
    InterfaceWindow* child = window->GetWindow(0xc3123);
    child = child->GetWindow(0xc312b);
    return child->GetWindow(0xc312c);
}

// The pointer position the panel hands the mouse: the halfword of b scaled to the real screen (x by 2048, y by 1536).
int32_t PointerX(int32_t b) {
    return (b >> 16) * static_cast<int32_t>(gUIRealScreenWidth) / 0x800;
}

int32_t PointerY(int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(b) & 0xffff) * static_cast<int32_t>(gUIRealScreenHeight) / 0x600;
}

}  // namespace

int32_t OnlinePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) {
        if (gOnlinePanel == window) {
            gOnlinePanel = nullptr;
            MoveHelpText(-1, -1);
            UI_SendMessage(gOnlineWindow, 4, 0, 0);
            UI_SendMessage(gOnlineWindowE8c, 4, 0, 0);
            UI_SendMessage(gOnlineWindowE90, 4, 0, 0);
        }
        if (gOnlineWindow == window) gOnlineWindow = nullptr;
    } else if (message < 0x14) {
        if (message == 0x11) {
            if (a == 1) {
                CoolRect rect;
                window->GetPosition(&rect, 1);
                if (window == gOnlinePanel) gOnlinePanelShown = 1;
            } else if (window == gOnlinePanel) {
                gOnlinePanelShown = 0;
            }
        }
    } else if (message == 0x10005) {
        if (a == 1) window->CaptureDevice();
    } else if (message < 0x10005 && message >= 0x10004) {
        if (a == 1) window->UnCaptureDevice();
    }

    Will_HandleCameraMessages(message, a, b);

    if (message == 0x10005) {
        bool passOn = true;
        int16_t shifts = CheckShifts();
        gKeyTableCamera->KeyDown(static_cast<int16_t>(a - 0x10), shifts);
        bool checkPanel = true;
        if (a == 0) {
            MouseOrNew();
            if (CurrentMouseMode() != 9 && gOnlineCursorState == 2) {
                checkPanel = false;
                CObject* ride = gOnlineTargetRide;
                if (ride != nullptr && reinterpret_cast<CThing*>(ride)->mKind == 3) {
                    ride->GoFirstPerson(0);
                    passOn = false;
                }
            }
        }
        if (checkPanel && a == 0 && gOnlinePanel == window) {
            // CaptureKb answers 1 when it took the keyboard from another window (the declaration is void; the answer is that state).
            bool took = gUIKeyboardWindow != window;
            window->CaptureKb(0);
            if (took) passOn = false;
        }
        if (passOn) {
            CMouse::ProcessIngameMouse(a == 0 ? 0xf0 : 0xf1, 1, PointerX(b), PointerY(b), nullptr);
        }
    } else if (message < 0x10005) {
        if (message == 0x10001) {
            gOnlineCursorState = 0;
            if (window == gOnlineWindow) gOnlineWindowKeyHeld = 1;
            else gOnlinePanelKeyHeld = 1;
        } else if (message > 0x10000) {
            if (message != 0x10003) {
                if (message < 0x10003) {
                    if (window == gOnlineWindow) gOnlineWindowKeyHeld = 0;
                    else gOnlinePanelKeyHeld = 0;
                } else {
                    int16_t shifts = CheckShifts();
                    gKeyTableCamera->KeyUp(static_cast<int16_t>(a - 0x10), shifts);
                }
            }
        } else if (message == 0x1e) {
            if (window == gOnlineWindow) UpdateAvatarPos();
            if ((window->GetAttr() & 1) != 0) {
                int32_t oldState = gOnlineCursorState;
                bool highlight = false;
                gOnlineCursorState = 0;
                if (gOnlinePanelKeyHeld != 0 || gOnlineWindowKeyHeld != 0) {
                    uint16_t cell = gIntrface.mF24;
                    if (cell != gOnlineNoCellId) {
                        uint16_t found;
                        LLCR_OverRide(&found, static_cast<int32_t>((cell - 1) & 0x7f), static_cast<int32_t>(static_cast<uint32_t>(cell - 1) >> 7));
                        if (found != gOnlineNoRideId) {
                            bool isRide = false;
                            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                            CObject* ride = static_cast<CObject*>(gThingArray[found].mThing);
                            if (reinterpret_cast<CThing*>(ride)->mKind == 3 && ride->UpgradesWhat().mIndex == gOnlineRideUpgradeId) {
                                bool fits = true;
                                if (ride->GetDBInfo()->mUIType != 0) {
                                    bool secure = false;
                                    if (ride->GetDBInfo()->mUIType == 3 && (ride->mFlags & 0x10) != 0) secure = true;
                                    if (!secure) fits = false;
                                }
                                if (fits) isRide = true;
                            }
                            if (isRide) {
                                gOnlineCursorState = 2;
                                highlight = true;
                                gOnlineTargetRide = ride;
                            }
                        }
                    }
                    MouseOrNew();
                    if (CurrentMouseMode() == 9) {
                        if (gOnlineCursorState == 2) gOnlineCursorState = 4;
                        else gOnlineCursorState = 3;
                    } else if (gOnlineCursorState == 0) {
                        gOnlineCursorState = 1;
                    }
                }
                int32_t state = gOnlineCursorState;
                if (state != oldState) {
                    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[7]);   // gText + 0x568
                    if (state == 2 || state == 4) {
                        TbUniStringVTag tag(2, UIMain_GetObjectName(gOnlineTargetRide));
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x244, &tag);
                        SetHelpTextString(gUIMainConvertBuffer);
                        window->setCursor(0x13);
                    } else if (state == 0) {
                        SetHelpTextString(gOnlineEmptyHelp);
                        window->setCursor(0);
                    } else if (state == 1) {
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x191);
                        SetHelpTextString(gUIMainConvertBuffer);
                        window->setCursor(0);
                    } else if (state == 3) {
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x243);
                        SetHelpTextString(gUIMainConvertBuffer);
                        window->setCursor(0x13);
                    }
                }
                if (highlight) HighlightRide();
                else UnhighlightRide();
            }
        }
    } else if (message == 0x1000a) {
        gKeyTableCamera->KeyDown(static_cast<int16_t>(a), static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff));
    } else if (message < 0x1000a) {
        if (message == 0x10007 && a == 0) {
            MouseOrNew();
            gMouse->DoubleClick(PointerX(b), PointerY(b));
        }
    } else if (message < 0x1000c) {
        int16_t key = static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff);
        if (a == 0x2f || a == 0xd || a == 9) {
            if (gOnlinePanelShown != 0 && key == 0) {
                window->UnCaptureKb(0);
                KeyboardChild(window)->CaptureKb(0);
                if (a == 0x2f) UI_PostMessage(KeyboardChild(window), 0x1000c, 0x2f, 0);
            } else {
                gKeyTableCamera->KeyUp(static_cast<int16_t>(a), key);
            }
        } else if (a == 0x73) {
            if (key == 0 && gOnlineBlockToggle == 0) {
                uint8_t shown = gOnlinePanelShown;
                UI_SendMessage(gOnlinePanel, 6, shown == 0, 0);
                UI_SendMessage(gOnlineWindow, 6, shown, 0);
                if (gOnlinePanelShown != 0) {
                    KeyboardChild(gOnlinePanel)->CaptureKb(0);
                    MouseOrNew();
                    if (CurrentMouseMode() == 9) gOnlinePanel->setCursor(0x13);
                } else {
                    gOnlineWindow->CaptureKb(0);
                }
            }
        } else {
            int16_t found = static_cast<int16_t>(gKeyTableShortcuts->Search(static_cast<int16_t>(a), key));
            if (found == 0xf) {
                UI_DoPostcard();
            } else if (found == 0) {
                MouseOrNew();
                bool handled;
                if (CurrentMouseMode() == 9) {
                    CMMOnline* mode = new CMMOnline;
                    MouseOrNew();
                    gMouse->AssignMouseMode(mode);
                    gOnlineWindow->setCursor(0);
                    handled = true;
                } else {
                    handled = false;
                }
                if (!handled) CreateMainMenu(0);
            } else if (found == 0x10) {
                CMMFirstPersonTarget* mode = new CMMFirstPersonTarget;
                MouseOrNew();
                gMouse->AssignMouseMode(mode);
                if (gOnlinePanelShown == 0) gOnlineWindow->setCursor(0x13);
                else gOnlinePanel->setCursor(0x13);
            } else {
                gKeyTableCamera->KeyUp(static_cast<int16_t>(a), key);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
