// ui_main.cpp (main app), unit w101: the panel plumbing every UI panel goes through. UIMain_SetPanel 0x101805a0 and UIMain_CheckPanel
// 0x10180514 keep the one panel window (gUIPanelWindow, src/ui_main_kbcapture.cpp) and the mouse; UIMain_DefaultPanelMsg 0x1018493c is the
// message function the panels share; UIMain_SetTitleText 0x10180794 gives a window the title look; UIMain_UISound 0x101802cc, the two advisor
// message hooks 0x101821cc and 0x1018227c and UIMain_GetFullyOutOfFirstPerson 0x10187434 are short.
#include <cstdint>
#include <new>

#include "AdvisorQueue.h"
#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "ui_main.h"

namespace {
// The messages a window hears (the numbers are the Mac's; the names are not).
constexpr int32_t kMsgClose = 4;           // asks a panel to close
constexpr int32_t kMsgCheck = 5;           // CheckPanel's: the panel is being taken away
constexpr int32_t kMsgRemoved = 0x14;      // the window is being deleted
constexpr int32_t kMsgKeyDown = 0x1000a;
constexpr int32_t kMsgKeyTyped = 0x1000c;   // (0x1000b between them is the key going up)
constexpr int32_t kModeDefaultReplace = 9;  // the current mouse mode's answer that makes SetPanel put the default mode back
constexpr int32_t kEscape = 0x1b;
constexpr int32_t kKeyF1 = 0x70;           // plays the advisor's last tutorial message

// The Mac makes the mouse and its default mode in this order twice inline (the second copy is for a missing mouse after the mode test): the
// mouse (an empty class on the Mac, operator new(1); the host's has one word), then the default mode (operator new(8), constructed only when
// it was made), which the mouse takes; then the two words of the mouse's data are set to -1. A mouse that could not be made leaves the words
// alone. The answer is the mouse, NULL when it could not be made.
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
}  // namespace

// 0x101802cc, 64 bytes.
void UIMain_UISound(int32_t code) {
    SOUND_SendEvent(0, gSoundBanks[3], code, 0, 0, 0);
}

// 0x10180514, 100 bytes. The message is sent before the global is cleared.
bool UIMain_CheckPanel() {
    InterfaceWindow* panel = gUIPanelWindow;
    if (panel != nullptr) {
        UI_SendMessage(panel, kMsgCheck, 0, 0);
        gUIPanelWindow = nullptr;
        UIMain_DecideKbCapture();
    }
    return panel != nullptr;
}

// 0x101805a0, 444 bytes. With a window: a mouse is made if there is none; a current mode that answers 9 is replaced by a fresh default mode
// (a mouse is made first when the first step could not make one: it is then used with no test for NULL, as on the Mac); the cursor is
// reset, three key tables turned off and the slide-out panel removed. Without a window only the last two lines run.
void UIMain_SetPanel(InterfaceWindow* window) {
    if (window != nullptr) {
        if (gMouse == nullptr) gMouse = MakeMouse();
        int32_t mode = 0;
        if (gMouseMode != nullptr) mode = gMouseMode->GetMode();
        if (mode == kModeDefaultReplace) {
            CMMDefault* fresh = new (std::nothrow) CMMDefault;
            if (gMouse == nullptr) gMouse = MakeMouse();
            gMouse->AssignMouseMode(fresh);
        }
        UI_SetGameWindowCursor(0);
        gKeyTableCamera->Disable();
        gKeyTableCheat->Disable();
        gKeyTableGame->Disable();
        RemoveSlideoutPanel(1);
    }
    gUIPanelWindow = window;
    UIMain_DecideKbCapture();
}

// 0x10180794, 364 bytes. The title draw object is made once (the Mac's is 0x19c bytes, constructed only when made). A child that has no draw
// object yet is widened by half its width on each side (the half rounded toward zero; the rectangle's shorts wrap), given the title draw
// object (no pre-draw callback), the fifth font of the current resolution group, centred both ways, and the title colour; the text goes to
// the child either way.
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    if (gTitleText == nullptr) gTitleText = new (std::nothrow) InterfaceTitleTextObject(nullptr);
    InterfaceWindow* child = window->GetWindow(id);
    if (child != nullptr) {
        if (child->mDrawObject == nullptr) {
            CoolRect rect;
            child->GetPosition(&rect, 1);
            const int32_t width = rect.mRight - rect.mLeft;
            const int32_t half = (width >> 1) + ((width < 0 && (width & 1) != 0) ? 1 : 0);
            rect.Set(static_cast<int16_t>(rect.mLeft - half), rect.mTop, static_cast<int16_t>(rect.mRight + half), rect.mBottom);
            child->SetPosition(&rect);
            gTitleText->SetPreDrawCallback(nullptr);
            child->SetDrawObject(gTitleText);
            child->SetFont(&gText.mFonts[gText.mResolutionGroup * 13 + 5]);
            child->SetFontJust(1, 1);
            UI_TITLE_COLOUR(child);
        }
        child->SetWindowDrawText(text);
    }
}

// 0x101821cc, 116 bytes. The request is built on the stack with the thing id 0: the Mac stores 0 in the high half of the word that carries
// the id (the id rides there) and the low half is stack padding the constructor never reads. Secondary 0, response -1, no override, score -1.
void UIMain_AdvisorMessage(AdvisorMessage message) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing;
    thing.mIndex = 0;
    CAdvisorRequest request(message, thing, 0, -1, false, -1);
    gGameWorld->GetAdvisor()->QueueRequest(request);
}

// 0x1018227c, 68 bytes.
void UIMain_CancelAdvisorMessage(AdvisorMessage message) {
    gGameWorld->GetAdvisor()->CancelMessage(message);
}

// 0x1018493c, 316 bytes. The key modifiers are bits 16 to 23 of the last argument. Key down goes to the shortcuts table (answer 0); key up
// to the advisor for F1 and a close message for Escape (both only without modifiers), else to the table (answer 0); a window being removed
// forgets the panel first; everything else, and the key typed message, is the window's default handler's.
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    const int16_t modifiers = static_cast<int16_t>((static_cast<uint32_t>(b) >> 16) & 0xff);
    if (message == kMsgKeyDown) {
        gKeyTableShortcuts->KeyDown(static_cast<int16_t>(a), modifiers);
        return 0;
    }
    if (message < kMsgKeyDown) {
        if (message == kMsgRemoved) {
            gUIPanelWindow = nullptr;
            UIMain_DecideKbCapture();
        }
    } else if (message < kMsgKeyTyped) {
        if (a == kEscape && modifiers == 0) {
            UI_SendMessage(window, kMsgClose, 0, 0);
        } else if (a == kKeyF1 && modifiers == 0) {
            gGameWorld->GetAdvisor()->PlayLastTutorialMessage();
        } else {
            gKeyTableShortcuts->KeyUp(static_cast<int16_t>(a), modifiers);
        }
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10187434, 100 bytes. Walking around (bit 2) is toggled off; riding a ride in first person (bit 4) is stopped, and walking around
// (bit 0x400) toggled off after it.
void UIMain_GetFullyOutOfFirstPerson() {
    const uint32_t flags = gFirstPersonFlags;
    if ((flags & 2) != 0) {
        ToggleFirstPersonWalkAround();
    } else if ((flags & 4) != 0) {
        StopRidingRideIn1stPerson();
        if ((flags & 0x400) != 0) ToggleFirstPersonWalkAround();
    }
}
