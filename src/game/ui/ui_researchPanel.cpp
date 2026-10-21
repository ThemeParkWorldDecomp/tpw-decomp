// ui_researchPanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>

// Unit v5184: the research panel's speed lights and bars, main app (ui_ridepanel.cpp on the Mac).
//   SpeedTimerFudge 0x101b35d4 (528 bytes), PercentComplete 0x101b3398 (532 bytes), ResearchSpeed 0x101b381c (1384 bytes).

uint16_t gResearchProjectNone = 0;   // 0x102aca98 (name invented): the project id that means "no project"

// 0x101b3398: the group's bar shows the percentage done, scaled to 1024. Groups outside 0 to 4 draw nothing.
void PercentComplete(int32_t group) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    uint32_t done = 100u - lab->GetRemainingPoints(static_cast<ResearchCat>(group));
    int32_t id;
    switch (group) {
        case 0: id = 0xd2d01; break;
        case 1: id = 0xd2d08; break;
        case 2: id = 0xd2d09; break;
        case 3: id = 0xd2d0a; break;
        case 4: id = 0xd2d07; break;
        default: return;
    }
    InterfaceWindow* bar = gResearchPanel->GetWindow(id)->GetWindow(0xd2d05);
    bar->SetValue(static_cast<int32_t>(done << 10) / 100);
}

// The window ids of the five groups' panels, in the group order 0 to 4 (rides, shops, sideshows, features, all but the order is the Mac's):
// group 0 0xd2d01, 1 0xd2d08, 2 0xd2d09, 3 0xd2d0a, 4 0xd2d07.

// 0x101b35d4: the window's animation state machine. extra 3 is the state, extra 0 the frame word, extra 1 the start tick, extra 2 the level.
void SpeedTimerFudge(InterfaceWindow* window, int32_t speed) {
    if (speed == 0) {
        int32_t state = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(3)));
        if (state == 3 || state == 1) {
            window->setExtraParam(3, reinterpret_cast<void*>(static_cast<intptr_t>(2)));
            window->GetWindow(0xd2d06)->SetFrame(0x20);
            window->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(0x200000)));
        }
    } else {
        int32_t level = static_cast<int32_t>(static_cast<uint32_t>(speed + 0x14) << 16) / 100;
        int32_t state = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(3)));
        if (state == 0 || state == 2) {
            window->setExtraParam(1, reinterpret_cast<void*>(static_cast<uintptr_t>(IveGotTicks())));
            window->setExtraParam(3, reinterpret_cast<void*>(static_cast<intptr_t>(3)));
            window->setExtraParam(0, reinterpret_cast<void*>(static_cast<intptr_t>(0x300000)));
            window->GetWindow(0xd2d06)->SetFrame(0x30);
        }
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(level)));
    }
}

namespace {

// The group's speed: 0 when its current project is the "none" id.
uint32_t SpeedOf(int32_t group) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    if (lab->GetCurrentProject(static_cast<ResearchCat>(group)).mIndex == gResearchProjectNone) return 0;
    return lab->GetResearchSpeed(static_cast<ResearchCat>(group));
}

void Fudge(int32_t id, uint32_t speed) {
    SpeedTimerFudge(gResearchPanel->GetWindow(id), static_cast<int32_t>(speed));
}

}  // namespace

// 0x101b381c: group -1 refreshes all five lights (order 0, 4, 1, 2, 3), another group only its own (none for a group outside 0 to 4,
// though its speed is still asked); then the sound parameter 0x12 gets the mean of the five speeds.
void ResearchSpeed(int32_t group) {
    if (group == -1) {
        Fudge(0xd2d01, SpeedOf(0));
        Fudge(0xd2d07, SpeedOf(4));
        Fudge(0xd2d08, SpeedOf(1));
        Fudge(0xd2d09, SpeedOf(2));
        Fudge(0xd2d0a, SpeedOf(3));
    } else {
        uint32_t speed = SpeedOf(group);
        switch (group) {
            case 0: Fudge(0xd2d01, speed); break;
            case 1: Fudge(0xd2d08, speed); break;
            case 2: Fudge(0xd2d09, speed); break;
            case 3: Fudge(0xd2d0a, speed); break;
            case 4: Fudge(0xd2d07, speed); break;
            default: break;
        }
    }
    uint32_t s3 = SpeedOf(3);
    uint32_t s2 = SpeedOf(2);
    uint32_t s1 = SpeedOf(1);
    uint32_t s0 = SpeedOf(0);
    uint32_t s4 = SpeedOf(4);
    uint32_t sum = s0 + s4 + s1 + s2 + s3;
    SOUND_SetParamValue(gResearchSoundA, 0x12, static_cast<uint32_t>(static_cast<int32_t>(sum) / 5));
}

// NameWindow 0x101b3da8 (564 bytes): the research panel's text for one category's current project.

void NameWindow(int32_t group) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    ResearchCat category = static_cast<ResearchCat>(group);
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> project = lab->GetCurrentProject(category);
    InterfaceWindow* window;
    int32_t id;
    switch (group) {
        case 0: id = 0xd2d01; break;
        case 1: id = 0xd2d08; break;
        case 2: id = 0xd2d09; break;
        case 3: id = 0xd2d0a; break;
        case 4: id = 0xd2d07; break;
        default: id = 0; break;
    }
    if (id != 0)
        window = gResearchPanel->GetWindow(id)->GetWindow(0xd2d05);
    else
        window = nullptr;
    if (gRideDatabase.GetRideInfo(project) != nullptr) {
        char16_t name[0x42];
        char16_t text[0x80];
        gRideDatabase.GetTypeName(project.mIndex, name, 0x42);
        int32_t cost = lab->GetEstimatedCostForCurrentProject(category);
        TbIntegerVTag costTag(13, cost);
        TbUniStringVTag nameTag(14, name);
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(text, 0x7f, 0x1be, &nameTag, &costTag);
        window->SetClientDrawText(text);
    } else {
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0xb9)));
        ResearchSpeed(group);
    }
}

// Unit v5097: the research panel's scroll handling (DoScroll 0x101b42d0), InitAll (0x101b4000) and ResearchPanelMsg (0x101b45e0).

// The Mac's UIMain_UISound answers the sound handle in r3 (the header declares it void): the same symbol, declared with the answer.
int32_t UIMain_UISoundHandle(int32_t code) __asm__("__Z13UIMain_UISoundi");

int32_t gResearchScrollSound;   // 0x10395960 (name invented)

void InitAll() {
    NameWindow(0);
    NameWindow(4);
    NameWindow(1);
    NameWindow(2);
    NameWindow(3);
    PercentComplete(0);
    PercentComplete(4);
    PercentComplete(1);
    PercentComplete(2);
    PercentComplete(3);
    CResearchLab* lab = gGameWorld->GetResearchLab();
    InterfaceWindow* window = gResearchPanel->GetWindow(0xd2d0e);
    if (window) window->SetValue(static_cast<int32_t>(lab->GetEffort(0)));
    window = gResearchPanel->GetWindow(0xd2d0f);
    if (window) window->SetValue(static_cast<int32_t>(lab->GetEffort(4)));
    window = gResearchPanel->GetWindow(0xd2d0d);
    if (window) window->SetValue(static_cast<int32_t>(lab->GetEffort(1)));
    window = gResearchPanel->GetWindow(0xd2d0c);
    if (window) window->SetValue(static_cast<int32_t>(lab->GetEffort(2)));
    window = gResearchPanel->GetWindow(0xd2d0b);
    if (window) window->SetValue(static_cast<int32_t>(lab->GetEffort(3)));
    gResearchPanelMade = 0;
    ResearchSpeed(-1);
}

// TankButton 0x101b4224 (120 bytes): step the category's project (flag: back), then redraw its name and percentage.
void TankButton(int32_t which, InterfaceWindow* window, int32_t flag) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    if (flag != 0)
        lab->PreviousProject(static_cast<ResearchCat>(which));
    else
        lab->NextProject(static_cast<ResearchCat>(which));
    NameWindow(which);
    PercentComplete(which);
}

void DoScroll(int32_t id, int32_t value) {
    InterfaceWindow* window = gResearchPanel->GetWindow(id);
    CResearchLab* lab = gGameWorld->GetResearchLab();
    switch (id) {
    case 0xd2d0e:
        lab->SetEffort(0, static_cast<uint32_t>(value));
        if (gResearchPanelMade == 0) ResearchSpeed(-1);
        break;
    case 0xd2d0f:
        lab->SetEffort(4, static_cast<uint32_t>(value));
        if (gResearchPanelMade == 0) ResearchSpeed(-1);
        break;
    case 0xd2d0d:
        lab->SetEffort(1, static_cast<uint32_t>(value));
        if (gResearchPanelMade == 0) ResearchSpeed(-1);
        break;
    case 0xd2d0c:
        lab->SetEffort(2, static_cast<uint32_t>(value));
        if (gResearchPanelMade == 0) ResearchSpeed(-1);
        break;
    case 0xd2d0b:
        lab->SetEffort(3, static_cast<uint32_t>(value));
        if (gResearchPanelMade == 0) ResearchSpeed(-1);
        break;
    case 0xd2d10: {
        if (value > gResearchGaugeValue) {
            window->SetValue(gResearchGaugeValue);
            value = gResearchGaugeValue;
        }
        int32_t share = (value * 30) / gResearchGaugeValue;
        int32_t load = share + 0x46;
        lab->SetWorkLoad(static_cast<uint32_t>(load));
        if (gResearchPanelMade == 0) ResearchSpeed(-1);
        PercentComplete(0);
        PercentComplete(4);
        PercentComplete(1);
        PercentComplete(2);
        PercentComplete(3);
        int32_t frame = static_cast<int32_t>(static_cast<uint32_t>(load - 0x46) << 1) / 7;
        InterfaceWindow* bar = gResearchPanel->GetWindow(0xd2d10);
        bar->GetWindow(0xd2d11)->SetFrame(frame);
        int32_t volume = (value * 100) / 1024;
        SOUND_SetParamValue(gResearchSoundA, 0xc, static_cast<uint32_t>(volume));
        SOUND_SetParamValue(gResearchSoundB, 0xc, static_cast<uint32_t>(volume));
        SOUND_SetParamValue(gResearchSoundA, 0xd, static_cast<uint32_t>(volume));
        SOUND_SetParamValue(gResearchSoundB, 0xd, static_cast<uint32_t>(volume));
        break;
    }
    default:
        break;
    }
    if (id != 0xd2d10) {
        SOUND_SetParamValue(gResearchScrollSound, 0xb, static_cast<uint32_t>(value));
        SOUND_SetParamValue(gResearchScrollSound, 0xa, static_cast<uint32_t>(value));
    }
}

int32_t ResearchPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x1e:
        if (static_cast<uint32_t>(gResearchCurrentAbility) !=
            static_cast<uint32_t>(gGameWorld->GetParkAnalyser()->CurrentResearcherAbility())) {
            gResearchCurrentAbility = gGameWorld->GetParkAnalyser()->CurrentResearcherAbility();
            CResearchLab* lab = gGameWorld->GetResearchLab();
            InterfaceWindow* bar = gResearchPanel->GetWindow(0xd2d10);
            int32_t ability = gResearchCurrentAbility;
            int32_t third = (100 - ability) / 3;
            int32_t gauge = static_cast<int32_t>(static_cast<uint32_t>(ability + third) << 10) / gResearchMaxAbility;
            int32_t product = static_cast<int32_t>(static_cast<uint32_t>(lab->mF1460 - 0x46) * static_cast<uint32_t>(gauge));
            int32_t level = product / 30;
            bar->GetWindow(0xd2d12)->SetValue(gauge);
            gResearchGaugeValue = gauge;
            gResearchPanel->GetWindow(0xd2d10)->SetValue(level);
        }
        break;
    case 5:
        RemoveResearchPanel();
        return 1;
    case 0x15:
        if (window->setAnimTimer(0x80080, 1000) != 0) window->startAnimTimer(0x80080);
        UIMain_SetTitleText(window, 0xd2d00, const_cast<char16_t*>(UIMain_GetString(0xb8)));
        break;
    case 0x10:
        PercentComplete(0);
        PercentComplete(4);
        PercentComplete(1);
        PercentComplete(2);
        PercentComplete(3);
        break;
    case 0x14:
        gResearchPanel = nullptr;
        SOUND_StopSoundEffect(gResearchSoundA);
        SOUND_StopSoundEffect(gResearchSoundB);
        if (gResearchScrollSound != 0) SOUND_StopSoundEffect(gResearchScrollSound);
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(200));
        break;
    case 0x100:
        if (a < 0 && a >= -2) {
            RemoveResearchPanel();
            return 0;
        }
        break;
    case 0x800:
        DoScroll(a, b);
        break;
    case 0x801:
        if (a != 0xd2d10) {
            gResearchScrollSound = UIMain_UISoundHandle(0x9c);
            SOUND_SetParamValue(gResearchScrollSound, 0xb, static_cast<uint32_t>(b));
            SOUND_SetParamValue(gResearchScrollSound, 0xa, static_cast<uint32_t>(b));
        }
        break;
    case 0x802:
        if (a != 0xd2d10) {
            SOUND_FadeSoundEffect(gResearchScrollSound);
            gResearchScrollSound = 0;
        }
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// Unit v51121: the small functions of ui_ridepanel.cpp (RideStatusPanel, ShopStatusPanel), ui_StaffCostsPanel.cpp, the research panel and
// the tank panel message function.

// 0x101b495c.
int32_t StoreTankMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gResearchEffortBar = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}

// AnimatingPumpMsg 0x101b49d0: the window handler of an animated pump. On message 0x1e it steps a 16.16 frame counter
// (extra param 0) by twice the speed (extra param 2) in the mode of extra param 3, wraps it per mode, shows the frame on
// the child window 0xd2d06 and stores the counter and the tick (extra param 1). Then the default handler runs.

int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

static int32_t Word(void* p) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(p)); }
static void* Ptr(int32_t v) { return reinterpret_cast<void*>(static_cast<intptr_t>(v)); }

int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        int32_t counter = Word(window->getExtraParam(0));
        window->getExtraParam(1);
        int32_t speed = Word(window->getExtraParam(2));
        int32_t mode = Word(window->getExtraParam(3));
        if (mode != 0) {
            int32_t frame = 0;  // the original leaves it unset for a mode above 3
            if (mode == 1) {
                counter = static_cast<int32_t>(static_cast<uint32_t>(counter) + (static_cast<uint32_t>(speed) << 1));
                frame = counter >> 16;
                if (frame > 0x1f) {
                    frame = 0;
                    counter = 0;
                }
            } else if (mode == 2) {
                counter = static_cast<int32_t>(static_cast<uint32_t>(counter) + (static_cast<uint32_t>(speed) << 1));
                frame = counter >> 16;
                if (frame > 0x2f) {
                    window->setExtraParam(3, Ptr(0));
                    frame = 0x30;
                    counter = 0x300000;
                }
            } else if (mode == 3) {
                counter = static_cast<int32_t>(static_cast<uint32_t>(counter) + (static_cast<uint32_t>(speed) << 1));
                frame = counter >> 16;
                if (frame > 0x3f) {
                    window->setExtraParam(3, Ptr(1));
                    frame = 0;
                    counter = 0;
                }
            }
            window->GetWindow(0xd2d06)->SetFrame(frame);
            window->setExtraParam(0, Ptr(counter));
            window->setExtraParam(1, Ptr(static_cast<int32_t>(IveGotTicks())));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// The five bar message functions of the research panel: a button press (0x100) tells TankButton which bar it was and
// whether it was the button 0xd2d03, then every message goes on to AnimatingPumpMsg, whose answer is returned.

void TankButton(int32_t which, InterfaceWindow* window, int32_t flag);                          // 0x101b4224
static int32_t BarMsg(int32_t which, InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) TankButton(which, window, a == 0xd2d03 ? 1 : 0);
    return AnimatingPumpMsg(window, message, a, b);
}

int32_t RideMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(0, window, message, a, b); }
int32_t UpgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(4, window, message, a, b); }
int32_t ShopMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(1, window, message, a, b); }
int32_t ShowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(2, window, message, a, b); }
int32_t FeatureMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(3, window, message, a, b); }

// CreateResearchPanel 0x101b4f74 (2888 bytes): builds the research panel (script 0x102ac35a, message function ResearchPanelMsg), hooks up its
// effort gauge and five bars, and sets the gauge from the park analyser's researcher ability and the lab's work load.

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
int32_t UIMain_UISoundHandle_2(int32_t code) __asm__("__Z14UIMain_UISoundi");

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
    gResearchSoundA = UIMain_UISoundHandle_2(0x9a);
    gResearchSoundB = UIMain_UISoundHandle_2(0x9b);

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

void RemoveResearchPanel() { UI_SendMessage(gResearchPanel, 4, 0, 0); }       // 0x101b5ae8

// 0x101b5b4c
void ResearchPanelCategoryComplete(int32_t category) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    if (!lab->IsCategoryActive(static_cast<ResearchCat>(category))) lab->NextProject(static_cast<ResearchCat>(category));
    if (gResearchPanel != nullptr) {
        NameWindow(category);
        PercentComplete(category);
    }
}

// Unit v5231: the ride list panel's row functions. RideListLCHandler 0x101b7d94 (the four category buttons), RideListLCEntryHandler
// 0x101b7f30 (the row text colour), RideListLCFormat 0x101b811c (the row window) and RideListLCPostFormat 0x101b827c (its draw objects).
// The names LCFormat and LCPostFormat exist at other addresses, so these carry the panel's prefix.

uint32_t gRideListFontIndex = 7;   // 0x102aced8
int32_t gRideListRowHeight;        // 0x10395984

int32_t RideListLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        int32_t type = -1;
        int32_t string = 0;
        if (a == 0x1fc) {
            type = 3;
            string = 0x7a;
        } else if (a < 0x1fc) {
            if (a == 0x1fa) {
                type = 2;
                string = 0x79;
            } else if (a > 0x1fa) {
                type = 0;
                string = 0x77;
            }
        } else if (a < 0x1fe) {
            type = 1;
            string = 0x78;
        }
        if (type >= 0) {
            ChangeInfoWindow(static_cast<UIObjType>(type));
            FillRideList(static_cast<UIObjType>(type), 1);
            UIMain_SetTitleText(gRideListPanel, 0x1fe, const_cast<char16_t*>(UIMain_GetString(string)));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t RideListLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        if (static_cast<int16_t>(b) < 0) {
            window->SetFontColour(0xe7, 0xef, 0x4b, 0xff);
        } else {
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id{static_cast<uint16_t>(b)};
            ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(id, 0);
            int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
            RideInfo* ride = gRideDatabase.GetRideInfo(id);
            CGoldenTicketControl* tickets = reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<char*>(gGameWorld) + 0x2c0);
            if (ride->Value(kRideSlot_GoldenTicketCost).mInteger != 0 && !tickets->HasBeenBought(id)) {   // the ride's slot 46, +0xc4
                if (!tickets->CanIAfford(id)) {
                    window->SetFontColour(0xff, 0, 0, 0xff);
                } else {
                    window->SetFontColour(0xff, 0xe0, 0, 0xff);
                }
            } else if (control->mUpgradeCost > balance) {
                window->SetFontColour(0xff, 0, 0, 0xff);
            } else {
                window->SetFontColour(0xff, 0xff, 0xff, 0xff);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

InterfaceWindow* RideListLCFormat() {
    if (gRideListRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(gRideListFontIndex);
        int16_t height = static_cast<int16_t>(font->GetHeight());   // the Mac calls the font's virtual at +0xc of the table at +0x10
        gRideListRowHeight = height * 0x600 / gUIRealScreenHeight + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(gRideListRowHeight));
    if (window != nullptr) {
        window->SetCallbackFunction(RideListLCEntryHandler);
        InterfaceWindow* created = window->CreateObject(1, 1, 1, 100, 0, 200, 0x32);
        created->SetCallbackFunction(RideListLCEntryHandler);
        window->CreateObject(6, 1, 2, 100, 0, 200, 0x32);
    }
    return window;
}

void RideListLCPostFormat(InterfaceWindow* window) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(memory)));
    window->SetFont(GetFontTableEntry(gRideListFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    InterfaceWindow* child = window->GetChild(0);
    memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) new (memory) InterfaceCashAndGTickTextObject(nullptr);   // 0x1018e190
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceCashAndGTickTextObject*>(memory)));
    child->SetFont(GetFontTableEntry(gRideListFontIndex));
    child->SetFontJust(2, 1);
    lender_callee::WindowSetValue(child, 0);
    InterfaceWindow* next = child->GetNext();
    next->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("i_boxtick")));
    lender_callee::WindowSetValue(next, 0);
}
