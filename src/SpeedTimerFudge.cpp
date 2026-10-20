// Unit v5184: the research panel's speed lights and bars, main app (ui_ridepanel.cpp on the Mac).
//   SpeedTimerFudge 0x101b35d4 (528 bytes), PercentComplete 0x101b3398 (532 bytes), ResearchSpeed 0x101b381c (1384 bytes).
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"

uint16_t gResearchProjectNone = 0;   // 0x102aca98 (name invented): the project id that means "no project"

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
