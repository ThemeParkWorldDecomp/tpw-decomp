// ui_main.cpp: name proven by __sinit_ui_main_cpp in the main application.

#define TPW_UIMENU_CTOR_EXTERNAL
#define TPW_V51039_STRONG
#include "AdvisorQueue.h"
#include "analyser.h"
#include "Cheat.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "InGame.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keyboard.h"
#include "LbDraw.h"
#include "LbTime.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "MapCell.h"
#include "MapWho.h"
#include "Mouse.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "Quickload.h"
#include "RideDB.h"
#include "rsse.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "Serialise.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "TbAudioSystem.h"
#include "TbColour.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TPString.h"
#include "TrackCell.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "ui_textmanager.h"
#include "ui_tooltip.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <new>

// ui_main.cpp, main app 0x1017fdf8 and 0x101867f8.

int32_t EffectPeekMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t) {
    CoolRect rect;
    if (message == 0x100)
        SOUND_SendEvent(0, gSoundBanks[3], (window->GetAttr() & 0x10) != 0 ? 0xbd : 0x1f, 0, 0, 0);
    if (message == 0x14 || message == 0x20) {
        if (window->WhatAmI() == 5) gUIIME.DeActivate();
    } else if (message == 0x1f) {
        if (window->WhatAmI() == 5) gUIIME.Activate(window);
    } else if (message == 0x801) {
        gUIIME.InvalidateStoredString(window->GetWindow(a));
    }
    if (gPlayerSettings.mCurrent.mTooltipsOn != 0) {
        if (message == 0x14) {
            if (window->WhatAmI() == 2) {
                auto* button = static_cast<InterfaceButton*>(window);
                if (button->mHilight == 1 || button->GetState() == 1) BTN_HighLight_Kill();
            }
        } else if (message == 0x11) {
            if (window->WhatAmI() == 2 && static_cast<InterfaceButton*>(window)->mHilight == 1) BTN_HighLight_Kill();
        } else if (message == 0x13 || message == 0x10001) {
            if (window->WhatAmI() == 2 &&
                (message == 0x10001 || static_cast<InterfaceButton*>(window)->GetState() == 1)) {
                window->GetPosition(&rect, 1);
                const int32_t width = rect.mRight - rect.mLeft;
                const int32_t height = rect.mBottom - rect.mTop;
                // Signed division truncates toward zero, matching srawi followed by addze.
                const float x = static_cast<float>(rect.mLeft + width / 2 - 1024) * (1.0f / 1024.0f);
                const float y = static_cast<float>(rect.mTop + height / 2 - 768) / 768.0f;
                if (message == 0x10001)
                    BTN_HighLight_On(x, y, static_cast<float>(width) / 2048.0f, static_cast<float>(height) / 1536.0f);
                else
                    BTN_HighLight_Move(x, y);
            }
        } else if (message == 0x10002 && window->WhatAmI() == 2) {
            BTN_HighLight_Off();
        }
    }
    return 0;
}

// GetFontTableEntry, main 0x10180270 (48 bytes); existing body separated for unit w43.

InterfaceFont* GetFontTableEntry(uint32_t index) {
    if (index >= 13) return nullptr;
    return &gText.mFonts[gText.mResolutionGroup * 13 + index];
}

// ui_main.cpp (main app), unit w101: the panel plumbing every UI panel goes through. UIMain_SetPanel 0x101805a0 and UIMain_CheckPanel
// 0x10180514 keep the one panel window (gUIPanelWindow, src/ui_main_kbcapture.cpp) and the mouse; UIMain_DefaultPanelMsg 0x1018493c is the
// message function the panels share; UIMain_SetTitleText 0x10180794 gives a window the title look; UIMain_UISound 0x101802cc, the two advisor
// message hooks 0x101821cc and 0x1018227c and UIMain_GetFullyOutOfFirstPerson 0x10187434 are short.

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

// ui_main.cpp (main app of SimTheme Park 2.04b3). PARTIAL: UIMain_ConvertMulti
// (0x10180334 and 0x101803cc, the same code for a const and a non-const argument) and UIMain_ConvertString (0x10181a7c) so far.

char16_t gUIMainConvertBuffer[0x4000];   // 0x10380c48 (name invented; 0x8000 bytes: the next global is at 0x10388c48)

// A multibyte text as Unicode, in one shared buffer of 0x200 characters; NULL for NULL.
// The text is converted only when the language's Unicode-to-multibyte converter is
// loaded (resource 1), though it is the multibyte-to-Unicode one (resource 0) that does
// the work.
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    if (text == nullptr) return nullptr;
    if (gText.mResourcePointers[1] != nullptr) {
        static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0])->ConvertString(gUIMainConvertBuffer, text, 0x200);
    }
    return gUIMainConvertBuffer;
}

char16_t* UIMain_ConvertMulti(unsigned char* text) { return UIMain_ConvertMulti(static_cast<const unsigned char*>(text)); }

// The player login panel (main app, ui_playerLogin.cpp, 0x101ae39c and 0x101ae9e0) and two small helpers of the interface (UIMain_GetString,
// ui_main.cpp, and UI_TITLE_COLOUR, ui_utils.cpp). Unit w39.
//
// The panel is the window script gLoginPanelWindowData (src/ui_loginpanel_data.cpp) built under the interface's root window. It has four
// rows, one for each saved player (the row windows 0x7a14, 0x7a19, 0x7a1a and 0x7a1b), each with a name area (0x7a15), a button (0x7a16)
// and a picture (0x7a17, shown for a player in the easy game); a fifth row (0x7a1c) carries the text of the quit button. CreateLoginPanel
// dresses the name areas with title text objects and FillLoginNames puts the players' names in them.
//
// Quirks kept: FillLoginNames and CreateLoginPanel call GetWindow on the answer of the one before without a test (a panel without the
// rows crashes them); the easy-mode answer is cut to a short before it is passed to Show; UI_TITLE_COLOUR takes no theme number (the
// colour is fixed).

// ---- the two helpers

const char16_t* UIMain_GetString(int32_t id) {
    const unsigned char* text = static_cast<const TbITable*>(gText.mResourcePointers[6])->GetConstString(static_cast<uint32_t>(id));
    if (text == nullptr) return nullptr;
    // (converted only when the Unicode-to-multibyte converter is loaded too, as UIMain_ConvertMulti does)
    if (gText.mResourcePointers[1] != nullptr)
        static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0])->ConvertString(gUIMainConvertBuffer, text, 0x200);
    return gUIMainConvertBuffer;
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

// Unit v51039 (main app): std::list<std::wstring>::clear and the destructors of the swear filter and of the theme map, the font
// wrapper's GetCharWidth, the park list's StoreReceivedList and four small UI helpers. The Mac's strings, heap and virtual calls are
// the callees of the headers' *_callee namespaces (weak definitions in the _callees file; the test library fakes them).

// ---- the weak definitions of the callees the host cannot call as the Mac does (the Mac's strings and heap, new plus constructor,
// virtual calls through __ptr_glue); the unit's test library defines them again as recording fakes.
#define TPW_WEAK __attribute__((weak))
namespace indent_callee {
// (virtual calls only: no symbol of another file is needed)
TPW_WEAK void Adjust(void* rect, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    static_cast<CoolRect*>(rect)->Adjust(left, top, right, bottom);
}
TPW_WEAK void RectDtor(void* rect, int16_t) { static_cast<CoolRect*>(rect)->~CoolRect(); }
}  // namespace indent_callee
// 0x10180940 (156)
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) {
    alignas(16) unsigned char rect[64];
    indent_callee::RectCtor(rect);
    indent_callee::GetPosition(window, rect, 1);
    indent_callee::Adjust(rect, a, b, static_cast<int16_t>(-a), static_cast<int16_t>(-b));
    indent_callee::SetClientRect(window, rect);
    indent_callee::RectDtor(rect, -1);
}

// 0x10180a1c (84): the frame of a staff state, 9 for a state outside 0..4.
int32_t UIMain_GetStaffStateFrame(int32_t state) {
    if (state == 3) return 2;
    if (state < 3) {
        if (state < 2) return state >= 0 ? 0 : 9;
        return 1;
    }
    return state < 5 ? 3 : 9;
}

// 0x10180aa0 (160). QUIRK: the converter used is resource 0 (+0x54c) but the test is on resource 1 (+0x550); an unconverted buffer is
// passed on when that one is missing.
void UIMain_SetStaffStateText(InterfaceWindow* window, int32_t state) {
    char16_t* text = gUIMainConvertBuffer;
    const unsigned char* string = staffstate_callee::GetConstString(gText.mResourcePointers[10], static_cast<uint32_t>(state));
    if (string == nullptr) {
        text = nullptr;
    } else if (gText.mResourcePointers[1] != nullptr) {
        staffstate_callee::ConvertString(gText.mResourcePointers[0], text, string, 0x200);
    }
    staffroom_callee::WindowSetText(window, text);
}

// 0x10180b84, 1028 bytes. The panel state of a ride or shop: 3 condemned, 0x19/0x1a track problems, 5 to 9 upgrade wanted, 0xb to 0xf
// flagged for maintenance with relief, 0x11 to 0x15 flagged without, 2 or 0x15 broken down, 0x16/0x17 no queue, 0x18 exit unconnected,
// else 1 or 0.
int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    CWorld* world;
    RideInfo* ride = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType});
    if (object->IsCondemned()) return 3;
    const int32_t kind = ride->Value(kRideSlot_WhichTrackType).mInteger;
    if (kind == 3) {
        if (!IsTrackClosed(object)) return 0x19;
        if (!IsTrackValid(object)) return 0x1a;
    } else if (kind == 1 || kind == 2) {
        if (object->mF28 == 0) return 0x19;
    }
    if (object->RequiresUpgrade()) {
        world = gGameWorld;
        if (world->mF1DA73E == gUIMainId7858) return 5;
        if (world->GetStaffHQ()->IsStriking(static_cast<StaffType>(1))) return 7;
        if (object->GetAssignedStaffMember().mIndex == gUIMainId785A) return 6;
        return object->IsBeingWorkedUpon() ? 9 : 8;
    }
    if (object->IsFlaggedForMaintenance()) {
        world = gGameWorld;
        if (object->mFlags & 1) {
            if (world->mF1DA73C == gUIMainId785C) return 0xb;
            if (world->GetStaffHQ()->IsStriking(static_cast<StaffType>(0))) return 0xd;
            if (object->GetAssignedStaffMember().mIndex == gUIMainId785E) return 0xc;
            return object->IsBeingWorkedUpon() ? 0xf : 0xe;
        }
        if (world->mF1DA73E == gUIMainId7860) return 0x11;
        if (world->GetStaffHQ()->IsStriking(static_cast<StaffType>(1))) return 0x13;
        if (object->GetAssignedStaffMember().mIndex == gUIMainId7862) return 0x12;
        return object->IsBeingWorkedUpon() ? 0x15 : 0x14;
    }
    if (object->IsBrokenDown()) return object->IsBeingWorkedUpon() ? 0x15 : 2;
    if ((object->mFlags & 8) && !object->IsBOQConnected()) return object->IsClosed() ? 0x17 : 0x16;
    if (ride->mUIType == 0) {
        if (object->UpgradesWhat().mIndex == gUIMainId7864 && ride->Value(kRideSlot_UpgradeType).mInteger != 2 &&
            !object->IsExitCellConnected())
            return 0x18;
    }
    if (flag == 0) return 1;
    if (flag > 1 && object->IsClosed()) return 1;
    return 0;
}

// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.

// 0x10180fc0: the colour of an object state (no bounds check; the Mac answers through a hidden pointer).
uint32_t UIMain_GetObjectStateTextColour(int32_t state) { return gUIObjectStateColours[state]; }

// Unit v51042: UVersion's destructor, the object-state text, the rank text object's Change, the lender list callbacks, the thought and
// name edit handlers (main app, the neighbours of ui_main_panel_UI_Congratulations.cpp).

// ---- data (the image's initialised data)
// The state's text colour, 0xRRGGBBAA (SetObjectStateText uses the top three bytes).
uint32_t gUIObjectStateColours[27] = {   // 0x102a7780
    0xffffffffu, 0x808080ffu, 0xff321effu, 0xff321effu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu,
    0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu,
    0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0x5064ffffu, 0xff961effu, 0xff961effu, 0xff961effu, 0xff961effu, 0xff961effu,
};
// The state's string id in the text table of resource 6 (GetConstString's index, not a pointer).
uint32_t gUIObjectStateStrings[27] = {   // 0x102a77ec
    0x000, 0x16d, 0x16e, 0x16f, 0x170, 0x174, 0x172, 0x173, 0x171,
    0x175, 0x176, 0x17a, 0x178, 0x179, 0x177, 0x17b, 0x17c, 0x180,
    0x17e, 0x17f, 0x17d, 0x181, 0x184, 0x185, 0x186, 0x182, 0x183,
};
// ---- weak callees (the virtuals and the runtime's delete; the test library fakes them)
namespace v51042_callee {
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }
}  // namespace v51042_callee

// 0x10187710 (112 bytes)
SamsUtilities::UVersion* UVersion_Dtor(SamsUtilities::UVersion* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mComment.~UString();   // +0x18
        self->mText.~UString();      // +0x0c
        if (deleteFlag > 0) v51042_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10181018 (148 bytes)
void UIMain_SetObjectStateText(InterfaceWindow* window, int32_t state) {
    uint32_t colour = gUIObjectStateColours[state];
    window->SetFontColour(static_cast<int32_t>(colour >> 24), static_cast<int32_t>((colour >> 16) & 0xff),
                          static_cast<int32_t>((colour >> 8) & 0xff), 0xff);
    const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[6]);   // 0x564
    const unsigned char* text = table->GetConstString(gUIObjectStateStrings[state]);
    v51042_callee::WindowSetText(window, UIMain_ConvertMulti(text));
}

// Unit v5L16 (the tail): two leaves of the main app (SimTheme Park 2.04b3). UIMain_GetObjectName (0x101810fc, 60 bytes, Mac file
// ui_main.cpp) and CalcZTrans (0x101879d0, 52 bytes, the draw objects' depth, Mac file ui_DrawObject.cpp).

// The object's interface name in the shared conversion buffer (at most 0x3fff characters); the answer is the buffer.
char16_t* UIMain_GetObjectName(CObject* object) {
    object->GetUIName(gUIMainConvertBuffer, 0x3fff);
    return gUIMainConvertBuffer;
}

// ui_main.cpp (main app), unit w49: UIMain_DecideKbCapture 0x1018116c (612 bytes) decides which window has the keyboard after a dialog,
// a panel or the game window has closed. With MainMenuUp 0x101fc1ec (80 bytes; the Mac file ui_mainmenu.cpp) and UIMenu::IsShown
// 0x1018b298 (8 bytes; ui_utils.cpp) and the windows' globals, which the Mac keeps in the data of those files.

InterfaceWindow* gGameWindow;
InterfaceWindow* gFirstPersonWindow;
InterfaceWindow* gBumholeWindow;
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gBankruptWindow;
InterfaceWindow* gUIPanelWindow;
UIMenu* gMainMenu;

int32_t MainMenuUp();

void UIMain_DecideKbCapture() {
    if (MainMenuUp() != 0) {
        gMainMenu->mWindow->CaptureKb(0);
        return;
    }
    if (gGameWindow == nullptr) {
        if (gFirstPersonWindow != nullptr && (gFirstPersonWindow->GetAttr() & 1) != 0) {
            gKeyTableCamera->Enable();
            gFirstPersonWindow->CaptureKb(0);
            return;
        }
        if (gOnlinePanel != nullptr && (gOnlinePanel->GetAttr() & 1) != 0) {
            gOnlinePanel->CaptureKb(0);
            return;
        }
        if (gOnlineWindow != nullptr && (gOnlineWindow->GetAttr() & 1) != 0) {
            gOnlineWindow->CaptureKb(0);
            return;
        }
        if (gLobby != nullptr && gLobby->mWindow != nullptr) gLobby->mWindow->CaptureKb(0);
        return;
    }
    if (gBankruptWindow != nullptr) {
        gBankruptWindow->CaptureKb(0);
        return;
    }
    // The original does not test the first-person window for NULL here (it reads the attributes at address 0x44 then).
    if ((gFirstPersonWindow->GetAttr() & 1) != 0) {
        gKeyTableCamera->Enable();
        gFirstPersonWindow->CaptureKb(0);
        return;
    }
    if ((gGameWindow->GetAttr() & 1) != 0) {
        if (gUIPanelWindow != nullptr) {
            gUIPanelWindow->CaptureKb(0);
        } else if (CoasterEditPanelUp() == 0) {
            gKeyTableCamera->Enable();
            gKeyTableGame->Enable();
            gKeyTableCheat->Enable();
            gGameWindow->CaptureKb(0);
        } else {
            gKeyTableCamera->Enable();
            CoasterEditPanelGetKb();
        }
        return;
    }
    if (gBumholeWindow != nullptr && (gBumholeWindow->GetAttr() & 1) != 0) {
        gKeyTableCamera->Enable();
        gBumholeWindow->CaptureKb(0);
    }
}

// Unit v5013: the ride and sprite previews, MAKECOLUMNBUTTON and UIMain_ZoomToThing (main 0x10181400, 0x10181648, 0x101818e4, 0x10181b00).

sRidePreviewInfo* gRidePreview = nullptr;   // 0x10380c44
CPeep* gSpritePreviewPeep = nullptr;        // 0x10380c3c
int32_t gSpritePreviewHandle = 0;           // 0x10380c40
uint16_t gZoomSkipThing = 0;                // 0x102a7866

// 0x1024d8b2: a six byte draw object name, and at +6 the assert text (0x1024d8b8)
static const char kStringPool[] = "!cbut\0Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
static const char* const kColumnButtonDrawObject = kStringPool;
static const char* const kColumnButtonAssert = kStringPool + 6;

// 0x10181400
void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) {
    float bottom = static_cast<float>(0x300 - static_cast<int32_t>(rect.mBottom)) / 768.0f;
    float top = static_cast<float>(0x300 - static_cast<int32_t>(rect.mTop)) / 768.0f;
    float right = static_cast<float>(static_cast<int32_t>(rect.mRight) - 0x400) * 0.0009765625f;
    float left = static_cast<float>(static_cast<int32_t>(rect.mLeft) - 0x400) * 0.0009765625f;
    float depth = CalcZTrans(z);
    if (gRidePreview != nullptr) DestroyRidePreview(gRidePreview);
    gRidePreview = CreateRidePreview(left, bottom, depth, right - left, top - bottom, id);
}

// ui_main.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: UIMain_TickRidePreview (0x10181570, 52 bytes) and
// UIMain_NotEnoughDough (0x10182160, 64 bytes).

// Ticks the ride preview the panel shows, if any.
void UIMain_TickRidePreview() {
    if (gRidePreview != nullptr) TickRidePreview(gRidePreview);
}

// Seven small functions of ui_main.cpp (unit v5723): UIMain_ZoomToMapId 0x10181d34, UIMain_ZoomToPeep 0x10181c70,
// UIMain_GetShiftKeyState 0x10182088, UIMain_DestroySpritePreview 0x10181864, UIMain_UpdateSpritePreview 0x101817e4,
// UIMain_MidWindowParticles 0x10182404, UIMain_DestroyRidePreview 0x101815d4.

// 0x101815d4
void UIMain_DestroyRidePreview() {
    if (gRidePreview != nullptr) {
        DestroyRidePreview(gRidePreview);
        gRidePreview = nullptr;
    }
}

// 0x10181648
void UIMain_ShowSpritePreview(CoolRect& rect, CPeep* peep, int32_t z, float scale) {
    if (gSpritePreviewPeep != nullptr) UIMain_DestroySpritePreview();
    gSpritePreviewPeep = peep;
    int32_t script = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(peep) + 0xc);   // the peep's script handle
    if (script == 0) {
        gSpritePreviewPeep = nullptr;
        return;
    }
    float scaled = scale * 0.25f;
    int32_t offset;   // fctiwz
    if (scaled != scaled) offset = INT32_MIN;
    else if (scaled >= 2147483648.0f) offset = INT32_MAX;
    else if (scaled < -2147483648.0f) offset = INT32_MIN;
    else offset = static_cast<int32_t>(scaled);
    int32_t left = rect.mLeft, top = rect.mTop, right = rect.mRight, bottom = rect.mBottom;
    int32_t halfWidth = (right - left) >> 1;
    int32_t quarterHeight = (bottom - top) >> 2;
    int32_t centre = left + halfWidth;
    int32_t y = 0x300 - ((bottom - quarterHeight) - offset);
    float x = static_cast<float>(centre - 0x3f8) * 0.0009765625f;
    float yy = static_cast<float>(y) / 768.0f;
    float depth = CalcZTrans(z);
    gSpritePreviewHandle = ESP_Spawn2DCopyOfScriptAt(script, x, yy, depth, scale);
}

// 0x101817e4: the peep's word at +0xc is the source script.
void UIMain_UpdateSpritePreview() {
    if (gSpritePreviewPeep != nullptr) {
        int32_t source = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(gSpritePreviewPeep) + 0xc);
        if (source != 0) ESP_Update2DScript(gSpritePreviewHandle, source);
    }
}

// 0x10181864
void UIMain_DestroySpritePreview() {
    if (gSpritePreviewPeep != nullptr) {
        SCR_KillScript(gSpritePreviewHandle);
        gSpritePreviewPeep = nullptr;
    }
}

// 0x101818e4
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t font, int32_t red, int32_t green, int32_t blue,
                      int32_t helpText) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>(kColumnButtonDrawObject)));
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    InterfaceFont* chosen = nullptr;
    if (static_cast<uint32_t>(font) < 13) chosen = &gText.mFonts[gText.mResolutionGroup * 13 + font];
    window->SetFont(chosen);
    window->SetFontColour(red, green, blue, 0xff);
    UI_RGBA colour;
    colour.mR = static_cast<uint8_t>(red);
    colour.mG = static_cast<uint8_t>(green);
    colour.mB = static_cast<uint8_t>(blue);
    colour.mA = 0xff;
    InterfaceButton* button = static_cast<InterfaceButton*>(window);
    button->SetTextUpColour(colour);
    button->SetTextDownColour(colour);
    window->SetClientDrawText(text);
    window->mHelpText = helpText;
}

// The interface's text drawing (main app of SimTheme Park 2.04b3): UI_DrawText 0x1018b578, MakeRenderArea 0x1018b360 and
// AdjustJustificationForHebrew 0x1018b2c8 (ui_utils.cpp), UIMain_ConvertString 0x10181a7c (ui_main.cpp) and the three
// InterfaceFont width members 0x1016915c, 0x10169264, 0x101692d4 (the inventory files them under FS_MasterSave.cpp).
// UI_DrawText hands the text to the ltms text formatter (unit w35a), which draws through the Bullfrog library's current
// area, viewport and clip rectangle (src/bfl_lbdraw_leaves.cpp). The declarations are in include/UI_Base.h and
// include/InterfaceFont.h.

// ---- ui_main.cpp

// The original has one UIMain_ConvertString, taking a char16_t* (src/ui_main.cpp, 0x10181a7c). The const overload is only this
// translation's way for callers holding a const text to reach it.
uint8_t* UIMain_ConvertString(const char16_t* text) { return UIMain_ConvertString(const_cast<char16_t*>(text)); }

uint8_t gUIMainMBBuffer[0x4000] = {};   // 0x10388c48 (name invented; the multibyte mirror image of the buffer above)

// 0x10181a7c, the one function of that name (it takes a char16_t*; the const overload in UI_drawtext.cpp forwards to it). The text in the
// language's code page, in gUIMainMBBuffer: converted only when the Unicode to multibyte converter (resource 1) is loaded, otherwise the
// buffer keeps whatever the last conversion left in it. Answers the buffer either way (a NULL text goes to the converter as it is).
uint8_t* UIMain_ConvertString(char16_t* text) {
    if (gText.mResourcePointers[1] != nullptr) {
        static_cast<TbIUnicodeToMB*>(gText.mResourcePointers[1])->ConvertString(gUIMainMBBuffer, text, 0x3fff);
    }
    return gUIMainMBBuffer;
}

// 0x10181b00
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    uint16_t index = id.mIndex;
    if (index != gZoomSkipThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kColumnButtonAssert);
        char* thing = static_cast<char*>(gThingArray[index].mThing);
        if (thing != nullptr) {
            uint16_t v = static_cast<uint16_t>(static_cast<uint8_t>(thing[4]) + static_cast<uint8_t>(thing[6]) * 0x80 + 1);
            float x = static_cast<float>(static_cast<int32_t>(((v - 1) & 0x7f) * 10));
            float z = static_cast<float>(static_cast<int32_t>(((static_cast<uint32_t>(v) - 1) >> 7) * 10));
            SetCameraPOI(&x, &z, nullptr, nullptr, nullptr);
        }
    }
}

// 0x10181c70
void UIMain_ZoomToPeep(CPeep* peep) {
    if (peep == nullptr) return;
    float t = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(gTickNow) - static_cast<uint32_t>(gLastTurnTime))) / 248.0f;
    float x, y, z;
    uint32_t stamp;
    peep->GetInterpolatedPeepPosition(t, 1, x, y, z, stamp);
    SetCameraPOI(&x, &z, nullptr, nullptr, nullptr);
}

// 0x10181d34: the map cell (index - 1) as column (low 7 bits) and row, ten units per cell.
void UIMain_ZoomToMapId(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>& id) {
    uint16_t v = id.mIndex;
    uint32_t m = static_cast<uint32_t>(v) - 1u;
    float x = static_cast<float>(static_cast<int32_t>((m & 0x7fu) * 10u));
    float y = static_cast<float>(static_cast<int32_t>((m >> 7) * 10u));
    SetCameraPOI(&x, &y, nullptr, nullptr, nullptr);
}

// UIMain_GotoStatusPanelForThing (unit v5071): shows the status panel that fits a thing.

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
}

uint16_t gNoUpgradeParent = 0;   // 0x102a7868
uint16_t gNoUpgradeTarget = 0;   // 0x102a786a

// 0x10181e2c
void UIMain_GotoStatusPanelForThing(void* thingPtr) {
    if (thingPtr == nullptr) return;
    CThing* thing = static_cast<CThing*>(thingPtr);
    const uint8_t* bytes = static_cast<const uint8_t*>(thingPtr);
    uint8_t kind = bytes[2];
    if (kind == 3) {
        CObject* object = static_cast<CObject*>(thingPtr);
        // The record's UI type (+0x7a8) and UpgradeType slot (+0x288) by name: the host's record is laid out wider (unit v5L31).
        const RideInfo* info = object->GetDBInfo();
        int32_t type = info->mUIType;
        switch (type) {
            case 0:
                if (info->Value(kRideSlot_UpgradeType).mInteger == 2) {
                    gUpgradeStatusPanel.Show(thing);
                } else {
                    uint16_t parent = object->GetUpgradeParent().mIndex;
                    if (parent == gNoUpgradeParent) {
                        if (object->UpgradesWhat().mIndex == gNoUpgradeTarget) gRideStatusPanel.Show(thing);
                    } else {
                        uint16_t again = object->GetUpgradeParent().mIndex;
                        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                        gRideStatusPanel.Show(static_cast<CThing*>(gThingArray[again].mThing));
                    }
                }
                break;
            case 1: gShopStatusPanel.Show(thing); break;
            case 2: gShowStatusPanel.Show(thing); break;
            case 3: {
                uint16_t flags = *reinterpret_cast<const uint16_t*>(bytes + 0x2e);
                if (flags & 1) gBogStatusPanel.Show(thing);
                else if (flags & 2) gStaffRoomStatusPanel.Show(thing);
                else gFeatureStatusPanel.Show(thing);
                break;
            }
            default: break;
        }
    } else if (kind == 5 || kind == 4 || kind == 6 || kind == 7 || kind == 8) {
        gStaffStatusPanel.Show(thing);
    } else if (kind == 1) {
        gVisitorStatusPanel.Show(thing);
    }
}

// 0x10182088: modifier bits to the 2-bit fields: 0x200 -> 3, 0x1100 -> 0xc, 0x800 -> 0x30.
uint32_t UIMain_GetShiftKeyState() {
    uint32_t result = 0;
    if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new SamsUtilities::UKeyboard;
    uint32_t bits = SamsUtilities::gKeyboard->GetModifiers();
    if (bits & 0x200) result |= 3;
    if (bits & 0x1100) result |= 0xc;
    if (bits & 0x800) result |= 0x30;
    return result;
}

// The "not enough money" sound: event 0x1d of the ui category.
void UIMain_NotEnoughDough() { SOUND_SendEvent(0, gSoundBanks[3], 0x1d, 0, 0, 0); }

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

// The dialog box's message functions (main app, SimTheme Park 2.04b3; the inventory files them under ui_ErrorDisplay.cpp), and the two
// small functions they call that undo UI_DialogBox's pause and key table switch-off. Unit w47: DialogBoxCallback 0x101f1520 (676 bytes),
// DefaultDialogCallback 0x101f1448 (152), UIMain_EnableAllKeyTables 0x10182304 (48) and UI_UnPauseTimerSystem 0x101d8ba0 (96; the
// inventory files it under ui_cityLobbyController.cpp).
//
// UI_DialogBox (src/ui_dialogbox.cpp) keeps in the dialog's extra parameters 0 to 2 the buttons' callbacks (NULL for none), 3 their
// argument, 4 the key table bookkeeping byte it found (-1: it left the tables alone) and 5 whether it paused the game.

// 0x10182304: the opposite of UIMain_DisableAllKeyTables (src/ui_dialogbox.cpp), which sets the byte to 1.
void UIMain_EnableAllKeyTables() {
    EnableAllKeytables();
    gUIMainKeyTablesDisabled = 0;
}

// ui_ErrorDisplay.cpp (main app), unit w43: UI_DialogBox 0x101f1800, 1008 bytes.

uint8_t gUIMainKeyTablesDisabled;   // 0x1038cc54

// 0x10182364, 48 bytes: first disable the six tables, then set the bookkeeping byte.
void UIMain_DisableAllKeyTables() {
    DisableAllKeytables();
    gUIMainKeyTablesDisabled = 1;
}
// 0x101823c8, 12 bytes: the bookkeeping byte, independently of the tables' enabled words.
uint8_t UIMain_IsKeyTableDisabled() { return gUIMainKeyTablesDisabled; }

// 0x10182404
void UIMain_MidWindowParticles(int32_t id, InterfaceWindow* window) {
    CoolRect rect;
    if (window->GetVisibility() != 0 && gGame.mF1C == 0) {
        window->GetPosition(&rect, 1);
        int32_t cx = rect.mLeft + ((rect.mRight - rect.mLeft) >> 1);
        int32_t cy = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
        int32_t px = static_cast<int32_t>(static_cast<uint32_t>(cx) * 100000u) / 2048;
        int32_t py = static_cast<int32_t>(static_cast<uint32_t>(cy) * 75000u) / 0x600;
        PAR_InitEmitter(id, px, 0, py);
    }
}

// ui_main.cpp (main app), unit w142: ContextMouse (0x10182548, 3,508 bytes), the park pointer. Every frame the game window asks it what
// the pointer is over: it picks the help text and the cursor from the mouse mode, the thing under the pointer (gIntrface's staff or
// guest), the shift keys and the map cell (gIntrface.mF24, the 1 based cell id), and highlights the ride the pointer is on.
//
// Words the original keeps (names invented, ui_main.h): gGameWinWordC38 is the KIND of the last help text shown (0 none, 2 grass, 1 and 3
// the cell kinds, 4 an upgradable, 5 a ride, 6 a cell type 0x15, 7 a staff member, 8 to 0xc a guest or such by type, 0xd the ticket booth,
// 0xe a park entrance); gGameWinWordC34 is the thing (or 0) the text is about. A text is only built again when the kind or the thing
// changed since the last call (`old`, the kind at entry; the kind is cleared to 0 at the start of every call). Where a path ends with
// "finish" below, the Mac ends the function without asking for a cursor.
//
// The things are reached at the Mac's offsets (kind byte +2, a staff member's word +0x1d8, a guest's +0x28), as src/rsse_ride_tick.cpp does.

namespace {
char16_t gEmptyHelp[1] = {0};   // 0x1024da3e

uint8_t ByteAt(const void* base, std::size_t offset) {
    return static_cast<const uint8_t*>(base)[offset];
}
uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}
// The Mac word for a thing's address (gGameWinWordC34 is a 32 bit word).
int32_t PtrWord(const void* p) {
    return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p));
}

// The thing of a map cell's thing id (0 when there is none; the null id of the Mac reads 0). The asserts are the Mac's.
void* ThingOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return gThingArray[id].mThing;
}

CMapCell* MapCellOf(uint32_t id) {   // the 1 based cell id
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + static_cast<uint32_t>((id - 1) * 0x44u));
}
CTrackCell* TrackCellOf(uint32_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + 0x110000 + static_cast<uint32_t>((id - 1) * 0x28u));
}

TbITable* HelpTable() {
    return static_cast<TbITable*>(gText.mResourcePointers[7]);
}

void ShowHelp() {
    SetHelpTextString(gUIMainConvertBuffer);
}
void ShowConst(uint32_t id) {
    SetHelpTextString(UIMain_ConvertMulti(const_cast<unsigned char*>(HelpTable()->GetConstString(id))));
}
}  // namespace

void ContextMouse(int32_t context) {
    // The shift keys: the keyboard object is made on first use (0xc bytes on the Mac; NULL when it could not be made).
    uint32_t keys = 0;
    if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new (std::nothrow) SamsUtilities::UKeyboard;
    const uint32_t bits = SamsUtilities::gKeyboard->GetModifiers();
    if ((bits & 0x200) != 0) keys |= 3;
    if ((bits & 0x1100) != 0) keys |= 0xc;
    if ((bits & 0x800) != 0) keys |= 0x30;

    const int32_t old = gGameWinWordC38;
    const int32_t staff = StaffPanelUp();
    gGameWinWordC38 = 0;

    int32_t mode;
    if (context != 0) {
        mode = 1;
    } else {
        if (gMouse == nullptr) gMouse = MakeMouse();
        mode = (gMouseMode != nullptr) ? gMouseMode->GetMode() : 0;
    }
    const int32_t cellMode = LLCC_SystemGetMode();

    char16_t* const buffer = gUIMainConvertBuffer;
    TbITable* const table = HelpTable();
    const uint16_t id = gIntrface.mF24;

    if (mode == 8 || (mode == 3 && static_cast<uint32_t>(cellMode - 0x15) <= 1u)) {
        // The ride under the pointer in the ride, or ride deleting, modes.
        if (gRideHighlightKeep != 0) {
            HighlightRide();
            return;
        }
        if (id == 0) return;
        const CMapCell* cell = MapCellOf(id);
        if (static_cast<int32_t>(cell->mType) != 0x15) return;
        const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(cell->mParentID)->GetObjectThingId();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        if (gThingArray[thing.mIndex].mThing == UI_GetSelectedObject()) HighlightRide();
        return;
    }

    if (mode == 9) {
        if (id == 0) return;
        uint32_t cellId = static_cast<uint32_t>(id) - 1;
        uint16_t found;
        LLCR_OverRide(&found, static_cast<int32_t>(cellId & 0x7f), static_cast<int32_t>(cellId >> 7));
        if (found == 0) {
            if (gGameWinWordC34 == 0 && old == gGameWinWordC38) return;
            gGameWinWordC34 = 0;
            table->FormatString(buffer, 0x3fff, 0x243);
            ShowHelp();
            return;
        }
        void* const obj = ThingOf(found);
        if (ByteAt(obj, 2) != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(obj);
        if (object->UpgradesWhat().mIndex != 0) return;
        if (object->GetDBInfo()->mUIType != 0) return;
        if (gGameWinWordC34 != PtrWord(obj) || old != gGameWinWordC38) {
            gGameWinWordC34 = PtrWord(obj);
            object->GetUIName(buffer, 0x3fff);
            TbUniStringVTag tag(2, buffer);
            table->FormatString(buffer, 0x3fff, 0x244, &tag);
            ShowHelp();
        }
        HighlightRide();
        return;
    }

    if (static_cast<uint32_t>(mode) > 1u) {
        // Any other mode: no help text, no cursor.
        if (old != gGameWinWordC38) SetHelpTextString(gEmptyHelp);
        gGameWinWordC34 = 0;
        return;
    }

    // Mode 0 or 1: the thing under the pointer, then the cell.
    void* const pointed = gIntrface.mPointedStaffOrGuest;
    int16_t cursor = 0;
    bool done = false;
    const bool staffCase = (staff == 2 && keys == 0) || (keys == 3 && staff == 0);

    if (staffCase) {
        if (pointed != nullptr && ByteAt(pointed, 2) == 1) {
            gGameWinWordC38 = 7;
            cursor = 2;
            if (!(old == 7 && gGameWinWordC34 == PtrWord(pointed))) {
                const uint32_t word = WordAt(pointed, 0x1d8);
                if (word == 0) {
                    table->FormatString(buffer, 0x3fff, 0x22a);
                } else {
                    TbIntegerVTag tag(3, static_cast<int32_t>(word));
                    table->FormatString(buffer, 0x3fff, 0x229, &tag);
                }
                ShowHelp();
            }
            gGameWinWordC34 = PtrWord(pointed);
            UI_SetGameWindowCursor(2);
            return;
        }
        if (old != gGameWinWordC38) SetHelpTextString(gEmptyHelp);
        UI_SetGameWindowCursor(0);
        return;
    }

    if (pointed != nullptr) {
        const uint8_t kind = ByteAt(pointed, 2);
        int32_t shown = 0;
        switch (kind) {
            case 4: shown = 9; break;
            case 5: shown = 8; break;
            case 6: shown = 0xa; break;
            case 7: shown = 0xb; break;
            case 8: shown = 0xc; break;
            default: break;
        }
        if (shown != 0) {
            cursor = 2;
            done = true;
            gGameWinWordC38 = shown;
            if (!(old == gGameWinWordC38 && gGameWinWordC34 == PtrWord(pointed))) {
                CStaff* const person = reinterpret_cast<CStaff*>(pointed);
                const char16_t* name = person->GetName();
                TbUniStringVTag tag(2, name);
                table->FormatString(buffer, 0x3fff, 0x22b, &tag);
                ShowHelp();
                person->BeingPointedAt();
            }
            gGameWinWordC34 = PtrWord(pointed);
        }
    }
    if (staff != 0) done = true;
    if (done || id == 0) {
        UI_SetGameWindowCursor(cursor);
        return;
    }

    // The park entrances and the ticket booths.
    const CMap* const map = gGameWorld->GetMap();
    const uint32_t e0 = map->GetParkEntrancePos(0).mIndex;
    const uint32_t e1 = map->GetParkEntrancePos(1).mIndex;
    const uint32_t b0 = map->GetTicketBoothPos(0).mIndex;
    const uint32_t b1 = map->GetTicketBoothPos(1).mIndex;
    if (id == e0 || id == e1 || id == ((e0 - 1) & 0xffff) || id == ((e1 + 1) & 0xffff)) {
        gGameWinWordC38 = 0xe;
        if (old != 0xe) ShowConst(0x1cb);
        gGameWinWordC34 = 0;
        HighlightRide();
        UI_SetGameWindowCursor(2);
        return;
    }
    if (id == b0 || id == b1) {
        gGameWinWordC38 = 0xd;
        if (old != 0xd) ShowConst(0x1cc);
        gGameWinWordC34 = 0;
        UI_SetGameWindowCursor(2);
        return;
    }

    // The cell.
    CMapCell* const cell = MapCellOf(id);
    CTrackCell* track = TrackCellOf(id);
    const uint32_t cellId = static_cast<uint32_t>(id) - 1;
    uint16_t found;
    LLCR_OverRide(&found, static_cast<int32_t>(cellId & 0x7f), static_cast<int32_t>(cellId >> 7));
    void* thing = nullptr;   // r27
    if (found != 0) thing = ThingOf(found);

    enum { kCursorCall, kF40, kFA8, kG090, kF7C } next = kG090;
    const uint32_t trackType = static_cast<uint32_t>(track->mType);
    if (trackType == 0xb || trackType == 0x10) {
        next = kF40;
    } else if (trackType == 0xc) {
        CTrackCell* const parent = TrackCellOf(track->mParentID);
        if (static_cast<uint32_t>(parent->mType) == 0x19) {
            done = true;
            next = kG090;
        } else {
            track = parent;
            next = kF40;
        }
    }
    if (next == kF40) next = (cell->IsRide() != 0 || cell->IsExit() != 0 || cell->IsEntrance() != 0) ? kF7C : kFA8;
    if (next == kF7C) {
        if (thing == nullptr) return;
        next = (reinterpret_cast<CObject*>(thing)->UpgradesWhat().mIndex == 0) ? kG090 : kFA8;
    }
    if (next == kFA8) {
        const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> owner = MapCellOf(track->mParentID)->GetObjectThingId();
        if (owner.mIndex == 0) {
            gGameWinWordC34 = 0;
            return;
        }
        void* const ride = ThingOf(owner.mIndex);
        cursor = (WordAt(ride, 0x28) != 0) ? 0xa : 6;
        gGameWinWordC38 = 5;
        if (old != 5) ShowConst(0x1be);
        gGameWinWordC34 = 0;
        done = true;
        HighlightRide();
    }
    // G090
    if (!done && cell->IsGrass() != 0) {
        gGameWinWordC38 = 2;
        done = true;
        cursor = 3;
        if (old != 2) ShowConst(0x1b9);
        gGameWinWordC34 = 0;
    }
    if (done) {
        UI_SetGameWindowCursor(cursor);
        return;
    }

    // The cell's type: 1 and 3 name the cell, 4, 9 and 10 an upgradable ride, 21 a ride.
    const uint32_t cellType = static_cast<uint32_t>(cell->mType);
    int32_t kind = 0;
    uint32_t textId = 0;
    switch (cellType) {
        case 1: kind = 1; cursor = 3; textId = 0x1ba; break;
        case 3: kind = 3; cursor = 4; textId = 0x1bc; break;
        case 4:
        case 9:
        case 10: kind = -1; break;
        case 0x15: kind = 6; cursor = 0xb; textId = 0x1c0; break;
        default: break;
    }
    if (kind == -1) {
        if (thing != nullptr) {
            CObject* const object = reinterpret_cast<CObject*>(thing);
            if (object->UpgradesWhat().mIndex == 0 || object->GetDBInfo()->Value(kRideSlot_UpgradeType).mInteger == 2) {
                gGameWinWordC38 = 4;
                if (PtrWord(thing) != gGameWinWordC34) {
                    gGameWinWordC34 = PtrWord(thing);
                    object->GetUIName(buffer, 0x3fff);
                    TbUniStringVTag tag(2, buffer);
                    table->FormatString(buffer, 0x3fff, 0x226, &tag);
                    ShowHelp();
                }
                HighlightRide();
                cursor = 2;
            }
        }
        UI_SetGameWindowCursor(cursor);
        return;
    }
    if (kind == 0) {
        if (old != gGameWinWordC38) SetHelpTextString(gEmptyHelp);
        gGameWinWordC34 = 0;
        UI_SetGameWindowCursor(0);
        return;
    }
    gGameWinWordC38 = kind;
    if (old != kind) ShowConst(textId);
    gGameWinWordC34 = 0;
    if (kind == 6) HighlightRide();
    UI_SetGameWindowCursor(cursor);
}

// ui_main.cpp (main app), unit w143: ContextMouseClick (0x10183320, 2,364 bytes), the click in the park. gGameWinWordC38 is the KIND of the help
// text the last ContextMouse call showed (src/ui_main_context.cpp): the click acts on what the pointer is over. Answers 0 (the click is used up:
// a ride is placed or the coaster editor opens) or 1; nothing happens in a menu or without a world.
//   kind 1, 2   a path is drawn (CMMPathDraw mode 1), when the pointer is over a cell
//   kind 3      a cell of type 3: its object is selected, the path mode 0x14
//   kind 4      an upgradable: with the command key held and the ride affordable the path mode 4 for its object type (answers 0), else its panel
//   kind 5      a track cell (types 0xb, 0x10, 0x19): the path mode 0xe, 0x13 or 0x1a
//   kind 6      the coaster editor for the object of the cell (answers 0)
//   kind 7      the guest panel, 8 to 0xc the staff panel, 0xd and 0xe the entry price panel
// The null ids are halfwords of the image (0x102a76d8, 0x102a78d0, 0x102a78d2, 0x102a78d4, 0x102a78d6, all 0 there); the names are invented.

extern const uint16_t gClickNoCell = 0;       // 0x102a76d8: the cell id of "no cell"
extern const uint16_t gClickNoResult = 0;     // 0x102a78d0: what LLCR_OverRide answers for a column without a cell
extern const uint16_t gClickNoThing3 = 0;     // 0x102a78d2: the thing id of "no thing" (kind 3)
extern const uint16_t gClickNoThing6 = 0;     // 0x102a78d4: the same (kind 6)
extern const uint16_t gClickNoThing5 = 0;     // 0x102a78d6: the same (kind 5)

namespace {
// The new mode goes to the mouse (made first if there is none; the Mac calls the assignment even for a missing mouse).
void GoToMode(CMouseMode* mode) {
    if (gMouse == nullptr) gMouse = MakeMouse();
    gMouse->AssignMouseMode(mode);
}

// A path drawing mode of the cell mode, handed to the mouse.
void PathDraw(int32_t cellMode) {
    CMMPathDraw* mode = new (std::nothrow) CMMPathDraw(static_cast<CELL_MODES>(cellMode));
    GoToMode(mode);
}
}  // namespace

int32_t ContextMouseClick() {
    if (gGameWorld == nullptr) return 0;
    if (MainMenuUp() != 0) return 0;
    if (gMouse == nullptr) gMouse = MakeMouse();
    int32_t current = 0;
    if (gMouseMode != nullptr) current = gMouseMode->GetMode();
    if (static_cast<uint32_t>(current) > 1) return 1;
    UI_CheckSelected();
    const uint32_t kind = static_cast<uint32_t>(gGameWinWordC38);
    if (kind > 0xe) return 1;
    switch (kind) {
        case 1:
        case 2: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            PathDraw(1);
            return 1;
        }
        case 3: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            const CMapCell* cell = MapCellOf(id);
            if (static_cast<int32_t>(cell->mType) != 3) return 1;
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(cell->mParentID)->GetObjectThingId();
            if (thing.mIndex == gClickNoThing3) return 1;
            UI_SetSelectedObject(ThingOf(thing.mIndex));
            LLCM_SetCurrentObject(true);
            PathDraw(0x14);
            return 1;
        }
        case 4: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            uint16_t found = 0;
            const uint32_t cellIndex = static_cast<uint32_t>(id) - 1;
            LLCR_OverRide(&found, static_cast<int32_t>(cellIndex & 0x7f), static_cast<int32_t>(cellIndex >> 7));
            if (found == gClickNoResult) return 1;
            if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new (std::nothrow) SamsUtilities::UKeyboard;
            const uint8_t commandDown = SamsUtilities::gKeyboard->IsCommandDown();
            if (commandDown != 0) {
                const uint8_t* const object = static_cast<const uint8_t*>(ThingOf(found));
                uint16_t objectType;
                std::memcpy(&objectType, object + 0xe, sizeof(objectType));
                int32_t rotation;
                std::memcpy(&rotation, object + 0x10, sizeof(rotation));
                const ObjectControl* const control =
                    gGameWorld->GetControlManager()->GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{objectType}, 0);
                int32_t price;
                price = control->mUpgradeCost;
                const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                if (price > balance) {
                    SOUND_SendEvent(0, gSoundBanks[3], 0x1d, 0, 0, 0);
                } else {
                    CMMPathDraw* mode = new (std::nothrow) CMMPathDraw(static_cast<CELL_MODES>(4), tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{objectType});
                    GoToMode(mode);
                    LLCC_SetRideRotation(rotation, true);
                }
                return 0;
            }
            UIMain_GotoStatusPanelForThing(UI_GetSelectedObject());
            return 1;
        }
        case 5: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            const CTrackCell* track = TrackCellOf(id);
            if (static_cast<int32_t>(track->mType) == 0xc) track = TrackCellOf(track->mParentID);
            const int32_t type = static_cast<int32_t>(track->mType);
            int32_t pathMode;
            if (type == 0x10) {
                pathMode = 0x13;
            } else if (type == 0xb) {
                pathMode = 0xe;
            } else if (type == 0x19) {
                LLCM_SetCurrentObject(true);
                PathDraw(0x1a);
                return 1;
            } else {
                return 1;
            }
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(track->mParentID)->GetObjectThingId();
            if (thing.mIndex == gClickNoThing5) return 1;
            UI_SetSelectedObject(ThingOf(thing.mIndex));
            LLCM_SetCurrentObject(true);
            PathDraw(pathMode);
            return 1;
        }
        case 6: {
            const uint16_t id = gIntrface.mF24;
            if (id == gClickNoCell) return 1;
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(MapCellOf(id)->mParentID)->GetObjectThingId();
            if (thing.mIndex == gClickNoThing6) return 1;
            void* object = ThingOf(thing.mIndex);
            UI_SetSelectedObject(object);
            CoasterEditMode(object);
            return 0;
        }
        case 7: {
            CThing* peep = UI_GetSelectedPeep();
            if (peep != nullptr) gVisitorStatusPanel.Show(peep);
            return 1;
        }
        case 8:
        case 9:
        case 10:
        case 11:
        case 12: {
            CThing* peep = UI_GetSelectedPeep();
            if (peep != nullptr) gStaffStatusPanel.Show(peep);
            return 1;
        }
        case 13:
        case 14:
            CreateEntryPricePanel();
            return 1;
        default:
            return 1;
    }
}

// ui_main.cpp (main app), unit w136: the message functions of the park's two big windows. Engine_GameWinMsg 0x10183c84 (2408 bytes) is
// gGameWindow's, the full-screen park window: it turns the pointer, button and key messages into mouse modes, camera keys and context menus.
// Engine_GameFirstPersonMsg 0x10184628 (720 bytes) is gFirstPersonWindow's: camera keys, leaving first person, the postcard key. Both answer
// what InterfaceWindow::CallDefaultWindowHandler answers.
//
// A position (the b of a button or key message) packs x in the HIGH half (arithmetic shift) and y in the low half (unsigned); x is scaled by
// the real screen's width / 2048 and y by its height / 0x600, both truncating toward zero.

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
constexpr int32_t kMsgKeyUp = 0x1000b;
constexpr int32_t kFirstPersonBits = 0x16;   // gFirstPersonFlags: walking (2), riding (4) and 0x10
constexpr int32_t kKeyEscape = 0x1b;
constexpr int32_t kRightPressLimit = 200;    // ticks a right press may last before it is a drag

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

// ui_main.cpp (main app of SimTheme Park 2.04b3), the part that starts the interface:
// the meshes of the interface (0x10184abc..0x10185640), InitGeneralUI (0x10185b68), the
// clock tick objects (0x10185b08, 0x10187994) and the file's global strings.
//
// Every .md2 file of the "UI" resource becomes a draw object in the library: the frames
// ("!frame", "!f_plain", "!cbut") and the slider ("!slider") under their file name, any
// other mesh under the name of its root node. The meshes of the language's folder follow.
// The first mesh loaded is what puts the "welcome" picture on the screen (CMesh::Init
// steps the loading picture).

using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

InterfaceTitleTextObject* gTitleText;
CursorTable gCursorTable;
TbDynamicStringTemplate<char> gUIFrameName("!frame");
TbDynamicStringTemplate<char> gUISliderName("!slider");
TbDynamicStringTemplate<char> gUIPlainFrameName("!f_plain");
LbTimeClockTickFunction gUIClockTick;

// A frame mesh ("!frame", "!f_plain" or "!cbut", whatever the case of the file's name): the
// object goes into the library under the file's name without ".md2". NULL for any other
// file, and when the mesh does not load. (`storage` is not used.)
InterfaceDrawEngineResizableFrame* TPResource_AddFrameMesh(CFileInfo info, CFileStorage&, const TbDynamicStringTemplate<char>& folder) {
    TbDynamicStringTemplate<char> name(info.mName);
    name = name.Left(static_cast<uint32_t>(name.Length() - gTPGlobals.mF42CC.Length()));
    if (!TPStrEq(name, gUIFrameName) && !TPStrEq(name, gUIPlainFrameName)) {
        TbDynamicStringTemplate<char> button("!cbut");
        const bool isButton = TPStrEq(name, button);
        if (!isButton) return nullptr;
    }
    InterfaceDrawEngineResizableFrame* frame = nullptr;
    TbDynamicStringTemplate<char> path;
    path = folder;
    path += info.mName;
    tag_sMesh* mesh = LoadMesh(path.mData, nullptr, 0, 1, 0);
    if (mesh != nullptr) {
        OverrideTexturePaths(mesh->mHeader, 0, 1);
        frame = new InterfaceDrawEngineResizableFrame(mesh);
        frame->mResizable = 1;
        DrawObjectLibrary::AddDrawObject(frame, name.mData);
    }
    return frame;
}

// The slider mesh ("!slider"), in the library under that name.
InterfaceDrawEngineResizableSlider* TPResource_AddSliderMesh(CFileInfo info, CFileStorage&, const TbDynamicStringTemplate<char>& folder) {
    TbDynamicStringTemplate<char> name(info.mName);
    TbDynamicStringTemplate<char> stem(name.Left(static_cast<uint32_t>(name.Length() - gTPGlobals.mF42CC.Length())));
    const bool isSlider = TPStrEq(stem, gUISliderName);
    if (!isSlider) return nullptr;
    InterfaceDrawEngineResizableSlider* slider = nullptr;
    TbDynamicStringTemplate<char> path;
    path = folder;
    path += info.mName;
    tag_sMesh* mesh = LoadMesh(path.mData, nullptr, 0, 1, 0);
    if (mesh != nullptr) {
        OverrideTexturePaths(mesh->mHeader, 0, 1);
        slider = new InterfaceDrawEngineResizableSlider(mesh);
        slider->mResizable = 1;
        static char libraryName[] = "!slider";
        DrawObjectLibrary::AddDrawObject(slider, libraryName);
    }
    return slider;
}

// Any other mesh, in the library under the name of its root node. Files whose name starts
// with '!' and "w_small_shadow.md2" (that exact spelling) are left out.
InterfaceDrawEngineMesh* TPResource_AddMeshDrawObject(CFileInfo info, CFileStorage&, TbDynamicStringTemplate<char> folder) {
    InterfaceDrawEngineMesh* object = nullptr;
    TbDynamicStringTemplate<char> path;
    if (*info.mName == '!') return nullptr;
    path = folder;
    path += info.mName;
    if (std::strcmp(info.mName, "w_small_shadow.md2") == 0) return nullptr;
    tag_sMesh* mesh = LoadMesh(path.mData, nullptr, 0, 1, 0);
    if (mesh != nullptr) {
        OverrideTexturePaths(mesh->mHeader, 0, 1);
        object = new InterfaceDrawEngineMesh(mesh);
        DrawObjectLibrary::AddDrawObject(object, mesh->mHeader->mF78->mName);
    }
    return object;
}

// The meshes of the "UI" resource; the result is how many became draw objects.
int32_t LoadAllUIMeshes() {
    static char file[] = "here2000.txt";
    static char folder[] = "data:ui";
    int32_t count = 0;
    CTPBaseResource* resource = nullptr;
    SetupDirectoryForMeshLoad(file, folder, nullptr);
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4600, &resource) == 1) {
        CFileInfo info;
        if (resource->FindFirst(gTPGlobals.mF4574, info) == 1) {
            do {
                CFileStorage* storage = resource->pGetCurrentSearchStorage();
                if (TPResource_AddFrameMesh(info, *storage, resource->mPath) != nullptr) ++count;
                else if (TPResource_AddSliderMesh(info, *storage, resource->mPath) != nullptr) ++count;
                else if (TPResource_AddMeshDrawObject(info, *storage, resource->mPath) != nullptr) ++count;
            } while (resource->FindNext(info) != 0);
        }
    }
    return count;
}

// The meshes in the language's folder of the "Langauge" resource, each marked as localised.
// The mesh paths are set up once, from the first file found.
int32_t LoadLocalisedUIMeshes() {
    static char textures[] = "data:ui:textures";
    CTPBaseResource* resource = nullptr;
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4678, &resource) == 1) {
        CFileInfo info;
        TbDynamicStringTemplate<char> pattern;
        TbDynamicStringTemplate<char> folder;
        bool first = true;
        pattern = gText.GetLanguageName();
        FS_AddDirectorySeparator(pattern);
        pattern += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4574);
        if (resource->FindFirst(pattern, info) == 1) {
            do {
                CFileStorage* storage = resource->pGetCurrentSearchStorage();
                if (first) {
                    folder = resource->mPath;
                    FS_AddDirectorySeparator(folder);
                    folder += gText.GetLanguageName();
                    SetupDirectoryForMeshLoad(info.mName, folder.mData, textures);
                    FS_AddDirectorySeparator(folder);
                    first = false;
                }
                InterfaceDrawEngineMesh* object =
                    TPResource_AddMeshDrawObject(info, *storage, TbDynamicStringTemplate<char>(folder.mData));
                // The original marks the object without looking whether there is one (a write
                // into low memory on the Mac when there is not).
                if (object != nullptr) object->mLocalised = 1;
            } while (resource->FindNext(info) != 0);
        }
    }
    return 0;
}

// The interface's colour cursors and the lobby's interface set-up (main app, ui_main.cpp range:
// LoadBFCurs 0x10185670, InitialiseCOLOURCursors 0x1018575c, InitLobbyUI 0x10186324; the
// InterfaceBFCursor constructor and LoadData 0x101fe208, 0x101fe52c, which the inventory files
// under TPEmbed.cpp; UIInterface::InitLobby 0x101ef750).

char gBFCursorFolderSet;                  // 0x10380b8c
const char* gBFCursorFolder;              // 0x10380b90
extern CLight gUILight;

// The cursor of the file "data:UI:cursors:<name>" (found with FindFilePath, which answers the path to read), or
// NULL when there is no such file. The folder's name is set on the first call (a static in the original).
InterfaceBFCursor* LoadBFCurs(const char* name) {
    InterfaceBFCursor* cursor = nullptr;
    if (gBFCursorFolderSet == 0) {
        gBFCursorFolder = "data:UI:cursors";
        gBFCursorFolderSet = 1;
    }
    char path[1024];
    // (sprintf in the original; the names are short)
    std::snprintf(path, sizeof path, "%s:%s", gBFCursorFolder, name);
    const char* file = FindFilePath(path, 1, nullptr);
    if (file != nullptr) {
        cursor = new InterfaceBFCursor;
        if (cursor != nullptr) cursor->LoadData(file);
    }
    return cursor;
}

// Loads the 22 cursors into gCursorTable by number: a picture that is not found leaves its number empty. The names
// are those of the files in data:UI:cursors (CNor normal, Cbus, Cpat, Cque, Clin, Cpla, Cnog, Cpic, Ccar, Cera, Cpyl,
// Ccam, Cash, Ctra, Chei, Crot, Cpiv, Csdn, Csup, CEnd, CMov, CCro). The car cursor (number 9) has its hot spot at
// (10, 64) and the last (number 0x15, CCro) at (16, 16).
void InitialiseCOLOURCursors() {
    CursorTable* table = &gCursorTable;
    InterfaceBFCursor* cursor;
    cursor = LoadBFCurs("CNor.tga");
    if (cursor != nullptr) table->SetEntry(0, cursor);
    cursor = LoadBFCurs("Cbus.tga");
    if (cursor != nullptr) table->SetEntry(1, cursor);
    cursor = LoadBFCurs("Cpat.tga");
    if (cursor != nullptr) table->SetEntry(3, cursor);
    cursor = LoadBFCurs("Cque.tga");
    if (cursor != nullptr) table->SetEntry(4, cursor);
    cursor = LoadBFCurs("Clin.tga");
    if (cursor != nullptr) table->SetEntry(5, cursor);
    cursor = LoadBFCurs("Cpla.tga");
    if (cursor != nullptr) table->SetEntry(7, cursor);
    cursor = LoadBFCurs("Cnog.tga");
    if (cursor != nullptr) table->SetEntry(8, cursor);
    cursor = LoadBFCurs("Cpic.tga");
    if (cursor != nullptr) table->SetEntry(2, cursor);
    cursor = LoadBFCurs("Ccar.tga");
    if (cursor != nullptr) {
        table->SetEntry(9, cursor);
        cursor->SetHot(10, 64);
    }
    cursor = LoadBFCurs("Cera.tga");
    if (cursor != nullptr) table->SetEntry(10, cursor);
    cursor = LoadBFCurs("Cpyl.tga");
    if (cursor != nullptr) table->SetEntry(11, cursor);
    cursor = LoadBFCurs("Ccam.tga");
    if (cursor != nullptr) table->SetEntry(0x13, cursor);
    cursor = LoadBFCurs("Cash.tga");
    if (cursor != nullptr) table->SetEntry(0x14, cursor);
    cursor = LoadBFCurs("Ctra.tga");
    if (cursor != nullptr) table->SetEntry(6, cursor);
    cursor = LoadBFCurs("Chei.tga");
    if (cursor != nullptr) table->SetEntry(12, cursor);
    cursor = LoadBFCurs("Crot.tga");
    if (cursor != nullptr) table->SetEntry(13, cursor);
    cursor = LoadBFCurs("Cpiv.tga");
    if (cursor != nullptr) table->SetEntry(14, cursor);
    cursor = LoadBFCurs("Csdn.tga");
    if (cursor != nullptr) table->SetEntry(15, cursor);
    cursor = LoadBFCurs("Csup.tga");
    if (cursor != nullptr) table->SetEntry(16, cursor);
    cursor = LoadBFCurs("CEnd.tga");
    if (cursor != nullptr) table->SetEntry(0x12, cursor);
    cursor = LoadBFCurs("CMov.tga");
    if (cursor != nullptr) table->SetEntry(0x11, cursor);
    cursor = LoadBFCurs("CCro.tga");
    if (cursor != nullptr) {
        table->SetEntry(0x15, cursor);
        cursor->SetHot(16, 16);
    }
}

uint32_t LbTimeClockTickFunction::tickyticky() { return LbTime_GetClock(); }

// Starts the interface: the title text object (made once), the debug font, the UI system
// and its callbacks, the string heap, the UI's heap, the cursors, then every mesh of the
// interface, the title text's style, the help text.
void InitGeneralUI() {
    if (gTitleText == nullptr) gTitleText = new InterfaceTitleTextObject(nullptr);
    gInGameFont.Gen8x8DebugFont(0xff, static_cast<int16_t>(0x4000 / gUIRealScreenWidth), 0,
                                static_cast<int16_t>(0x4800 / gUIRealScreenHeight),
                                static_cast<int16_t>(0x3000 / gUIRealScreenHeight));
    UI_InitSystem(0x80, 0x800, gText.IsHebrew());
    UI_SetClockTickFunction(&gUIClockTick);
    UI_SetPeekCallBack(EffectPeekMsg);
    InterfaceStringHeap::Allocate(0x40, 0x80, 0x10, 0x80);
    HNewHeap(0x100000, 3, 0xd, &gUIHeap);
    gCursorTable.Allocate(0x16);
    InitialiseCOLOURCursors();
    UI_UseCursorTable(&gCursorTable);
    UI_SetStandardCursor(0);
    UI_SetCursor(1);
    UI_HideCursor();
    const int32_t meshes = LoadAllUIMeshes();
    LoadLocalisedUIMeshes();
    TRACE("Loaded %d UI meshes\n", meshes);
    gTitleText->mInLibrary = 1;
    // (The rest of both records is whatever the stack held.)
    DOTextStyle style;
    DOChange change;
    style.mColour[0] = 0xff;
    style.mColour[1] = 0xff;
    style.mColour[2] = 0xff;
    style.mF09 = 0;
    style.mF04 = 1;
    change.mText = &style;
    style.mF05 = 1;
    gTitleText->Change(0xc00, &change);
    InitHelpText();
}

// Three windows the end of the interface deletes. Nothing translated sets them (original names unknown).
InterfaceWindow* gUIGeneralWindowA;   // 0x10380b88
InterfaceWindow* gUIGeneralWindowB;   // 0x10380b84
InterfaceWindow* gUIGeneralWindowC;   // 0x10380b80

// Undoes InitGeneralUI: the cursor table, the UI system, the three windows (A, B, C: the original reaches each destructor
// through the vtable pointer at +0x38 of the Mac's window, slot 2, with the delete flag 1), the string heap, the debug
// font's bitmap and the UI's heap. The title text object, the draw object library and the meshes are left as they are.
void DeInitGeneralUI() {
    gCursorTable.DeAllocate();
    UI_CloseSystem();
    if (gUIGeneralWindowA != nullptr) delete gUIGeneralWindowA;
    if (gUIGeneralWindowB != nullptr) delete gUIGeneralWindowB;
    if (gUIGeneralWindowC != nullptr) delete gUIGeneralWindowC;
    InterfaceStringHeap::DeAllocate();
    gInGameFont.Release();
    HFreeHeap(gUIHeap);
}

// ui_main.cpp (main): InitGameUI 0x10185e58 (944 B), DeInitGameUI 0x1018622c (212 B).
// UIInterface wrappers: InitGame 0x101ef6a4 (44 B), DeInitGame 0x101ef6fc (36 B).

float gGameWindowCentreX;              // 0x102b2b5c
float gGameWindowCentreY;              // 0x102b2b58
float gGameWindowHalfWidth = 1.0f;     // 0x10278a28
float gGameWindowHalfHeight = 1.0f;    // 0x10278a2c

void InitGameUI() {
    UI_FlushAllMessages();
    gText.InitThemeSpecific();
    CreateStaticTags();
    if (gMouse == nullptr) {
        CMouse* mouse = new (std::nothrow) CMouse;
        if (mouse != nullptr) {
            CMMDefault* mode = new (std::nothrow) CMMDefault;
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    if (gMouseMode != nullptr) gMouseMode->GetMode();
    CLight* light = &gUILight;
    gei_System.UpdateLights();
    light->mColorWord = 0xffffffffu;
    light->mColor[0] = 1.0f;
    light->mColor[1] = 1.0f;
    light->mColor[2] = 1.0f;
    light->mColor[3] = 1.0f;
    light = &gUILight;
    gei_System.UpdateLights();
    light->mNormal.x = -0.3f;
    light->mNormal.y = -0.5f;
    light->mNormal.z = 0.8f;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(light), 0x20);

    InterfaceWindow* window = gUIRootWindow->CreateObject(1, 0, 0, 0, 0, 0x7ff, 0x5ff);
    if (window != nullptr) {
        window->Show(1);
        window->SetCallbackFunction(Engine_GameWinMsg);
        gGameWindow = window;
        CoolRect rect;
        window->GetPosition(&rect, 1);
        const int32_t halfWidth = (static_cast<int32_t>(rect.mRight) - rect.mLeft) / 2;
        const int32_t width = gUIRealScreenWidth;
        const int32_t centreX = rect.mLeft + halfWidth;
        const int32_t scaledHalfWidth = width * halfWidth;
        gGameWindowCentreX = static_cast<float>((width * centreX) / 2048);
        const int32_t halfHeight = (static_cast<int32_t>(rect.mBottom) - rect.mTop) / 2;
        const int32_t height = gUIRealScreenHeight;
        const int32_t centreY = rect.mTop + halfHeight;
        // lfd / fsubs converts each signed quotient, with one rounding to single precision.
        gGameWindowCentreY = static_cast<float>((height * centreY) / 1536);
        gGameWindowHalfWidth = static_cast<float>(scaledHalfWidth / 2048);
        gGameWindowHalfHeight = static_cast<float>((height * halfHeight) / 1536);
    }
    window = gUIRootWindow->CreateObject(1, 0, 1, 0, 0, 0x7ff, 0x5ff);
    if (window != nullptr) {
        window->SetCallbackFunction(Engine_GameFirstPersonMsg);
        gFirstPersonWindow = window;
        window->setCursor(0x15);
        gFirstPersonWindow->Build(gFirstPersonWindowScript, nullptr);
    }
    CreateMainPanel();
    InitControlPanel();
    InitStaffPanel();
    InitCashPanel();
    InitTagPanel();
    InitCoasterPanel();
    gUIPanelWindow = nullptr;
    UIMain_DecideKbCapture();
    if (gPlayerSettings.mCurrent.mTooltipsOn) UI_EnableHelpText();
    else UI_DisableHelpText();
}

void DeInitGameUI() {
    InstantRemoveAllTags();
    gei_MapWho->Unlink(static_cast<CMapLinkBase*>(&gUILight), 0x20);
    UI_DisableHelpText();
    DeInitControlPanel();
    DeInitStaffPanel();
    DeInitCashPanel();
    DeInitTagPanel();
    DeInitCoasterPanel();
    UI_SendMessage(gGameWindow, 4, 0, 0);
    UI_SendMessage(gFirstPersonWindow, 4, 0, 0);
    DestroyStaticTags();
    gText.DeInitThemeSpecific();
}

// ---- the lobby

// Empties the message queues, lights the interface's light white from the front (its colour word all ones,
// the four colours 1.0, direction (-0.3, -0.5, 0.8): each change tells the engine the lights changed), links it
// into the map's light list 0x20 and turns the help text on or off by the player's tooltips setting.
void InitLobbyUI() {
    UI_FlushAllMessages();
    CLight* light = &gUILight;
    gei_System.UpdateLights();
    light->mColorWord = 0xffffffffu;
    light->mColor[0] = 1.0f;
    light->mColor[1] = 1.0f;
    light->mColor[2] = 1.0f;
    light->mColor[3] = 1.0f;
    light = &gUILight;
    gei_System.UpdateLights();
    light->mNormal.x = -0.3f;
    light->mNormal.y = -0.5f;
    light->mNormal.z = 0.8f;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(light), 0x20);
    if (gPlayerSettings.mCurrent.mTooltipsOn)
        UI_EnableHelpText();
    else
        UI_DisableHelpText();
}

// The way out of the lobby (main app): the key sparkle when the player enters an island (ui_islandLobbyController.cpp
// 0x101e1460), the interface's lobby tear-down (ui_main.cpp 0x10186434, ui_ErrorDisplay.cpp 0x101ef7a4) and the first-person
// ride test (TPCamera.cpp 0x100329cc).

// Takes the interface's light out of the map's light list 0x20 (InitLobbyUI linked it) after turning the help text off. The
// Mac tests the address of the light's pointer word against NULL (a constant that is never NULL).
void DeInitLobbyUI() {
    UI_DisableHelpText();
    gei_MapWho->Unlink(static_cast<CMapLinkBase*>(&gUILight), 0x20);
}

// Unit v5060: the online lobby's interface set-up (InitOnlineUI 0x101864b0) and the bankrupt window's message function
// (BankruptWindowMsg 0x101868c0), Mac file ui_main.cpp.

void CreateOnlinePanel();   // 0x101e8f08

// Gives the interface's light its colour (white, colour word all ones) and direction, links it into the map's light list 0x20,
// builds the online panel and the first person window (the full-screen window over the root, the message function
// Engine_GameFirstPersonMsg, cursor 0x15, script gFirstPersonWindowScript), then sets the help text by the tooltips setting.
void InitOnlineUI() {
    UI_FlushAllMessages();
    gei_System.UpdateLights();
    gUILight.mColorWord = 0xffffffffu;
    gUILight.mColor[0] = 1.0f;
    gUILight.mColor[1] = 1.0f;
    gUILight.mColor[2] = 1.0f;
    gUILight.mColor[3] = 1.0f;
    gei_System.UpdateLights();
    gUILight.mNormal.x = -0.3f;
    gUILight.mNormal.y = -0.5f;
    gUILight.mNormal.z = 0.8f;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(&gUILight), 0x20);
    CreateOnlinePanel();
    InterfaceWindow* window = gUIRootWindow->CreateObject(1, 0, 1, 0, 0, 0x7ff, 0x5ff);
    if (window != nullptr) {
        window->SetCallbackFunction(Engine_GameFirstPersonMsg);
        gFirstPersonWindow = window;
        window->setCursor(0x15);
        gFirstPersonWindow->Build(gFirstPersonWindowScript, nullptr);
    }
    if (gPlayerSettings.mCurrent.mTooltipsOn)
        UI_EnableHelpText();
    else
        UI_DisableHelpText();
}

// DeInitOnlineUI 0x10186620 (124 bytes), Mac file ui_main.cpp: the undo of InitOnlineUI.

void RemoveOnlinePanel();   // 0x101e92c4 (56 bytes)

void DeInitOnlineUI() {
    RemoveOnlinePanel();
    UI_SendMessage(gFirstPersonWindow, 4, 0, 0);
    UI_DisableHelpText();
    gei_MapWho->Unlink(static_cast<CMapLinkBase*>(&gUILight), 0x20);
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

extern int32_t gUIOnFrameCount;

int32_t gUIGeneralQueued;
int32_t gUIDrawQueued;
extern int32_t gPostcardDelay;

extern int32_t gPostcardWanted;

extern std::deque<errorthingy> gUIErrorQueue;

extern bthread_mutex gUIErrorMutex;

extern InterfaceWindow* gUIErrorDialog;

int32_t UI_DispatchMessages(int32_t limit);

int32_t UI_DispatchDrawMessages(int32_t limit);

int32_t UI_DispatchDeferredMessages();

void onframe(InterfaceWindow* window);

int32_t UI_SendOnFrameMessages();

uint32_t IveGotTicks();

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect);

void UI_ServiceAllTimers();

bool UI_PostcardPending();

void TickUIErrors();

// ---- the pass

void ProcessUI() {
    TickUIErrors();
    UI_ServiceAllTimers();
    gUIGeneralQueued = gUIMessageQueue->GetNumQd();
    gUIDrawQueued = gUIDrawQueue->GetNumQd();
    if (UI_PostcardPending()) UI_ActuallyDoPostcard();
    UI_DispatchMessages(0x400);
    UI_DispatchDeferredMessages();
    UI_PostMessage(gUIRootWindow, 2, 0, 0);
    UI_SendOnFrameMessages();
    UI_DispatchDrawMessages(0x1000);
}

// ui_main.cpp, main app 0x1018679c and 0x10186854 (unit w88): the pause end of the interface (UnPauseUI is in ui_main_peek.cpp).

// 0x1018679c (60 bytes): with a root window, the timers stop and the pause window is shown.
void PauseUI() {
    if (gUIRootWindow != nullptr) {
        UI_PauseTimerSystem();
        ShowPauseWindow();
    }
}

void UnPauseUI() {
    if (gUIRootWindow != nullptr) {
        UI_UnPauseTimerSystem();
        HidePauseWindow();
    }
}

// 0x10186854 (72 bytes): the pause window is made or closed, then the timers follow whether one is up now (no root window test).
void TogglePauseUI() {
    TogglePauseWindow();
    if (gPauseWindow == nullptr) {
        UI_UnPauseTimerSystem();
    } else {
        UI_PauseTimerSystem();
    }
}

namespace {

// Closes the bankrupt window's way: puts the bumhole window back to the bankrupt script and animates it from its middle.
InterfaceWindow* BankruptRebuild() {
    InterfaceWindow* window = gBumholeWindow->Build(gBankruptScript, BankruptWindowMsg);
    return window;
}

void BankruptAnimate(InterfaceWindow* window) {
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gBankruptAnim, 0x23);
}

}  // namespace

// Message 0x15 keeps the UI clock; 0x14 forgets the window; the right button (0x10006, button 1, RMB cancel on) more than 2000 ticks
// later, or the escape key (0x1000b, 0x1b, no modifier), closes the window and animates the bumhole window; the rest goes to the
// default handler (the 0x1000b case answers 0 in every path).
int32_t BankruptWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (a == 1 && gPlayerSettings.mCurrent.mRMBCancelOn &&
            static_cast<int32_t>(IveGotTicks() - static_cast<uint32_t>(gCongratsTicks)) > 2000 && gBankruptWindow != nullptr) {
            UI_SendMessage(window, 4, 0, 0);
            StopRidingRideIn1stPerson();
            ToggleGameWindow();
            InterfaceWindow* built = BankruptRebuild();
            EnableAllKeytables();
            gUIMainKeyTablesDisabled = 0;
            BankruptAnimate(built);
            return 0;
        }
    } else if (message < 0x10006) {
        if (message == 0x15) {
            gCongratsTicks = static_cast<int32_t>(IveGotTicks());
        } else if (message < 0x15 && message > 0x13) {
            gBankruptWindow = nullptr;
        }
    } else if (message == 0x1000b) {
        if (gBankruptWindow != nullptr && a == 0x1b && ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0) {
            UI_SendMessage(window, 4, 0, 0);
            StopRidingRideIn1stPerson();
            ToggleGameWindow();
            InterfaceWindow* built = BankruptRebuild();
            CoolRect rect;
            built->GetPosition(&rect, 1);
            built->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
            built->startAnim(gBankruptAnim, 0x23);
            EnableAllKeytables();
            gUIMainKeyTablesDisabled = 0;
        }
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// ui_main.cpp (main app): UI_Congratulations 0x1018724c, UI_Bankrupt 0x10186f34, CongratsWindowMsg 0x10186b9c and the file's static
// initialiser __sinit_ui_main_cpp 0x101877b8 (unit v5022).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

// ---- the image's data (the two window scripts are generated at configure time, unit v5L42) and the file's globals
int16_t gCongratsScript[] = {TPW_WINDOW_SCRIPT_102A772C};   // 0x102a772c (21 words)
int16_t gBankruptScript[] = {TPW_WINDOW_SCRIPT_102A7756};   // 0x102a7756 (21 words)
// The window animations (the image's frames): the congratulations window slides in, rises, spins once, drops and slides out, then again
// from the start; the bankrupt window shakes, sinks and closes in, then again.
UI_RectDelta gCongratsAnim[] = {                   // 0x102a79d4 (29 frames)
    {16, 0, -16, 0}, {16, 0, -16, 0}, {16, 0, -16, 0},
    {16, 0, -16, 0}, {16, 0, -16, 0}, {16, 0, -16, 0},
    {0, -4, 0, -4}, {0, -4, 0, -4}, {0, -4, 0, -4},
    {0, -4, 0, -4}, {kAnimZRot, 512, 0, 0}, {kAnimZRot, 1024, 0, 0},
    {kAnimZRot, 1536, 0, 0}, {kAnimZRot, 2048, 0, 0}, {kAnimZRot, 2560, 0, 0},
    {kAnimZRot, 3072, 0, 0}, {kAnimZRot, 3584, 0, 0}, {kAnimZRot, 4096, 0, 0},
    {0, 4, 0, 4}, {0, 4, 0, 4}, {0, 4, 0, 4},
    {0, 4, 0, 4}, {-16, 0, 16, 0}, {-16, 0, 16, 0},
    {-16, 0, 16, 0}, {-16, 0, 16, 0}, {-16, 0, 16, 0},
    {-16, 0, 16, 0}, {kAnimGoto, 0, 0, 0},
};
UI_RectDelta gBankruptAnim[] = {                   // 0x102a7914 (24 frames)
    {-1, -1, 1, 1}, {0, -1, 0, 1}, {-1, -1, 1, 1},
    {0, -1, 0, 1}, {-1, -1, 1, 1}, {0, -1, 0, 1},
    {-1, -1, 1, 1}, {0, -1, 0, 1}, {-1, -1, 1, 1},
    {0, 1, 0, -1}, {0, 1, 0, -1}, {0, 1, 0, -1},
    {0, 1, 0, -1}, {0, 1, 0, -1}, {0, 1, 0, -1},
    {0, 1, 0, -1}, {0, 1, 0, -1}, {0, 1, 0, -1},
    {1, 0, -1, 0}, {1, 0, -1, 0}, {1, 0, -1, 0},
    {1, 0, -1, 0}, {1, 0, -1, 0}, {kAnimGoto, 0, 0, 0},
};
int32_t gCongratsTicks;                            // 0x10380b78
int32_t gUIMainWordCCF8;                           // 0x1038ccf8
int32_t gUIMainWordCCFC;                           // 0x1038ccfc
uint8_t gUIMainNodeLight[12];                      // 0x1038ccec
uint8_t gUIMainNodeCursors[12];                    // 0x1038cc6c
uint8_t gUIMainNodeFrame[12];                      // 0x10380c08
uint8_t gUIMainNodeSlider[12];                     // 0x10380be8
uint8_t gUIMainNodePlain[12];                      // 0x10380bc8
uint8_t gUIMainNodeButton[12];                     // 0x10380ba8

void UIMain_DestroyLight(void* object) { static_cast<CLight*>(object)->~CLight(); }
void UIMain_DestroyCursorTable(void* object) { static_cast<CursorTable*>(object)->~CursorTable(); }
void UIMain_DestroyString(void* object) {
    static_cast<TbDynamicStringTemplate<char>*>(object)->~TbDynamicStringTemplate();
}

// 0x10186f34, 428 bytes. The callback is set before the script is built.
void UI_Bankrupt(CObject* object) {
    UIMain_GetFullyOutOfFirstPerson();
    object->GoFirstPerson(0);
    DisableAllKeytables();
    gUIMainKeyTablesDisabled = 1;
    if (GameWindowToggle_Hidden() != 0) ToggleGameWindow();
    if (gUIPanelWindow != nullptr) {
        UI_SendMessage(gUIPanelWindow, 5, 0, 0);
        gUIPanelWindow = nullptr;
        UIMain_DecideKbCapture();
    }
    UI_SendMessage(gGameWindow, 6, 0, 0);
    UI_SendMessage(gFirstPersonWindow, 6, 0, 0);
    InterfaceWindow* created = gUIRootWindow->CreateObject(1, 1, 0x7472, 0, 0, 0x7ff, 0x5ff);
    created->SetCallbackFunction(BankruptWindowMsg);
    InterfaceWindow* window = created->Build(gBankruptScript, nullptr);
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gBankruptAnim, 0x23);
    created->CaptureKb(0);
    gBankruptWindow = created;
}

// UI_BankruptAfterLoad 0x1018710c, 232 bytes.

void UI_BankruptAfterLoad() {
    gBankruptWindow = nullptr;
    if (GameWindowToggle_Hidden() == 0) ToggleGameWindow();
    InterfaceWindow* window = gBumholeWindow->Build(gBankruptScript, BankruptWindowMsg);
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gBankruptAnim, 0x23);
    gBumholeWindow->CaptureKb(0);
}

// Unit v51110: the server and ride lists' small members, their destructors and LoggedInGetList, the FS_OnlineList static initialiser and
// three one-liners (CFSOnlineRide::GetFilePath, CFSOnlineSystem::GetAvatarID, UI_ByeByeBankrupt).

void UI_ByeByeBankrupt() {}   // 0x10187220 (4): blr

// 0x1018724c, 436 bytes.
void UI_Congratulations(CObject* object) {
    UIMain_GetFullyOutOfFirstPerson();
    object->GoFirstPerson(0);
    DisableAllKeytables();
    gUIMainKeyTablesDisabled = 1;
    if (GameWindowToggle_Hidden() != 0) ToggleGameWindow();
    if (gUIPanelWindow != nullptr) {
        UI_SendMessage(gUIPanelWindow, 5, 0, 0);
        gUIPanelWindow = nullptr;
        UIMain_DecideKbCapture();
    }
    UI_SendMessage(gGameWindow, 6, 0, 0);
    UI_SendMessage(gFirstPersonWindow, 6, 0, 0);
    InterfaceWindow* created = gUIRootWindow->CreateObject(1, 1, 0x7472, 0, 0, 0x7ff, 0x5ff);
    InterfaceWindow* window = created->Build(gCongratsScript, nullptr);
    window->Enable(0);   // the virtual at table offset 0x1c
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gCongratsAnim, 0x23);
    created->SetCallbackFunction(CongratsWindowMsg);
    created->CaptureKb(0);
}

// The end of the window (inlined twice in the original): the winning sequence over, the windows and key tables back, and for the easy
// game type a dialog with the interface string 0x1d7. The string is converted into gUIMainConvertBuffer when the converter (resource 1)
// is loaded; the dialog's text is that buffer, or NULL when the table has no such string.
static void CongratsClose(InterfaceWindow* window) {
    UI_SendMessage(window, 4, 0, 0);
    gGameWorld->EndWinningSequence();
    UI_SendMessage(gFirstPersonWindow, 6, 0, 0);
    UI_SendMessage(gGameWindow, 6, 1, 0);
    EnableAllKeytables();
    gUIMainKeyTablesDisabled = 0;
    UIMain_DecideKbCapture();
    if (TheGameType().mType == kGameTypeEasy) {
        const unsigned char* string = static_cast<TbITable*>(gText.mResourcePointers[6])->GetConstString(0x1d7);
        char16_t* text = gUIMainConvertBuffer;
        if (string == nullptr) {
            text = nullptr;
        } else if (gText.mResourcePointers[1] != nullptr) {
            static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0])->ConvertString(gUIMainConvertBuffer, string, 0x200);
        }
        dlgboxparams params;
        params.mText = text;
        params.mRed = 0xff;
        params.mGreen = 0xff;
        params.mBlue = 0xff;
        params.mValue8 = 4;
        params.mValueC = 0;
        UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(0), nullptr, static_cast<dlgbutton>(0), nullptr, false);
    }
}

// 0x10186b9c, 860 bytes.
int32_t CongratsWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (a == 1 && *reinterpret_cast<const uint8_t*>(&gPlayerSettings.mCurrent.mRMBCancelOn) != 0 &&   // a byte test (cmplwi), not a bool's low bit

            static_cast<int32_t>(IveGotTicks() - static_cast<uint32_t>(gCongratsTicks)) > 2000) {
            CongratsClose(window);
            return 0;
        }
    } else if (message < 0x10006) {
        if (message == 0x15) gCongratsTicks = static_cast<int32_t>(IveGotTicks());
    } else if (message == 0x1000b) {
        if (a == 0x1b && ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0) CongratsClose(window);
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

// Two small functions of the interface's main file (ui_main.cpp, main app; unit w40): GetFontTableEntry 0x10180270 (48 bytes), which
// every panel builder uses to pick a font, and UI_CreateVersionNumber 0x101874d0 (528 bytes), which CLBLobby::Init calls to put the
// game's version under the lobby.

namespace {

// The version label's format at 0x1024da40 (the wide string behind the table slot 0x1026392c's target 0x1024da3e, plus 2): a "v", a
// space and the version resource's short text, a plain (narrow) string.
const char16_t kVersionFormat[] = u"v %s";

}  // namespace

// 0x10180270. The font table is 13 fonts to a resolution group (0x138 bytes a group on the Mac, 0x18 a font, from +0x1c of the text
// object); 13 and up is no font. The group is not checked (SetRez keeps it in 0..3).

// 0x101874d0. The label is "v " and the version text, at most 63 characters. It is measured (font 6, left and top justified, not
// wrapped, in a box of 32000 by 32000), and the window is the measured size scaled from the screen's real pixels to the interface's
// 2048 x 1536 units (the widths and heights are signed 32-bit products, divided with the screen's size as a signed halfword; a screen
// size of 0 would divide by zero, as on the Mac), ending at (0x7c0, 0x5c0) whatever the size. The window's coordinates are 16 bits.
// It has a text draw object as its client draw object, the text font 6 of the current group, and the label as its text.
void UI_CreateVersionNumber(bool show) {
    InterfaceWindow* parent = show ? gUIRootWindow : gLobby->mWindow;
    SamsUtilities::UVersion version(2, 0, 0, 0);
    SamsUtilities::UString text;
    version.LoadFromResource(SamsUtilities::UVersion::kDefaultVersionResource, 0);
    text = version.mText;
    char16_t label[64];
    swprintf(label, 0x40, kVersionFormat, text.mText);
    const TbRect limit = {0, 0, 32000, 32000};
    TbRect size = {0, 0, 0, 0};
    UI_MeasureText(label, limit, 6, 0, 0, 0, &size);
    const int32_t width = ((size.right - size.left) * 0x800) / static_cast<int32_t>(gUIRealScreenWidth);
    const int32_t left = 0x7c0 - width;
    const int32_t height = ((size.bottom - size.top) * 0x600) / static_cast<int32_t>(gUIRealScreenHeight);
    const int32_t top = 0x5c0 - height;
    InterfaceWindow* window = parent->CreateObject(1, 3, 0x8cfd55, static_cast<int16_t>(left), static_cast<int16_t>(top),
                                                   static_cast<int16_t>(left + width), static_cast<int16_t>(top + height));
    window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    window->SetFont(&gText.mFonts[gText.mResolutionGroup * 13 + 6]);
    window->SetClientDrawText(label);
}

// 0x101877b8, 436 bytes.
void __sinit_ui_main_cpp() {
    gUIMainWordCCFC = 0;
    gUIMainWordCCF8 = 0x10000;
    // The interface's light: the Mac builds a CLightParallel in place (the link base first, then the light with its virtual base left
    // out), points its vtable words at the 0x10278c58 record and sets flag 2; the host builds the light.
    CLight* light = new (&gUILight) CLight();
    light->mFlags = 2;
    light->SetNormal(0.0f, 0.0f, 1.0f);
    light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    __register_global_object(light, reinterpret_cast<void*>(&UIMain_DestroyLight), gUIMainNodeLight);
    CursorTable* table = new (&gCursorTable) CursorTable();
    __register_global_object(table, reinterpret_cast<void*>(&UIMain_DestroyCursorTable), gUIMainNodeCursors);
    auto* frame = new (&gUIFrameName) TbDynamicStringTemplate<char>("!frame");
    __register_global_object(frame, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodeFrame);
    auto* slider = new (&gUISliderName) TbDynamicStringTemplate<char>("!slider");
    __register_global_object(slider, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodeSlider);
    auto* plain = new (&gUIPlainFrameName) TbDynamicStringTemplate<char>("!f_plain");
    __register_global_object(plain, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodePlain);
    auto* button = new (&gUIButtonName) TbDynamicStringTemplate<char>("!cbut");
    __register_global_object(button, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodeButton);
    // Last the original stores the two vtable words of gUIClockTick (the object's own construction): nothing to do here.
}

// Park window scripts (main); signed 16-bit records, in image order.

// 0x102a76da, 41 words
int16_t gFirstPersonWindowScript[41] = {
    0, 1, 3, 0, -9390, 137, 170, 170, 1878, 1366,
    17, -1, -1,
    18, -1, -1,
    1, -16675, -20214,
    5,
    0, 2, 1, 0, -9389, 137, 1916, 1404, 2018, 1506,
    17, 468, 0,
    18, -1, -1,
    1, -16704, 11571,
    5,
    5,
};
