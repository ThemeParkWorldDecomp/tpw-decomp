// ui_onlinePanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keyboard.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "online.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbPixFmt.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "ui_tooltip.h"
#include "UIInterface.h"
#include "World.h"
#include "WindowScripts.inc"
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>

// ui_onlineOutbox.cpp / ui_onlineParkFindPanel.cpp leftovers (unit v5579): the chat panel's postcard hand-over, the outbox
// file's static initialiser and the JPEG error handler.

// Globals no other file names (original names unknown).
InterfaceWindow* gOnlineWord80 = nullptr;       // 0x10395e80: a window the chat saved before a postcard (not 0 wins in ChatPrePostCard)
InterfaceWindow* gOnlineSavedWindow84 = nullptr; // 0x10395e84: the window ChatPrePostCard chose, put back by ChatReinstateAfterPostcard
// 0x101e568c
void ChatPrePostCard() {
    InterfaceWindow* saved = gOnlineWord80;
    if (saved != nullptr) {
        gOnlineSavedWindow84 = saved;
        return;
    }
    if (gOnlinePanelShown != 0) {
        gOnlineSavedWindow84 = gOnlinePanel;
        return;
    }
    gOnlineSavedWindow84 = gOnlineWindow;
}

// 0x101e5700
void ChatReinstateAfterPostcard() {
    InterfaceWindow* panel = gOnlinePanel;
    bool same = (panel == gOnlineSavedWindow84);
    UI_SendMessage(panel, 6, same ? 1 : 0, 0);
    UI_SendMessage(gOnlineWindow, 6, same ? 0 : 1, 0);
    gOnlineSavedWindow84 = nullptr;
}

// 0x10395e6c .. 0x10395e77: the chat panel's first-person bookkeeping (declared in online.h; zero at start like the Mac's bss).
int32_t gOnlineFirstPersonCountdown = 0;   // 0x10395e6c
uint32_t gOnlineSavedFlags = 0;            // 0x10395e70
uint8_t gOnlineMapZ[2] = {0, 0};           // 0x10395e74
uint8_t gOnlineMapX[2] = {0, 0};           // 0x10395e76

namespace {

// The control is made on first use: operator new(0xe8c), then the constructor.
COnlineControl* OnlineControlOrNew() {
    if (gOnlineControl == nullptr) {
        COnlineControl* made = static_cast<COnlineControl*>(online_callee::OperatorNew(0xe8c));
        if (made != nullptr) new (made) COnlineControl();
        gOnlineControl = made;
    }
    return gOnlineControl;
}

// A position as a map cell and a 1/256 offset: the cell is the truncated tenth, the offset the truncated 256 times the rest.
void ToMapPos(float position, uint8_t out[2]) {
    float tenth = position / 10.0f;
    out[0] = static_cast<uint8_t>(ppc_fctiwz(tenth));
    float rest = tenth - static_cast<float>(out[0]);
    out[1] = static_cast<uint8_t>(ppc_fctiwz(256.0f * rest));
}

float FromMapPos(const uint8_t in[2]) {
    float cell = static_cast<float>(in[0]);
    float offset = static_cast<float>(in[1]);
    return 10.0f * ppc_fmadds(offset, 0.00390625f, cell);
}

MapPos MakeMapPos(const uint8_t in[2]) {
    MapPos p;
    p.mCell = in[0];
    p.mOffset = in[1];
    return p;
}

}  // namespace

// 0x101e57bc
void ChatGoFullScreen() {
    uint8_t shown = gOnlinePanelShown;
    if (shown == 0 || gOnlineBlockToggle != 0) return;
    UI_SendMessage(gOnlinePanel, 6, shown == 0 ? 1 : 0, 0);
    UI_SendMessage(gOnlineWindow, 6, shown, 0);
    if (gOnlinePanelShown == 0) {
        gOnlineWindow->CaptureKb(0);
        return;
    }
    InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
    w = w->GetWindow(0xc312b);
    w = w->GetWindow(0xc312c);
    w->CaptureKb(0);
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(online_callee::OperatorNew(1));
        if (mouse != nullptr) {
            CMMDefault* mode = static_cast<CMMDefault*>(online_callee::OperatorNew(8));
            if (mode != nullptr) new (mode) CMMDefault();
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    int32_t current = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
    if (current == 9) gOnlinePanel->setCursor(0x13);
}
// 0x101e5968
void ChatGoFirstPerson() {
    if (gOnlinePanelShown != 0) gOnlineWord80 = gOnlinePanel;
    else gOnlineWord80 = gOnlineWindow;
    CoolRect rect;
    gOnlineWindow->GetPosition(&rect, 1);
    UI_SendMessage(gOnlinePanel, 6, 0, 0);
    UI_SendMessage(gOnlineWindow, 6, 0, 0);
    ToMapPos(gPointerWorldPosn.x, gOnlineMapX);
    ToMapPos(gPointerWorldPosn.z, gOnlineMapZ);
    OnlineControlOrNew()->EnteringFirstPerson(MakeMapPos(gOnlineMapX), MakeMapPos(gOnlineMapZ));
    if ((gFirstPersonFlags & 4) != 0) OnlineControlOrNew()->HideAvatar();
    gOnlineByte78 = 1;
    gOnlineFirstPersonCountdown = 5;
    gOnlineSavedFlags = gFirstPersonFlags;
}

// 0x101e5ba0
void ChatBackToThirdPerson() {
    if ((gFirstPersonFlags & 0x400) != 0) return;
    bool same = (gOnlinePanel == gOnlineWord80);
    UI_SendMessage(gOnlinePanel, 6, same ? 1 : 0, 0);
    UI_SendMessage(gOnlineWindow, 6, same ? 0 : 1, 0);
    gOnlineWord80 = nullptr;
    if (gOnlinePanel != nullptr) gOnlinePanel->setCursor(0);
    if (gOnlineWindow != nullptr) gOnlineWindow->setCursor(0);
    ToMapPos(gCameraViewPoint.x, gOnlineMapX);
    ToMapPos(gCameraViewPoint.z, gOnlineMapZ);
    OnlineControlOrNew()->LeavingFirstPerson(MakeMapPos(gOnlineMapX), MakeMapPos(gOnlineMapZ));
    gOnlineByte78 = 0;
    gCameraSavedPosition[0] = FromMapPos(gOnlineMapX);
    gCameraSavedPosition[2] = FromMapPos(gOnlineMapZ);
}

// 0x101e5e08
void UpdateAvatarPos() {
    bool unhide = false;
    uint32_t flags = gFirstPersonFlags;
    if ((flags & 0x400) != 0) {
        if ((gOnlineSavedFlags & 0x400) == 0) {
            gOnlineByte78 = 0;
            OnlineControlOrNew()->HideAvatar();
        }
    } else if ((flags & 0x16) != 0 && (gOnlineSavedFlags & 0x400) != 0) {
        gOnlineByte78 = 1;
        unhide = true;
        gOnlineFirstPersonCountdown = 0;
    }
    gOnlineSavedFlags = gFirstPersonFlags;
    if (gOnlineFirstPersonCountdown != 0) gOnlineFirstPersonCountdown = gOnlineFirstPersonCountdown - 1;
    if (gOnlineFirstPersonCountdown != 0 || gOnlineByte78 == 0) return;

    uint8_t x[2];
    uint8_t z[2];
    if (unhide) {
        ToMapPos(gCameraPosition[2], z);
        ToMapPos(gCameraPosition[0], x);
    } else {
        ToMapPos(gCameraViewPoint.z, z);
        ToMapPos(gCameraViewPoint.x, x);
    }
    if (x[0] == gOnlineMapX[0] && x[1] == gOnlineMapX[1] && z[0] == gOnlineMapZ[0] && z[1] == gOnlineMapZ[1] && !unhide) return;
    gOnlineMapX[0] = x[0];
    gOnlineMapX[1] = x[1];
    gOnlineMapZ[0] = z[0];
    gOnlineMapZ[1] = z[1];
    if (unhide) OnlineControlOrNew()->UnHideAvatar(MakeMapPos(x), MakeMapPos(z));
    else OnlineControlOrNew()->UpdateFirstPersonPos(MakeMapPos(x), MakeMapPos(z));
}

// OnlinePanelMsg_DisableVoteButton.cpp, main app: two helpers of the online panel (gOnlinePanel).

// 0x101e6224: with the online panel, takes its child 0xc3123, that one's 0xc3124, that one's 0xc3129 and calls the virtual
// Enable (slot 0x1c of the table at +0x118) on it with !flag. No NULL check on the children.
namespace online_callee {
void DisableVoteButton(bool flag) {
    if (gOnlinePanel) {
        InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
        w = w->GetWindow(0xc3124);
        w = w->GetWindow(0xc3129);
        w->Enable(!flag);
    }
}
}  // namespace online_callee

// 0x101e6168: the same path ending in child 0xc3128, an InterfaceButton: SetState(down == 1, 1).
void EarmuffsButtonDown(bool down) {
    if (gOnlinePanel) {
        int8_t state = (down == 1) ? 1 : 0;
        InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
        w = w->GetWindow(0xc3124);
        w = w->GetWindow(0xc3128);
        static_cast<InterfaceButton*>(w)->SetState(state, 1);
    }
}

// OnlinePanelMsg 0x101e62d0 (3,552 bytes, Mac file ui_onlineParkFindPanel.cpp): the message function of the online park panel (gOnlinePanel) and of
// its full screen window (gOnlineWindow).
//   0x14 (destroy)     the panel forgets itself, the help text goes and the three other windows are told 4; the window forgets itself
//   0x11 (shown)       a == 1: the panel being shown sets the "panel shown" byte; a != 1: the panel being hidden clears it
//   0x10005 / 0x10004  mouse down / up of the pointer capture (a == 1 captures / releases the pointer)
//   then Will_HandleCameraMessages, then by message: key and mouse messages go to the camera key table, the shortcuts table, the mouse; the tick
//   (0x1e) works out what is under the pointer (ride of the "upgrades what" kind) and sets the help text and the cursor.
// The window's default handler answers last.

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

// OnlineEditMsg 0x101e71a8 (1128 bytes) and OnlinePanelSubPanelMsg 0x101e70ec (120 bytes): the message functions of the online
// panel's edit box and of its sub panel.

// The calls through __ptr_glue and the allocator, weak so the test library can fake them.
namespace online_edit_callee {
__attribute__((weak)) void* New(size_t size) { return ::operator new(size); }
// the virtual at slot 0x2c of the table at window+0x118
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
// the virtual at slot 0x2c of the mouse mode's table
__attribute__((weak)) int32_t MouseModeOf(CMouseMode* mode) { return mode->GetMode(); }
}  // namespace online_edit_callee

namespace {

// gMouse is made on first use (the same code is inlined five times in the original).
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(online_edit_callee::New(1));
        if (mouse != nullptr) {
            void* raw = online_edit_callee::New(8);
            CMMDefault* mode = nullptr;
            if (raw != nullptr) mode = new (raw) CMMDefault();
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
}

int32_t CurrentMouseMode_2() {
    if (gMouseMode == nullptr) return 0;
    return online_edit_callee::MouseModeOf(gMouseMode);
}

}  // namespace

// 0x101e70ec
int32_t OnlinePanelSubPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x402) ContactMenu(b);
    return window->CallDefaultWindowHandler(message, a, b);
}
// 0x101e71a8
int32_t OnlineEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x20) {
        UIMain_EnableAllKeyTables();
    } else if (message < 0x20) {
        if (message >= 0x1f) UIMain_DisableAllKeyTables();
    } else if (message == 0x1000b) {
        uint32_t modifiers = (static_cast<uint32_t>(b) >> 16) & 0xff;
        if (a == 0xd) {
            if (modifiers != 0) return window->CallDefaultWindowHandler(message, a, b);
            std::u16string text(gOnlineEditText);
            if (gOnlineControl == nullptr) {
                void* raw = online_edit_callee::New(0xe8c);
                COnlineControl* control = nullptr;
                if (raw != nullptr) control = new (raw) COnlineControl();
                gOnlineControl = control;
            }
            gOnlineControl->SendText(text);
            online_edit_callee::WindowSetText(window, gOnlineEmptyHelp);
            return 1;
        }
        if (a < 0xd && a == 9) {
            window->UnCaptureKb(0);
            gOnlinePanel->CaptureKb(0);
            return 1;
        }
        if (a == 0x73) {
            if (modifiers == 0 && gOnlineBlockToggle == 0) {
                uint8_t shown = gOnlinePanelShown;
                UI_SendMessage(gOnlinePanel, 6, shown == 0 ? 1 : 0, 0);
                UI_SendMessage(gOnlineWindow, 6, shown, 0);
                if (gOnlinePanelShown != 0) {
                    InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
                    w = w->GetWindow(0xc312b);
                    w = w->GetWindow(0xc312c);
                    w->CaptureKb(0);
                    EnsureMouse();
                    if (CurrentMouseMode_2() == 9) gOnlinePanel->setCursor(0x13);
                } else {
                    gOnlineWindow->CaptureKb(0);
                }
            }
            return window->CallDefaultWindowHandler(message, a, b);
        }
        if (static_cast<int16_t>(gKeyTableShortcuts->Search(static_cast<int16_t>(a), static_cast<int16_t>(modifiers))) == 0) {
            window->UnCaptureKb(0);
            gOnlinePanel->CaptureKb(0);
            EnsureMouse();
            bool handled;
            if (CurrentMouseMode_2() == 9) {
                void* raw = online_edit_callee::New(8);
                CMMOnline* online = nullptr;
                if (raw != nullptr) online = new (raw) CMMOnline();
                EnsureMouse();
                gMouse->AssignMouseMode(online);
                gOnlineWindow->setCursor(0);
                handled = true;
            } else {
                handled = false;
            }
            if (!handled) CreateMainMenu(0);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v5279: the online panel's contacts and messages lists. Mac file ui_onlineParkFindPanel.cpp.
//   0x101e7648 ContactsLCEntryHandler, 0x101e776c MessagesLCEntryHandler, 0x101e7a58 ContactsLCFormat, 0x101e7b34 ContactsLCPostFormat,
//   0x101e8068 _OnlineAddContact, 0x101e812c _OnlineRemoveContact, 0x101e81f4 _OnlineUpdateContact

uint32_t gContactsFontIndex = 7;                                                  // 0x102b0c4c
uint8_t gMessagesLCColours[32] = {                                                // 0x102b0c58: red, green, blue, spare
    0x00, 0xe2, 0x06, 0x00,  0xff, 0xff, 0xff, 0x00,  0x51, 0x89, 0xef, 0x00,  0xfe, 0x30, 0x30, 0x00,
    0xff, 0xff, 0xff, 0x00,  0x51, 0x89, 0xef, 0x00,  0xfe, 0x30, 0x30, 0x00,  0xf0, 0xff, 0x00, 0x00,
};
uint32_t gContactsLCColours[4] = {0xffffff00u, 0x5189ef00u, 0x00e20600u, 0xfe303000u};   // 0x102b0c78
int32_t gOnlineContactsBusy = 0;                                                  // 0x10395e7c

// 0x101e7648: message 0x403 sets the font colour from the table by the low bits of b (bit 0 clear: word 0; else bit 1 clear: word 3;
// else bit 2 clear: word 1; else word 2); the bytes are the word's, most significant first. Every message then goes to the default handler.
int32_t ContactsLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        uint32_t colour;
        if ((b & 1) == 0)
            colour = gContactsLCColours[0];
        else if ((b & 2) == 0)
            colour = gContactsLCColours[3];
        else if ((b & 4) == 0)
            colour = gContactsLCColours[1];
        else
            colour = gContactsLCColours[2];
        window->SetFontColour(colour >> 24, (colour >> 16) & 0xff, (colour >> 8) & 0xff, 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e776c: message 0x403 sets the font colour from the entry b & 7 of the table.
int32_t MessagesLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        const uint8_t* entry = gMessagesLCColours + ((static_cast<uint32_t>(b) << 2) & 0x1c);
        window->SetFontColour(entry[0], entry[1], entry[2], 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e7a58: the row window, 100 wide, as high as the font (scaled by 0x600 / the real screen height) plus 4.
InterfaceWindow* ContactsLCFormat() {
    InterfaceFont* font = GetFontTableEntry(gContactsFontIndex);
    int16_t height = static_cast<int16_t>(font->GetHeight());
    int16_t bottom = static_cast<int16_t>(height * 0x600 / gUIRealScreenHeight + 4);
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, bottom);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    window->SetCallbackFunction(ContactsLCEntryHandler);
    return window;
}

// 0x101e7b34
void ContactsLCPostFormat(InterfaceWindow* window) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceTextObjectSprite*>(memory));
    window->SetFont(GetFontTableEntry(gContactsFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
}

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

// 0x101e7c18, 180 bytes.
InterfaceWindow* MessagesLCFormat() {
    InterfaceFont* font = GetFontTableEntry(gContactsFontIndex);
    int16_t height = static_cast<int16_t>(font->GetHeight());
    int16_t bottom = static_cast<int16_t>(height * 0x600 / gUIRealScreenHeight + 4);
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, bottom);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    window->SetCallbackFunction(MessagesLCEntryHandler);
    return window;
}

// 0x101e7cf4, 168 bytes.
void MessagesLCPostFormat(InterfaceWindow* window) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceTextObjectSprite*>(memory));
    window->SetFont(GetFontTableEntry(gContactsFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
}

// Unit v5991: the online contacts panel's receive-string / handler / buddy helpers (main app).

int32_t gContactsMaxRows = 20;   // 0x102b0c54: rows kept in the list (initial word 0x14)

// Placeholders for the program (the host cannot call the Mac's virtuals through __ptr_glue); the test library defines the real ones.
namespace contacts_callee {
__attribute__((weak)) int32_t FontCharWidth(InterfaceFont*, char16_t) { return 0; }   // the font's virtual at +0x14 of the table at +0x10
__attribute__((weak)) void WindowSlot1c(InterfaceWindow*, int32_t) {}                  // the window's virtual at +0x1c of the table at +0x118
__attribute__((weak)) COnlineControl* NewOnlineControl() { return new COnlineControl(); }   // operator new(0xe8c) and the constructor
}  // namespace contacts_callee

static int32_t WLen(const char16_t* s) {
    int32_t n = 0;
    while (s[n] != 0) n++;
    return n;
}

// The index of the last character that still fits when the text is laid out from `start`; -1 when nothing starts there.
static int32_t BreakIndex(const char16_t* str, int32_t start, int32_t len, int32_t limit, InterfaceFont* font) {
    int32_t last = -1;
    if (start >= len) return -1;
    int32_t acc = static_cast<int16_t>(contacts_callee::FontCharWidth(font, str[start]));
    int32_t idx = start;
    while (acc < limit && idx < len) {
        last = idx;
        idx++;
        if (str[idx] != 0) acc += static_cast<int16_t>(contacts_callee::FontCharWidth(font, str[idx]));
    }
    return last;
}

// 0x101e7dd8
void OnlineReceiveString(char16_t* text, int32_t data) {
    if (gOnlinePanel == nullptr) return;
    InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc312b));
    InterfaceFont* font = GetFontTableEntry(gContactsFontIndex);
    int16_t left, right;
    list->GetColumnPos(0, &left, &right);
    int32_t limit = (static_cast<int32_t>(gUIRealScreenWidth) * (static_cast<int32_t>(right) - static_cast<int32_t>(left))) / 2048;
    int32_t len = WLen(text);
    int32_t brk = BreakIndex(text, 0, len, limit, font);
    int32_t start = 0;
    while (brk != -1) {
        char16_t saved = text[brk + 1];
        text[brk + 1] = 0;
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = text + start;
        list->AddItem(&item, data, -1, 1);
        text[brk + 1] = saved;
        start = brk + 1;
        len = WLen(text);
        brk = BreakIndex(text, start, len, limit, font);
    }
    for (int32_t n = list->mItemCount; n >= gContactsMaxRows; n--) list->RemoveItem(0);
    list->SetCurrent(list->mItemCount - 1);
}

static COnlineControl* EnsureControl() {
    if (gOnlineControl == nullptr) gOnlineControl = contacts_callee::NewOnlineControl();
    return gOnlineControl;
}

// 0x101e7854
int32_t ContactListHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101) {
        if (a == 0xc3126) {
            gOnlineContactsBusy = 0;
            FillContactList();
        } else {
            gOnlineContactsBusy = 1;
            FillBuddyList();
        }
    } else if (message == 0x100) {
        if (a == 0xc3129) {
            EnsureControl()->VoteForThisPark();
            InterfaceWindow* w = window->GetWindow(0xc3129);
            contacts_callee::WindowSlot1c(w, 0);
        } else if (a < 0xc3129) {
            if (a >= 0xc3128) {
                if (b == 1) EnsureControl()->EarmuffsOn();
                else EnsureControl()->EarmuffsOff();
            }
        } else if (a < 0xc312b) {
            UI_DoPostcard();
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e8068
void _OnlineAddContact(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 0) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = name;
        list->AddItem(&item, data, -1, 1);
    }
}

// 0x101e812c
void _OnlineRemoveContact(char16_t* name) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 0) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) list->RemoveItem(position);
    }
}

// 0x101e81f4
void _OnlineUpdateContact(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 0) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) {
            list->SetData(position, data);
            list->UpdateDisplay();
        }
    }
}

// 0x101e82dc, 152 bytes: empties the contacts list, makes the online control on first use and has it refill the user list.
void FillContactList() {
    InterfaceWindow* sub = gOnlinePanel->GetWindow(0xc3123);
    static_cast<InterfaceListControl2*>(sub->GetWindow(0xc3124))->RemoveAllItems();
    if (gOnlineControl == nullptr) gOnlineControl = contacts_callee::NewOnlineControl();
    gOnlineControl->UpdateUserList();
}

// 0x101e839c
void _OnlineAddBuddy(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 1) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = name;
        list->AddItem(&item, data, -1, 1);
    }
}

// 0x101e845c
void _OnlineRemoveBuddy(char16_t* name) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 1) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) list->RemoveItem(position);
    }
}

// 0x101e8524
void _OnlineUpdateBuddy(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 1) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) {
            list->SetData(position, data);
            list->UpdateDisplay();
        }
    }
}

// The online panel's contact list helpers (unit v5660): the buddy list refill, the buttons menu's message function and the pop-up menu
// for a contact. Names invented.

namespace {

// The online control is made on first use (0xe8c bytes) at every call site of the original.
COnlineControl* OnlineControl() {
    if (gOnlineControl == nullptr) gOnlineControl = new COnlineControl();
    return gOnlineControl;
}

}  // namespace

// Empties the contacts list of the online panel and has the control refill it.
void FillBuddyList() {
    InterfaceWindow* sub = gOnlinePanel->GetWindow(0xc3123);
    static_cast<InterfaceListControl2*>(sub->GetWindow(0xc3124))->RemoveAllItems();
    OnlineControl()->UpdateBuddyList();
}

// The buttons menu's message function: on a button press (0x100) the button's id picks the command on the name kept in extra
// parameter 0 (ids below 0x178a and between 0x1790 and 0x3003 do nothing), then the menu hides. Every message goes on to the default
// handler.
int32_t ButtonsMenuMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        std::u16string name(static_cast<const char16_t*>(window->getExtraParam(0)));
        switch (a) {
            case 0x178a:
            case 0x3004:
                OnlineControl()->BlackMark(name);
                break;
            case 0x178b:
            case 0x3005:
                OnlineControl()->Goto(name);
                break;
            case 0x178c:
            case 0x3006:
                if (b == 1)
                    OnlineControl()->IgnoreOn(name);
                else
                    OnlineControl()->IgnoreOff(name);
                break;
            case 0x178d:
                OnlineControl()->BuddyOff(name);
                break;
            case 0x178e:
                OnlineControl()->Locate(name);
                break;
            case 0x178f:
                OnlineControl()->Page(name);
                break;
            case 0x3007:
                OnlineControl()->BuddyOn(name);
                break;
            default:
                break;
        }
        HideButtonsMenu();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v51037, main app: the buttons menu underlay's message function and its hide function (Mac file ui_onlineParkFindPanel.cpp), the static
// initialisers of the online park info and list panels, RemoveOnlineParkListPanel, and four small UIInterface functions.

static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= 64, "string storage");

namespace {
InterfaceWindow* WindowFromWord(int32_t word) {
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(word)));
}
}  // namespace

// 0x101e8b18 (88 bytes): hides the menu window the word gOnlineWord88 holds and gives the keyboard back (to the online panel's window).
void HideButtonsMenu() {
    WindowFromWord(gOnlineWord88)->Show(0);
    WindowFromWord(gOnlineWord88)->unlink();
    gOnlinePanel->CaptureKb(0);
}

// 0x101e86c4 (180 bytes)
int32_t ButtonsUnderlayMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        HideButtonsMenu();
    } else if (message < 0x10006 && message == 0x14) {
        if (window == gOnlineWindowE90) {
            gOnlineWindowE90 = nullptr;
        } else {
            gOnlineWindowE8c = nullptr;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Pops the buttons menu up at the mouse for the contact at `position` of the contacts list (nothing for the player's own name). Bit 0 of
// the contact's data picks the menu (set: the one in gOnlineWindowE90, else gOnlineWindowE8c), bit 3 the state of its ignore button.
void ContactMenu(int32_t position) {
    InterfaceWindow* panel = gOnlinePanel->GetWindow(0xc3123);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(panel->GetWindow(0xc3124));
    int32_t data[4];
    list->GetData(position, data);
    UI_LCAdd item;
    list->GetItem(position, 0, item);
    if (OnlineControl()->IsThisMe(item.mText)) return;
    InterfaceWindow* menu;
    InterfaceWindow* sub;
    InterfaceWindow* button;
    int32_t subId, buttonId;
    if (data[0] & 1) {
        gOnlineWord88 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gOnlineWindowE90));
        subId = 0x1789;
        buttonId = 0x178c;
    } else {
        gOnlineWord88 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gOnlineWindowE8c));
        subId = 0x3003;
        buttonId = 0x3006;
    }
    // The original reloads the menu from gOnlineWord88 (a window pointer in a word); the local copy is the same window.
    menu = (data[0] & 1) ? gOnlineWindowE90 : gOnlineWindowE8c;
    sub = menu->GetWindow(subId);
    gOnlinePanel->GetWindow(0xc3123)->GetWindow(0xc3124)->link(menu, 1, 1);
    {
        CoolRect rect;
        sub->GetPosition(&rect, 1);
        int16_t mouseX, mouseY;
        UI_GetMouseXY(mouseX, mouseY);
        sub->ShiftPosition(static_cast<int16_t>(mouseX - rect.mLeft), static_cast<int16_t>(mouseY - rect.mTop));
        menu->setZ(static_cast<int16_t>(menu->mZ + 4));
        menu->Show(1);
        menu->CaptureKb(0);
    }
    button = sub->GetWindow(buttonId);
    static_cast<InterfaceButton*>(button)->SetState((data[0] & 8) ? 1 : 0, 1);
    sub->setExtraParam(0, const_cast<char16_t*>(item.mText));
}

// OnlineParkFindPanelMsg 0x101e93b0 (700 bytes) and CreateOnlinePanel 0x101e8f08 (916 bytes), Mac file ui_onlineParkFindPanel.cpp.

// The three window scripts: the words are the image's, generated at configure time (devtools/gen_game_tables.py, WindowScripts.inc).
int16_t gOnlinePanelScript[] = {TPW_WINDOW_SCRIPT_102B0834};       // 0x102b0834
int16_t gOnlineUnderlayScriptA[] = {TPW_WINDOW_SCRIPT_102B0A22};   // 0x102b0a22
int16_t gOnlineUnderlayScriptB[] = {TPW_WINDOW_SCRIPT_102B0B5E};   // 0x102b0b5e
uint32_t gOnlineEditFontIndex = 7;              // 0x102b0c50
uint8_t gOnlineByte78;                          // 0x10395e78
int32_t gOnlineWord88;                          // 0x10395e88
char16_t gOnlineEditText[256];                  // 0x10395e9c
// Builds the online panel (script 0x102b0834, message function OnlinePanelMsg) with its contacts and messages lists, the edit box and the
// two button windows, then the full screen window over the root.
void CreateOnlinePanel() {
    gOnlineContactsBusy = 0;
    gOnlinePanel = gUIRootWindow->Build(gOnlinePanelScript, OnlinePanelMsg);
    if (gOnlinePanel != nullptr) {
        InterfaceWindow* sub = gOnlinePanel->GetWindow(0xc3123);
        sub->SetCallbackFunction(OnlinePanelSubPanelMsg);
        InterfaceListControl2* contacts = static_cast<InterfaceListControl2*>(sub->GetWindow(0xc3124));
        contacts->SetColumnFormatsAll(ContactsLCFormat, ContactsLCPostFormat);
        contacts->SetColumnType(0, static_cast<LCDataType>(0));
        contacts->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        contacts->SetCallbackFunction(ContactListHandler);
        InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(contacts->GetWindow(0xc3125));
        checks->SetCurrent(0xc3126);
        InterfaceListControl2* messages = static_cast<InterfaceListControl2*>(sub->GetWindow(0xc312b));
        messages->SetColumnFormatsAll(MessagesLCFormat, MessagesLCPostFormat);
        messages->SetColumnType(0, static_cast<LCDataType>(0));
        InterfaceWindow* edit = messages->GetWindow(0xc312c);
        edit->SetCallbackFunction(OnlineEditMsg);
        InterfaceCursorPosTextObjectSprite* sprite = new InterfaceCursorPosTextObjectSprite(nullptr);
        edit->SetClientDrawObject(sprite);
        edit->SetFont(GetFontTableEntry(gOnlineEditFontIndex));
        static_cast<InterfaceEditBox*>(edit)->SetStaticTextArea(gOnlineEditText, 0xff);
        static_cast<InterfaceEditBox*>(edit)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        edit->SetText(gOnlineEmptyHelp);   // the virtual at table offset 0x2c
        gOnlinePanelShown = 1;
        CoolRect rect;
        gOnlinePanel->GetPosition(&rect, 1);
        gGameWindowCentreX = gScreenCentreX;
        gGameWindowCentreY = gScreenCentreY;
        gGameWindowHalfWidth = gViewCentreX;
        gGameWindowHalfHeight = gViewCentreY;
    }
    gOnlineWindowE90 = gUIRootWindow->Build(gOnlineUnderlayScriptA, ButtonsUnderlayMsg);
    gOnlineWindowE90->Show(0);
    gOnlineWindowE90->unlink();
    gOnlineWindowE90->GetWindow(0x1789)->SetCallbackFunction(ButtonsMenuMsg);
    gOnlineWindowE8c = gUIRootWindow->Build(gOnlineUnderlayScriptB, ButtonsUnderlayMsg);
    gOnlineWindowE8c->Show(0);
    gOnlineWindowE8c->unlink();
    gOnlineWindowE8c->GetWindow(0x3003)->SetCallbackFunction(ButtonsMenuMsg);
    gOnlineWord88 = 0;
    gOnlineWindow = gUIRootWindow->CreateObject(1, 0, static_cast<int32_t>(0xfff04d2a), 0, 0, 0x7ff, 0x5ff);
    gOnlineWindow->SetCallbackFunction(OnlinePanelMsg);
    MoveHelpText(-1, 10);
    gOnlineCursorState = 0;
    gOnlineByte78 = 0;
}

// 0x101e92c4, 56 bytes.
void RemoveOnlinePanel() { UI_SendMessage(gOnlinePanel, 4, 0, 0); }
