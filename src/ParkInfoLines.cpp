// The park status panel: ParkInfoLines 0x101abb90, ParkStatusPanelMsg 0x101abce4, DoDominantThoughts 0x101aba14.
#include <cstdint>

#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

int32_t gParkStatusLastTotal;    // 0x10394a18
int32_t gParkStatusLastInPark;   // 0x10394a1c

namespace {

// A thought number 1 to 0x16 is a frame of the picture; anything else hides it.
void ShowThought(InterfaceWindow* window, uint8_t thought) {
    if (thought != 0 && thought < 0x17) {
        window->SetValue(thought - 1);
    } else {
        window->Show(0);
    }
}

// (percent << 10) / 100 by the multiply the compiler made: mulhw with 0x51eb851f, srawi 5, plus the sign.
int32_t Scale(uint32_t percent) {
    int32_t value = static_cast<int32_t>(percent << 10);
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * value) >> 32);
    int32_t shifted = high >> 5;
    return shifted + static_cast<int32_t>(static_cast<uint32_t>(shifted) >> 31);
}

}  // namespace

// 0x101aba14
void DoDominantThoughts() {
    InterfaceWindow* panel = gParkStatusPanel->GetWindow(0x4a68d1);
    if (panel == nullptr) return;
    uint8_t first, second, third;
    gGameWorld->GetParkAnalyser()->GetThreeMostDominantThoughts(first, second, third);
    ShowThought(panel->GetWindow(0x4a68d4), first);
    ShowThought(panel->GetWindow(0x4a68d3), second);
    ShowThought(panel->GetWindow(0x4a68d2), third);
}

// 0x101abb90: the selected histories of the analyser, one line each.
void ParkInfoLines(CUIGrapher& grapher) {
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    if (gParkStatusFlags & 1)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x20cc0),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[3]), 1, gParkStatusRange);
    if (gParkStatusFlags & 2)
        GraphLine(grapher, reinterpret_cast<CHistory<float, 144>*>(analyser + 0x20f10),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[2]), 1, gParkStatusRange);
    if (gParkStatusFlags & 4)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x21160),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[1]), 1, gParkStatusRange);
    if (gParkStatusFlags & 8)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x205d0),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[4]), 1, gParkStatusRange);
    if (gParkStatusFlags & 0x10)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x213b0),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[0]), 1, gParkStatusRange);
}

// 0x101abce4
int32_t ParkStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x15:
        UIMain_SetTitleText(window, 0x4a68c5, const_cast<char16_t*>(UIMain_GetString(0x96)));
        break;
    case 0x10:
        if (a == 0x80083 && gParkStatusPanel != nullptr) {
            InterfaceWindow* panel = gParkStatusPanel->GetWindow(0x4a68c9);
            int32_t inPark = gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark();
            int32_t total = gGameWorld->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
            if (gParkStatusLastInPark != inPark) {
                gParkStatusLastInPark = inPark;
                panel->GetWindow(0x4a68cc)->SetValue(gParkStatusLastInPark);
            }
            if (gParkStatusLastTotal != total) {
                gParkStatusLastTotal = total;
                panel->GetWindow(0x4a68ca)->SetValue(gParkStatusLastTotal);
            }
            int32_t under = gGameWorld->GetParkAnalyser()->PercentageOfVisitorsWithHappinessUnder(0x21);
            int32_t over = gGameWorld->GetParkAnalyser()->PercentageOfVisitorsWithHappinessOver(0x42);
            gParkStatusPanel->GetWindow(0x4a68d0)->SetValue(Scale(static_cast<uint32_t>(under)));
            gParkStatusPanel->GetWindow(0x4a68cf)->SetValue(Scale(100u - static_cast<uint32_t>(over)));
            DoDominantThoughts();
        }
        break;
    case 5:
        RemoveParkStatusPanel();
        return 1;
    case 0x14:
        gParkStatusPanel = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(199));
        break;
    case 0x100:
        if (a == -1) {
            RemoveParkStatusPanel();
            return 0;
        }
        if (a == 0x4a68c6) {
            CreateAllKidsPanel(nullptr);
            return 0;
        }
        if (a == 0x4a68c8) {
            CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr);
            return 0;
        }
        if (a == 0x4a68c7) {
            CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr);
            return 0;
        }
        break;
    case 0x800:
        gGameWorld->GetBankAccount()->SetAdmissionFee(static_cast<uint32_t>(b));
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}
