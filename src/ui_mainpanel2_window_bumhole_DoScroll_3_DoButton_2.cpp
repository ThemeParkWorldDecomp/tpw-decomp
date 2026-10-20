// Unit v5978 (Mac file ui_parkStatus.cpp): the options panel's DoButton 0x101a9808 and message function OptionsPanelMsg 0x101a9d94.
#include <cstdint>
#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/msl.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "Game.h"
#include "HLSprite.h"
#include "MapWho.h"
#include "tb_ddraw.h"
#include "ui_ErrorDisplay.h"
#include "ui_parkStatus.h"

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
