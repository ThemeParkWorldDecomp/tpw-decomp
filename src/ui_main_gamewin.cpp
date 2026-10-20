// ui_main.cpp (main app), unit w136: the message functions of the park's two big windows. Engine_GameWinMsg 0x10183c84 (2408 bytes) is
// gGameWindow's, the full-screen park window: it turns the pointer, button and key messages into mouse modes, camera keys and context menus.
// Engine_GameFirstPersonMsg 0x10184628 (720 bytes) is gFirstPersonWindow's: camera keys, leaving first person, the postcard key. Both answer
// what InterfaceWindow::CallDefaultWindowHandler answers.
//
// A position (the b of a button or key message) packs x in the HIGH half (arithmetic shift) and y in the low half (unsigned); x is scaled by
// the real screen's width / 2048 and y by its height / 0x600, both truncating toward zero.
#include <cstdint>
#include <cstdlib>
#include <new>

#include "Cheat.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "ui_Postcard.h"
#include "ui_main.h"

// The function statics of Engine_GameWinMsg and their guards (names invented; BSS addresses in ui_main.h). All start at 0.
int32_t gGameWinRightDown;
int8_t gGameWinRightDownGuard;
int32_t gGameWinPressTicks;
int8_t gGameWinPressTicksGuard;
int32_t gGameWinCameraOn;
int8_t gGameWinCameraOnGuard;
int16_t gGameWinPressX;
int16_t gGameWinPressY;
int32_t gGameWinPointerIn;
int32_t gGameWinWordC34;
int32_t gGameWinWordC38;
int8_t gGameWinCheatGuard;
alignas(4) uint8_t gGameWinCheat[12];
int32_t gRideHighlightKeep;

namespace {
constexpr int32_t kMsgDestroyed = 0x14;
constexpr int32_t kMsgCreated = 0x15;
constexpr int32_t kMsgFrame = 0x1e;
constexpr int32_t kMsgPointerEnter = 0x10001;
constexpr int32_t kMsgPointerLeave = 0x10002;
constexpr int32_t kMsgPointerMove = 0x10003;
constexpr int32_t kMsgButtonUp = 0x10004;
constexpr int32_t kMsgButtonDown = 0x10005;
constexpr int32_t kMsgClick = 0x10006;
constexpr int32_t kMsgKeyDown = 0x1000a;
constexpr int32_t kMsgKeyUp = 0x1000b;
constexpr int32_t kFirstPersonBits = 0x16;   // gFirstPersonFlags: walking (2), riding (4) and 0x10
constexpr int32_t kKeyEscape = 0x1b;
constexpr int32_t kRightPressLimit = 200;    // ticks a right press may last before it is a drag

// The mouse, made on first use as UIMain_SetPanel makes it: operator new(1), a default mode (operator new(8), built when it was made), the
// mode assigned, the two words of the mouse's data set to -1. A mouse that could not be made leaves the words alone; the answer is the mouse
// (NULL when it could not be made) and the caller stores it.
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}

void EnsureMouse() {
    if (gMouse == nullptr) gMouse = MakeMouse();
}

int32_t ScaleX(int32_t value) {
    return value * gUIRealScreenWidth / 2048;
}

int32_t ScaleY(int32_t value) {
    return value * gUIRealScreenHeight / 0x600;
}

// The scaled position of a message's b: x from the high half (signed), y from the low half (unsigned).
int32_t MessageX(int32_t b) {
    return ScaleX(b >> 16);
}

int32_t MessageY(int32_t b) {
    return ScaleY(static_cast<int32_t>(static_cast<uint32_t>(b) & 0xffff));
}

// The camera key table's key for a mouse button: a - 0x10 as a short (the listing adds 0x10000 first, which the short drops).
int16_t CameraKey(int32_t a) {
    return static_cast<int16_t>(static_cast<uint32_t>(a) - 0x10);
}

// The static CCheat, built on first use.
CCheat* GameWinCheat() {
    CCheat* cheat = reinterpret_cast<CCheat*>(gGameWinCheat);
    if (gGameWinCheatGuard == 0) {
        new (cheat) CCheat;
        gGameWinCheatGuard = 1;
    }
    return cheat;
}

// Leaving first person (both message functions): the cursor, the walk-around toggle when walking (bit 2) or riding off the ride (bit 4),
// then the cursor 0x15. The flags are read again after the first cursor call. The caller has tested the 0x16 bits.
void LeaveFirstPerson(InterfaceWindow* window) {
    window->setCursor(1);
    const uint32_t flags = gFirstPersonFlags;
    if ((flags & 2) != 0) ToggleFirstPersonWalkAround();
    else if ((flags & 4) != 0) StopRidingRideIn1stPerson();
    window->setCursor(0x15);
}
}  // namespace

// 0x10183c84, 2408 bytes.
int32_t Engine_GameWinMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (gGameWinRightDownGuard == 0) {
        gGameWinRightDown = 0;
        gGameWinRightDownGuard = 1;
    }
    if (gGameWinPressTicksGuard == 0) {
        gGameWinPressTicks = 0;
        gGameWinPressTicksGuard = 1;
    }
    if (gGameWinCameraOnGuard == 0) {
        gGameWinCameraOn = 1;
        gGameWinCameraOnGuard = 1;
    }
    gGameWinCameraOn = 1;

    // First switch: the right button. Off, a right button (a == 1) only takes or gives back the pointer.
    if (!gPlayerSettings.mCurrent.mRMBCancelOn) {
        if (a == 1) {
            if (message == kMsgButtonDown) window->CaptureDevice();
            else if (message == kMsgButtonUp) window->UnCaptureDevice();
        }
    } else if (message == kMsgPointerMove) {
        // The press is watched until the pointer has moved 9 or more (the position of the move is in a, the press's in b).
        if (gGameWinRightDown != 0) {
            const bool still = std::labs((a >> 16) - static_cast<int32_t>(gGameWinPressX)) < 9 &&
                               std::labs(static_cast<int32_t>(static_cast<uint32_t>(a) & 0xffff) - static_cast<int32_t>(gGameWinPressY)) < 9;
            if (!still) {
                Will_HandleCameraMessages(kMsgButtonDown, 1, 0);
                gGameWinRightDown = 0;
            }
        }
    } else if (message < kMsgPointerMove) {
        if (message == kMsgFrame && gGameWinRightDown != 0) {
            const int32_t held = static_cast<int32_t>(IveGotTicks() - static_cast<uint32_t>(gGameWinPressTicks));
            if (held > kRightPressLimit) {
                Will_HandleCameraMessages(kMsgButtonDown, 1, 0);
                gGameWinRightDown = 0;
            }
        }
    } else if (message == kMsgButtonDown) {
        if (a == 1) {
            window->CaptureDevice();
            EnsureMouse();
            // The current mode is asked (its answer is dropped).
            if (gMouseMode != nullptr) gMouseMode->GetMode();
            gGameWinRightDown = 1;
            gGameWinPressTicks = static_cast<int32_t>(IveGotTicks());
            gGameWinCameraOn = 0;
            gGameWinPressX = static_cast<int16_t>(b >> 16);
            gGameWinPressY = static_cast<int16_t>(b);
        }
    } else if (message < kMsgButtonDown && a == 1) {
        // Message 0x10004: the right button is released. A press that is still watched gives the mouse a NEW default mode, made before the
        // mouse itself is checked.
        window->UnCaptureDevice();
        if (gGameWinRightDown != 0) {
            CMMDefault* fresh = new (std::nothrow) CMMDefault;
            EnsureMouse();
            gMouse->AssignMouseMode(fresh);
            LLCC_SystemSetMode(0, true);
            EndCoasterEditMode();
        }
        gGameWinRightDown = 0;
    }

    if (gGameWinCameraOn != 0) Will_HandleCameraMessages(message, a, b);

    // Second switch.
    if (message == kMsgFrame) {
        EnsureMouse();
        gMouse->ImmediateMouseHandler(ScaleX(gUIPointerX), ScaleY(gUIPointerY));
        if (gRideHighlightKeep == 0) UnhighlightRide();
        if (gUIPanelWindow == nullptr && gGameWinPointerIn != 0) ContextMouse(0);
    } else if (message == kMsgCreated) {
        gGameWinRightDown = 0;
        gGameWinPointerIn = 0;
    } else if (message == kMsgDestroyed) {
        gGameWindow = nullptr;
    } else if (message == kMsgButtonDown) {
        CKeyTable* camera = gKeyTableCamera;
        camera->KeyDown(CameraKey(a), CheckShifts());
        if (gUIPanelWindow == nullptr && a != 2) {
            int32_t go = 1;
            if (a == 0) {
                ContextMouse(0);
                go = ContextMouseClick();
            }
            if (go != 0) CMouse::ProcessIngameMouse(a == 0 ? 0xf0 : 0xf1, 1, MessageX(b), MessageY(b), nullptr);
        }
    } else if (message == kMsgPointerEnter) {
        gGameWinWordC34 = 0;
        if (gUIPanelWindow == nullptr) ContextMouse(0);
        gGameWinPointerIn = 1;
        gGameWinWordC38 = 0;
    } else if (message == kMsgPointerLeave) {
        gGameWinPointerIn = 0;
    } else if (message == kMsgButtonUp) {
        gKeyTableCamera->KeyUp(CameraKey(a), CheckShifts());
        EnsureMouse();
        gMouse->ImmediateMouseHandler(MessageX(b), MessageY(b));
        CMouse::ProcessIngameMouse(a == 0 ? 0xf0 : 0xf1, 0, MessageX(b), MessageY(b), nullptr);
    } else if (message == kMsgKeyDown) {
        // The tables in the order debug, camera, game, shortcuts; the first that answers stops. None: the cheat table, when cheating.
        const int16_t key = static_cast<int16_t>(a);
        const int16_t modifiers = static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff);
        if (gKeyTableDebug->KeyDown(key, modifiers) == 0 && gKeyTableCamera->KeyDown(key, modifiers) == 0 &&
            gKeyTableGame->KeyDown(key, modifiers) == 0 && gKeyTableShortcuts->KeyDown(key, modifiers) == 0) {
            if (GameWinCheat()->IsCheating() != 0) gKeyTableCheat->KeyDown(key, modifiers);
        }
    } else if (message == kMsgKeyUp) {
        const int16_t key = static_cast<int16_t>(a);
        const int16_t modifiers = static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff);
        if (gKeyTableDebug->KeyUp(key, modifiers) == 0 && gKeyTableCamera->KeyUp(key, modifiers) == 0 &&
            gKeyTableGame->KeyUp(key, modifiers) == 0 && gKeyTableShortcuts->KeyUp(key, modifiers) == 0) {
            if (GameWinCheat()->IsCheating() != 0) gKeyTableCheat->KeyUp(key, modifiers);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10184628, 720 bytes.
int32_t Engine_GameFirstPersonMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMsgDestroyed) {
        gFirstPersonWindow = nullptr;
    } else if (message == kMsgButtonUp) {
        gKeyTableCamera->KeyUp(CameraKey(a), CheckShifts());
    } else if (message == kMsgButtonDown) {
        gKeyTableCamera->KeyDown(CameraKey(a), CheckShifts());
    } else if (message == kMsgKeyDown) {
        gKeyTableCamera->KeyDown(static_cast<int16_t>(a), static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff));
    } else if (message == kMsgClick) {
        if (gPlayerSettings.mCurrent.mRMBCancelOn && a == 1 && (gFirstPersonFlags & kFirstPersonBits) != 0) LeaveFirstPerson(window);
    } else if (message == kMsgKeyUp) {
        const int16_t key = static_cast<int16_t>(a);
        const int16_t modifiers = static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff);
        gKeyTableCamera->KeyUp(key, modifiers);
        // Escape leaves without asking the shortcuts table; otherwise its answer (a short) 0x10 leaves and, asked AGAIN, 0xf is the postcard.
        if (a == kKeyEscape || static_cast<int16_t>(gKeyTableShortcuts->Search(key, modifiers)) == 0x10) {
            if ((gFirstPersonFlags & kFirstPersonBits) != 0) LeaveFirstPerson(window);
        } else if (static_cast<int16_t>(gKeyTableShortcuts->Search(key, modifiers)) == 0xf) {
            UI_DoPostcard();
        }
    } else if (message == 0x100) {
        if ((gFirstPersonFlags & kFirstPersonBits) != 0) LeaveFirstPerson(window);
    }
    Will_HandleCameraMessages(message, a, b);
    return window->CallDefaultWindowHandler(message, a, b);
}
