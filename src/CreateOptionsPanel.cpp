// Unit v5634: CreateOptionsPanel 0x101a9fb8 (6432 bytes), the options panel of the game (ui_parkStatus.cpp's neighbourhood on the
// Mac). Translated from the listing; the Ghidra draft had the video card test's comparison the wrong way round.
#include <cstdint>

#include "Advisor.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/msl.h"
#include "tb_ddraw.h"
#include "tb_texturecache.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

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
