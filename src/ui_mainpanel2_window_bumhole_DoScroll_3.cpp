// The options panel's DoScroll (0x101a8fb8) and StartScroll (0x101a96f8).
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

// The virtual calls through the table at +0x118 (slot 0x2c SetText, slot 0x24 SetValue).
namespace options_callee {
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace options_callee

namespace options_panel {

namespace {

// Jump table slots (id - 0x1d4d5 in DoScroll, id - 0x1d4d8 in StartScroll).
constexpr uint32_t SOUNDQ_INDEX = 11, SFX_INDEX = 3, MUSIC_INDEX = 5, SPEECH_INDEX = 7, MOVIE_INDEX = 9;
constexpr uint32_t START_SFX = 0, START_MUSIC = 2, START_SPEECH = 4;

// The shared body of the four volume sliders; answers the value after the "off" test.
int32_t Slider(int32_t id, int32_t value, int32_t textId, bool on) {
    if (!on) value = -1;
    InterfaceWindow* window = gSetOnOffRoot->GetWindow(id);
    char16_t local[10];
    if (value < 0)
        swprintf(local, 10, gOptionsPanelNumberFormats, UIMain_GetString(0x14e));
    else
        swprintf(local, 10, gOptionsPanelNumberFormats + 5, value);
    UIMain_GetString(textId);
    wcscat(gUIMainConvertBuffer, local);
    InterfaceWindow* label = window->GetWindow(0x1d4c9);
    options_callee::WindowSetText(label, gUIMainConvertBuffer);
    if (value >= 0) options_callee::WindowSetValue(window, value);
    return value;
}

}  // namespace

// 0x101a8fb8 (1824 bytes).
void DoScroll(int32_t id, int32_t value) {
    CPlayerSettings::Settings& s = gPlayerSettings.mCurrent;
    uint32_t index = static_cast<uint32_t>(id) - 0x1d4d5u;
    if (index > 11) return;
    switch (index) {
    case 0: {
        gPlayerSettings.SetResolution(static_cast<CPlayerSettings::eRESOLUTION>(
            static_cast<int32_t>(static_cast<uint32_t>(value) << 1) / 100 + 1));
        int32_t strId;
        switch (static_cast<int32_t>(s.mResolution)) {
        case 0: strId = 0x155; break;
        case 1: strId = 0x156; break;
        case 2: strId = 0x157; break;
        case 3: strId = 0x158; break;
        default: strId = 0; break;
        }
        InterfaceWindow* window = gSetOnOffRoot->GetWindow(0x1d4d5);
        window = window->GetWindow(0x1d4c9);
        if (strId > -1) {
            char16_t local[64];
            wcsncpy(local, UIMain_GetString(strId), 0x3f);
            UIMain_GetString(0x13f);
            wcscat(gUIMainConvertBuffer, local);
        } else {
            UIMain_GetString(0x13f);
        }
        options_callee::WindowSetText(window, gUIMainConvertBuffer);
        break;
    }
    case 1: {
        int32_t factor = (static_cast<int32_t>(s.mRenderer) == 0) ? 1 : 2;
        gPlayerSettings.SetGraphicsDetail(
            static_cast<CPlayerSettings::eDETAIL>(static_cast<int32_t>(static_cast<uint32_t>(value) * factor) / 100));
        int32_t strId;
        switch (static_cast<int32_t>(s.mDetail)) {
        case 0: strId = 0x150; break;
        case 1: strId = 0x151; break;
        case 2: strId = 0x152; break;
        default: strId = 0; break;
        }
        InterfaceWindow* window = gSetOnOffRoot->GetWindow(0x1d4d6);
        window = window->GetWindow(0x1d4c9);
        if (strId > -1) {
            char16_t local[64];
            wcsncpy(local, UIMain_GetString(strId), 0x3f);
            UIMain_GetString(0x13e);
            wcscat(gUIMainConvertBuffer, local);
        } else {
            UIMain_GetString(0x13e);
        }
        options_callee::WindowSetText(window, gUIMainConvertBuffer);
        break;
    }
    case SOUNDQ_INDEX: {
        gPlayerSettings.SetSoundQuality(value);
        InterfaceWindow* window = gSetOnOffRoot->GetWindow(id);
        char16_t local[10];
        if (value < 0)
            swprintf(local, 10, gOptionsPanelNumberFormats, UIMain_GetString(0x14e));
        else
            swprintf(local, 10, gOptionsPanelNumberFormats + 5, value);
        UIMain_GetString(0x140);
        wcscat(gUIMainConvertBuffer, local);
        InterfaceWindow* label = window->GetWindow(0x1d4c9);
        options_callee::WindowSetText(label, gUIMainConvertBuffer);
        if (value >= 0) options_callee::WindowSetValue(window, value);
        break;
    }
    case SFX_INDEX:
        s.mSfx.mVolume = value;
        value = Slider(id, value, 0x141, s.mSfx.mOn);
        SOUND_SetSfxVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case MUSIC_INDEX:
        s.mMusic.mVolume = value;
        value = Slider(id, value, 0x142, s.mMusic.mOn);
        SOUND_SetMusicVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case SPEECH_INDEX:
        s.mSpeech.mVolume = value;
        value = Slider(id, value, 0x143, s.mSpeech.mOn);
        SOUND_SetSpeechVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case MOVIE_INDEX:
        s.mMovie.mVolume = value;
        Slider(id, value, 0x144, s.mMovie.mOn);
        break;
    default:
        break;
    }
}

// 0x101a96f8 (236 bytes): the sound sliders' scroll start; ids 0x1d4d8 to 0x1d4e0.
void StartScroll(int32_t id, int32_t value) {
    CPlayerSettings::Settings& s = gPlayerSettings.mCurrent;
    uint32_t index = static_cast<uint32_t>(id) - 0x1d4d8u;
    if (index > 8) {
        gOptionsPanelF0C = 0;
        return;
    }
    switch (index) {
    case START_SFX:
        if (s.mSfx.mOn) SOUND_SetSfxVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case START_MUSIC:
        if (s.mMusic.mOn) SOUND_SetMusicVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case START_SPEECH:
        if (s.mSpeech.mOn) SOUND_SetSpeechVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case 6:
    case 8:
        break;
    default:
        gOptionsPanelF0C = 0;
        break;
    }
}

}  // namespace options_panel
