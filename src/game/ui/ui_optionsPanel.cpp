// ui_optionsPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Advisor.h"
#include "AdvisorQueue.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_MasterSave.h"
#include "Game.h"
#include "HLSprite.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keyboard.h"
#include "keytable_actions.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "MapWho.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/msl.h"
#include "tb_ddraw.h"
#include "tb_texturecache.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"
#include "ui_Postcard.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <new>

// Unit v5143 (Mac file ui_mainpanel2.cpp): the bumhole window's message function, the game window toggle and SetOnOffText.

InterfaceWindow* gSetOnOffRoot;    // 0x10394a14

// 0x101a8e60 (304 bytes).
void SetOnOffText(int32_t window, int32_t text, bool first, bool second, bool button, int32_t buttonId) {
    InterfaceWindow* parent = gSetOnOffRoot->GetWindow(window);
    InterfaceWindow* label = parent->GetWindow(0x1d4c9);
    int32_t id;
    if (second) {
        id = first ? 0x14f : 0x14e;
    } else {
        id = first ? 0x14d : 0x14c;
    }
    char16_t local[38];
    wcsncpy(local, UIMain_GetString(id), 0x1f);
    UIMain_GetString(text);
    wcscat(gUIMainConvertBuffer, local);
    label->SetText(gUIMainConvertBuffer);
    if (button) {
        InterfaceButton* control = static_cast<InterfaceButton*>(gSetOnOffRoot->GetWindow(buttonId));
        if (first) {
            control->SetState(0, 1);
        } else {
            control->SetState(1, 1);
        }
    }
}

// Unit v5978 (Mac file ui_parkStatus.cpp): the options panel's DoButton 0x101a9808 and message function OptionsPanelMsg 0x101a9d94.

// The virtual calls through the table at window +0x118 (SetValue slot 0x24, SetText slot 0x2c).
__attribute__((weak)) int32_t options_callee::CallSetValue(InterfaceWindow* window, int32_t value) { return window->SetValue(value); }
__attribute__((weak)) void options_callee::CallSetText(InterfaceWindow* window, const char16_t* text) {
    window->SetText(const_cast<char16_t*>(text));
}

namespace {

// The label of a row: the prefix string (when it is not negative) cut to 0x3f characters, then the suffix string appended.
// UIMain_GetString leaves its answer in gUIMainConvertBuffer, which is what the window gets.
void SetLabel(InterfaceWindow* window, int32_t prefix, int32_t suffix) {
    char16_t local[64];
    if (prefix > -1) {
        wcsncpy(local, UIMain_GetString(prefix), 0x3f);
        UIMain_GetString(suffix);
        wcscat(gUIMainConvertBuffer, local);
    } else {
        UIMain_GetString(suffix);
    }
    options_callee::CallSetText(window, gUIMainConvertBuffer);
}

InterfaceWindow* LabelOf(int32_t row) { return gSetOnOffRoot->GetWindow(row)->GetWindow(0x1d4c9); }

}  // namespace

namespace options_panel {

// 0x101a9808 (1388 bytes): a switch or button of the panel was pressed; `on` is the button's state (the flags store "state != 1").
void DoButton(int32_t id, int32_t on) {
    CPlayerSettings::Settings& s = gPlayerSettings.mCurrent;
    bool flag = on != 1;
    switch (static_cast<uint32_t>(id) - 0x1d4c1u) {
    case 0:
        s.mAdvisorOn = flag;
        SetOnOffText(0x1d4cb, 0x145, s.mAdvisorOn, true, false, 0);
        break;
    case 1:
        s.mTutorialOn = flag;
        SetOnOffText(0x1d4cc, 0x146, s.mTutorialOn, true, false, 0);
        break;
    case 2:
        s.mTooltipsOn = flag;
        SetOnOffText(0x1d4cd, 0x147, s.mTooltipsOn, true, false, 0);
        break;
    case 3:
        s.mConfirmDeleteOn = flag;
        SetOnOffText(0x1d4ce, 0x148, s.mConfirmDeleteOn, true, false, 0);
        break;
    case 4:
        s.mRMBCancelOn = flag;
        SetOnOffText(0x1d4cf, 0x14b, s.mRMBCancelOn, true, false, 0);
        break;
    case 5: {
        s.mIsometricOn = !s.mIsometricOn;
        SetLabel(LabelOf(0x1d4d0), s.mIsometricOn ? 0x161 : 0x160, 0x15f);
        break;
    }
    case 6: {
        s.mRMBScrollOn = !s.mRMBScrollOn;
        SetLabel(LabelOf(0x1d4d1), !s.mRMBScrollOn ? 0x163 : 0x164, 0x162);
        break;
    }
    case 17: {
        gPlayerSettings.SetRendererType(static_cast<CPlayerSettings::eRENDERER>(s.mRenderer == 0));
        int32_t prefix = s.mRenderer == 0 ? 0x13d : 0x13c;
        int32_t level = static_cast<int32_t>(static_cast<uint32_t>(s.mDetail) * 100u) / 2;
        InterfaceWindow* slider = gSetOnOffRoot->GetWindow(0x1d4d6);
        if (static_cast<int16_t>(options_callee::CallSetValue(slider, level)) == 0) DoScroll(0x1d4d6, level);
        InterfaceWindow* label = LabelOf(0x1d4c8);
        UIMain_GetString(prefix);
        options_callee::CallSetText(label, gUIMainConvertBuffer);
        break;
    }
    case 18: {
        if (s.mVideoCard == 0) {
            if (gei_DDraw->mDeviceCount > 1) gPlayerSettings.SetVideoCard(static_cast<CPlayerSettings::eVIDEOCARD>(1));
        } else {
            gPlayerSettings.SetVideoCard(static_cast<CPlayerSettings::eVIDEOCARD>(0));
        }
        int32_t prefix = s.mVideoCard == 0 ? 0x15d : 0x15e;
        SetLabel(LabelOf(0x1d4ca), prefix, 0x15c);
        break;
    }
    case 22:
        s.mSfx.mOn = flag;
        DoScroll(0x1d4d8, gPlayerSettings.GetSFXVolume());
        break;
    case 24:
        s.mMusic.mOn = flag;
        DoScroll(0x1d4da, gPlayerSettings.GetMusicVolume());
        break;
    case 26:
        s.mSpeech.mOn = flag;
        DoScroll(0x1d4dc, gPlayerSettings.GetSpeechVolume());
        break;
    case 28:
        s.mMovie.mOn = flag;
        DoScroll(0x1d4de, gPlayerSettings.GetMovieVolume());
        break;
    default:
        break;
    }
}

}  // namespace options_panel

// 0x101a9d94 (488 bytes)
int32_t OptionsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x800) {
        options_panel::DoScroll(a, b);
    } else if (message > 0x800) {
        if (message == 0x802) {
            gOptionsPanelF0C = 0;
        } else if (message < 0x802) {
            options_panel::StartScroll(a, b);
        }
    } else if (message == 0x100) {
        if (a == -1) {
            gPlayerSettings.UpdateSettings();
            return 0;
        }
        if (a == -2) {
            gPlayerSettings.ResetSettings();
            RemoveOptionsPanel();
            return 0;
        }
        options_panel::DoButton(a, b);
    } else if (message < 0x100 && message == 0x14) {
        gSetOnOffRoot = nullptr;
        gei_MapWho->SetRenderOptions(gOptionsPanelRenderOptions);
        if (gGame.mF3C == 0 && gOptionsPanelGameRunning == 0) {
            gGame.mF3C = 1;
            gGame.Unpause();
            UI_UnPauseTimerSystem();
        }
        if (gLobby != nullptr)
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
        else
            UI_SendMessage(gGameWindow, 6, 1, 0);
        UIMain_DecideKbCapture();
        if (gPlayerSettings.mCurrent.mTooltipsOn)
            UI_EnableHelpText();
        else
            UI_DisableHelpText();
        BTN_HighLight_Kill();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v5634: CreateOptionsPanel 0x101a9fb8 (6432 bytes), the options panel of the game (ui_parkStatus.cpp's neighbourhood on the
// Mac). Translated from the listing; the Ghidra draft had the video card test's comparison the wrong way round.

uint8_t gOptionsPanelGameRunning;      // 0x10394a08
int32_t gOptionsPanelF0C;              // 0x10394a0c
int32_t gOptionsPanelRenderOptions;    // 0x10394a10
uint32_t gOptionsPanelFontIndex = 6;   // 0x102ab668

namespace {

// The child `id` of the panel, whose text child (0x1d4c9) gets a new text sprite (operator new 0x19c and the constructor, not when
// the allocation answered NULL), black, the panel's font and no text. Answers the child.
InterfaceWindow* MakeTextWindow(int32_t id) {
    InterfaceWindow* window = gSetOnOffRoot->GetWindow(id);
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    InterfaceDrawObject* sprite = reinterpret_cast<InterfaceTextObjectSprite*>(memory);
    window->GetWindow(0x1d4c9)->SetClientDrawObject(sprite);
    window->GetWindow(0x1d4c9)->SetFontColour(0, 0, 0, 0xff);
    InterfaceFont* font = GetFontTableEntry(gOptionsPanelFontIndex);
    window->GetWindow(0x1d4c9)->SetFont(font);
    window->GetWindow(0x1d4c9)->SetClientDrawText(nullptr);
    return window;
}

// A text window that is a scroll bar too: 0 to 100, at `value`.
void MakeScrollWindow(int32_t id, int32_t value) {
    InterfaceWindow* window = MakeTextWindow(id);
    static_cast<InterfaceScrollBar*>(window)->SetMinMax(0, 100);
    window->SetValue(value);
}

// The text child of the panel's child `window` gets the string `prefix` followed by the string `value` (at most 0x3f characters of
// it; a string of 0x3f or more leaves the copy unterminated, as on the Mac). The original tests `value` > -1 first; every caller
// passes a number that is, so the second branch never runs.
void SetPairText(int32_t window, int32_t value, int32_t prefix) {
    InterfaceWindow* label = gSetOnOffRoot->GetWindow(window)->GetWindow(0x1d4c9);
    char16_t local[64];
    if (value > -1) {
        wcsncpy(local, UIMain_GetString(value), 0x3f);
        UIMain_GetString(prefix);
        wcscat(gUIMainConvertBuffer, local);
    } else {
        UIMain_GetString(prefix);
    }
    label->SetText(gUIMainConvertBuffer);
}

// The on/off button `id` shows the volume's switch (pressed when off) and the panel hears of it.
void SetVolumeButton(int32_t id, bool on) {
    int8_t state = on ? 0 : 1;
    static_cast<InterfaceButton*>(gSetOnOffRoot->GetWindow(id))->SetState(state, 1);
    options_panel::DoButton(id, state);
}

}  // namespace

// 0x101a9fb8 (6432 bytes).
void CreateOptionsPanel() {
    if (gSetOnOffRoot != nullptr) return;
    CPlayerSettings& settings = gPlayerSettings;
    settings.StoreSettings();
    gOptionsPanelF0C = 0;
    if (gGame.mF3C == 1) {
        gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
        gGame.mF3C = 0;
        UI_PauseTimerSystem();
        gOptionsPanelGameRunning = 0;
    } else {
        gOptionsPanelGameRunning = 1;
    }
    ADV_Kill(1);
    gOptionsPanelRenderOptions = gei_MapWho->GetRenderOptions();
    gei_MapWho->SetRenderOptions(0x21);
    gei_TCache->Flush(0);
    if (gLobby != nullptr) {
        UI_SendMessage(gLobby->mWindow, 6, 0, 0);
    } else {
        UI_SendMessage(gGameWindow, 6, 0, 0);
    }
    gSetOnOffRoot = gUIRootWindow->Build(gOptionsPanelScript, OptionsPanelMsg);
    char16_t* title = const_cast<char16_t*>(UIMain_GetString(0x13b));
    UIMain_SetTitleText(gSetOnOffRoot, 0x1d4d4, title);

    InterfaceWindow* window = gSetOnOffRoot->GetWindow(0x1d4d2);
    int16_t z = static_cast<int16_t>(window->mZ + 2);
    window->setZ(z);
    window->Enable(0);
    window = gSetOnOffRoot->GetWindow(0x1d4d3);
    window->setZ(z);
    window->Enable(0);
    static const int32_t kRaised[] = {0x1d4c1, 0x1d4c2, 0x1d4c3, 0x1d4c4, 0x1d4c5, 0x1d4c6, 0x1d4c7,
                                      0x1d4d7, 0x1d4d9, 0x1d4db, 0x1d4dd, -1,      -2};
    for (int32_t id : kRaised) gSetOnOffRoot->GetWindow(id)->setZ(z);

    static const int32_t kTexts[] = {0x1d4c8, 0x1d4ca, 0x1d4cb, 0x1d4cc, 0x1d4cd, 0x1d4ce, 0x1d4cf, 0x1d4d0, 0x1d4d1};
    for (int32_t id : kTexts) MakeTextWindow(id);

    int32_t resolution = settings.mCurrent.mResolution;
    MakeScrollWindow(0x1d4d5, (resolution - 1) * 100 / 2);
    int32_t detail = settings.mCurrent.mDetail;
    MakeScrollWindow(0x1d4d6, detail * 100 / 2);
    int32_t quality = settings.mCurrent.mSoundQuality;
    MakeScrollWindow(0x1d4e0, quality);
    int32_t volume = settings.GetSFXVolume();
    MakeScrollWindow(0x1d4d8, volume);
    volume = settings.GetMusicVolume();
    MakeScrollWindow(0x1d4da, volume);
    volume = settings.GetSpeechVolume();
    MakeScrollWindow(0x1d4dc, volume);
    volume = settings.GetMovieVolume();
    MakeScrollWindow(0x1d4de, volume);

    // The renderer: the string alone (UIMain_GetString leaves it in gUIMainConvertBuffer).
    InterfaceWindow* label = gSetOnOffRoot->GetWindow(0x1d4c8)->GetWindow(0x1d4c9);
    UIMain_GetString(settings.mCurrent.mRenderer == 0 ? 0x13d : 0x13c);
    label->SetText(gUIMainConvertBuffer);

    // The video card: the second only when it is chosen and the display has more than one device.
    int32_t card = 0x15d;
    if (settings.mCurrent.mVideoCard != 0 && gei_DDraw->mDeviceCount > 1) card = 0x15e;
    SetPairText(0x1d4ca, card, 0x15c);

    options_panel::DoScroll(0x1d4e0, settings.mCurrent.mSoundQuality);
    SetVolumeButton(0x1d4d7, settings.mCurrent.mSfx.mOn);
    SetVolumeButton(0x1d4d9, settings.mCurrent.mMusic.mOn);
    SetVolumeButton(0x1d4db, settings.mCurrent.mSpeech.mOn);
    SetVolumeButton(0x1d4dd, settings.mCurrent.mMovie.mOn);

    int32_t text;
    switch (settings.mCurrent.mResolution) {
        case 0: text = 0x155; break;
        case 1: text = 0x156; break;
        case 2: text = 0x157; break;
        case 3: text = 0x158; break;
        default: text = 0; break;
    }
    SetPairText(0x1d4d5, text, 0x13f);
    switch (settings.mCurrent.mDetail) {
        case 0: text = 0x150; break;
        case 1: text = 0x151; break;
        case 2: text = 0x152; break;
        default: text = 0; break;
    }
    SetPairText(0x1d4d6, text, 0x13e);

    SetOnOffText(0x1d4cb, 0x145, settings.mCurrent.mAdvisorOn, true, true, 0x1d4c1);
    SetOnOffText(0x1d4cc, 0x146, settings.mCurrent.mTutorialOn, true, true, 0x1d4c2);
    SetOnOffText(0x1d4cd, 0x147, settings.mCurrent.mTooltipsOn, true, true, 0x1d4c3);
    SetOnOffText(0x1d4ce, 0x148, settings.mCurrent.mConfirmDeleteOn, true, true, 0x1d4c4);
    SetOnOffText(0x1d4cf, 0x14b, settings.mCurrent.mRMBCancelOn, true, true, 0x1d4c5);

    SetPairText(0x1d4d0, settings.mCurrent.mIsometricOn ? 0x161 : 0x160, 0x15f);
    SetPairText(0x1d4d1, settings.mCurrent.mRMBScrollOn ? 0x164 : 0x163, 0x162);
}

// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.

// 0x101ab904
void RemoveOptionsPanel() { UI_SendMessage(gSetOnOffRoot, 4, 0, 0); }
