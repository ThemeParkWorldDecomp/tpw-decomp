// CreateResearchPanel 0x101b4f74 (2888 bytes): builds the research panel (script 0x102ac35a, message function ResearchPanelMsg), hooks up its
// effort gauge and five bars, and sets the gauge from the park analyser's researcher ability and the lab's work load.
#include <cstdint>
#include <new>

#include "ui_main.h"
#include "InterfaceWindow.h"
#include "InterfaceScrollBar.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "FS_Online.h"
#include "World.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "AdvisorScoring.h"
#include "WindowScripts.inc"

// The globals of the panel (BSS at 0x10395964 to 0x10395980, data at 0x102aca94 and 0x102ac35a).
InterfaceWindow* gResearchPanel = nullptr;
InterfaceWindow* gResearchEffortBar = nullptr;
int32_t gResearchMaxAbility = 0;
int32_t gResearchCurrentAbility = 0;
int32_t gResearchGaugeValue = 0;
uint8_t gResearchPanelMade = 0;
int32_t gResearchSoundA = 0;
int32_t gResearchSoundB = 0;
uint32_t gResearchFontIndex = 6;
int16_t gResearchPanelScript[] = {TPW_WINDOW_SCRIPT_102AC35A};   // 0x102ac35a: the image's words (generated, devtools/gen_game_tables.py)

// UIMain_UISound is declared void, but this function stores what the call leaves in r3 (the sound's handle): the same symbol, read as an int.
int32_t UIMain_UISoundHandle(int32_t code) __asm__("__Z14UIMain_UISoundi");

namespace {

// The halfword at 0x102aca9a: the "no thing" list end (0).
constexpr uint16_t kNoThing = 0;

// "effortguage.wct" (0x1024e3fe) and, 0x10 bytes on, "bluebar.wct" (0x1024e40e).
char kResearchMeshes[] = "effortguage.wct\0bluebar.wct";

const int32_t kGaugeWindow = 0xd2d12;
const int32_t kBarWindow = 0xd2d05;
const int32_t kFrameWindow = 0xd2d06;

// One of the five bars: its handler, a gradient behind a sprite text with no text, in the font of the table.
void SetUpBar(InterfaceWindow* bar, InterfaceWindowCallback callback) {
    bar->SetCallbackFunction(callback);
    bar->setExtraParam(0, reinterpret_cast<void*>(0x30));
    bar->setExtraParam(3, nullptr);
    bar->GetWindow(kFrameWindow)->SetFrame(0x30);
    InterfaceWindow* inner = bar->GetWindow(kBarWindow);
    InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(kResearchMeshes + 0x10);
    inner->SetDrawObject(gradient);
    InterfaceTextObjectSprite* text = new InterfaceTextObjectSprite(nullptr);
    inner->SetClientDrawObject(text);
    inner->SetFontJust(1, 1);
    inner->SetFontWrap(1, 1);
    inner->SetFont(GetFontTableEntry(gResearchFontIndex));
    inner->SetFontColour(0xe0, 0x56, 0x56, 0xff);
    inner->SetClientDrawText(nullptr);
}

void ShowDialog(int32_t stringId) {
    dlgboxparams params;
    params.mText = UIMain_GetString(stringId);
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    params.mValueC = 0;
    UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(0), nullptr, static_cast<dlgbutton>(0), nullptr, false);
}

void SetBarRange(InterfaceWindow* window, int32_t id) {
    InterfaceWindow* bar = window->GetWindow(id);
    if (bar) static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 100);
}

}  // namespace

void CreateResearchPanel() {
    if (MainMenuUp() != 0) return;
    if (gResearchPanel != nullptr) return;
    UIMain_CheckPanel();
    if (TheGameType().mType == kGameTypeEasy) {
        ShowDialog(0x1d4);
        return;
    }
    if (gGameWorld->mF1DA742 == kNoThing) {
        ShowDialog(0x1d3);
        return;
    }
    gResearchPanel = gGameWindow->Build(gResearchPanelScript, ResearchPanelMsg);
    if (gResearchPanel == nullptr) return;
    InterfaceWindow* panel = gResearchPanel;
    UIMain_SetPanel(panel);
    gResearchPanelMade = 1;
    gResearchSoundA = UIMain_UISoundHandle(0x9a);
    gResearchSoundB = UIMain_UISoundHandle(0x9b);

    gResearchEffortBar = panel->GetWindow(0xd2d10);
    if (gResearchEffortBar != nullptr) {
        gResearchEffortBar->SetCallbackFunction(StoreTankMsg);
        static_cast<InterfaceScrollBar*>(gResearchEffortBar)->SetMinMax(0, 0x3ff);
        InterfaceDrawEngineVGradient* gradient = new InterfaceDrawEngineVGradient(kResearchMeshes);
        gResearchEffortBar->GetWindow(kGaugeWindow)->SetClientDrawObject(gradient);
        gResearchEffortBar->GetWindow(kGaugeWindow)->SetValue(500);
    }
    SetBarRange(panel, 0xd2d0e);
    SetBarRange(panel, 0xd2d0f);
    SetBarRange(panel, 0xd2d0d);
    SetBarRange(panel, 0xd2d0c);
    SetBarRange(panel, 0xd2d0b);

    InterfaceWindow* bar = panel->GetWindow(0xd2d01);
    if (bar) SetUpBar(bar, RideMsg);
    bar = panel->GetWindow(0xd2d07);
    if (bar) SetUpBar(bar, UpgMsg);
    bar = panel->GetWindow(0xd2d08);
    if (bar) SetUpBar(bar, ShopMsg);
    bar = panel->GetWindow(0xd2d09);
    if (bar) SetUpBar(bar, ShowMsg);
    bar = panel->GetWindow(0xd2d0a);
    if (bar) SetUpBar(bar, FeatureMsg);

    gResearchMaxAbility = gGameWorld->GetParkAnalyser()->MaxResearcherAbility();
    gResearchCurrentAbility = gGameWorld->GetParkAnalyser()->CurrentResearcherAbility();
    CResearchLab* lab = gGameWorld->GetResearchLab();
    InterfaceWindow* gauge = gResearchPanel->GetWindow(0xd2d10);
    int32_t current = gResearchCurrentAbility;
    // (current + (100 - current) / 3) * 1024 / max; the load less 70 times that, over 30.
    uint32_t scaled = static_cast<uint32_t>(current + (100 - current) / 3) << 10;
    int32_t value = static_cast<int32_t>(scaled) / gResearchMaxAbility;
    int32_t product = static_cast<int32_t>(static_cast<uint32_t>(lab->mF1460 - 0x46) * static_cast<uint32_t>(value));
    int32_t share = product / 30;
    gauge->GetWindow(kGaugeWindow)->SetValue(value);
    gResearchGaugeValue = value;
    gResearchPanel->GetWindow(0xd2d10)->SetValue(share);
    InitAll();
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(200));
}
