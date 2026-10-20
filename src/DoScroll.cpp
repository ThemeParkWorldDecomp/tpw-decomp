// Unit v5097: the research panel's scroll handling (DoScroll 0x101b42d0), InitAll (0x101b4000) and ResearchPanelMsg (0x101b45e0).
#include <cstdint>

#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

// The Mac's UIMain_UISound answers the sound handle in r3 (the header declares it void): the same symbol, declared with the answer.
int32_t UIMain_UISoundHandle(int32_t code) __asm__("__Z13UIMain_UISoundi");

int32_t gResearchScrollSound;   // 0x10395960 (name invented)

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
