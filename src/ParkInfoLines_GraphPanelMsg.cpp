#include <cstdint>
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

namespace {

// The five histories the flag bits select (analyser offsets 0x20cc0, 0x20f10, 0x21160, 0x205d0, 0x213b0); the
// largest value over the selected ones, 0 when none is selected.
int32_t ParkStatusMaxValue() {
    int32_t value = 0;
    char* analyser = reinterpret_cast<char*>(gGameWorld->GetParkAnalyser());
    if (gParkStatusFlags & 1)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x20cc0), value, gParkStatusRange);
    if (gParkStatusFlags & 2)
        MaxAnalyserValue(reinterpret_cast<CHistory<float, 144>*>(analyser + 0x20f10), value, gParkStatusRange);
    if (gParkStatusFlags & 4)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x21160), value, gParkStatusRange);
    if (gParkStatusFlags & 8)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x205d0), value, gParkStatusRange);
    if (gParkStatusFlags & 0x10)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x213b0), value, gParkStatusRange);
    return value;
}

void ParkStatusXRange() {
    switch (gParkStatusRange) {
    case 0: gParkStatusGrapher.SetXRange(0, 0xb, 0xc); break;
    case 1: gParkStatusGrapher.SetXRange(0, 0x23, 0x24); break;
    case 2: gParkStatusGrapher.SetXRange(0, 0x8f, 0x90); break;
    default: break;
    }
}

void ParkStatusRedraw(InterfaceWindow* window) {
    int32_t value = ParkStatusMaxValue();
    gParkStatusGrapher.SetYRange(0, value, 10);
    DrawMeGraph(window, gParkStatusGrapher, reinterpret_cast<void (*)(CUIGrapher&)>(gParkInfoLines));
}

}  // namespace

// 0x101ac044
int32_t GraphPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CoolRect rect;
    if (message == 0x100) {
        uint32_t bit = 0;
        switch (a) {
        case 0x4a68bc: bit = 1; break;
        case 0x4a68bd: bit = 2; break;
        case 0x4a68be: bit = 4; break;
        case 0x4a68bf: bit = 8; break;
        case 0x4a68c0: bit = 0x10; break;
        default: break;
        }
        if (bit != 0) {
            if (b == 1)
                gParkStatusFlags |= bit;
            else
                gParkStatusFlags &= ~bit;
            ParkStatusRedraw(window);
        }
    } else if (message == 0x15) {
        window->GetClientRect(&rect);
        int32_t value = ParkStatusMaxValue();
        SetupGraph(gParkStatusGrapher, rect, 0, value, 10, 0, 0xb, 0xc);
        ParkStatusXRange();
        DrawMeGraph(window, gParkStatusGrapher, reinterpret_cast<void (*)(CUIGrapher&)>(gParkInfoLines));
    } else if (message == 0x101) {
        bool hit = false;
        switch (a) {
        case 0x4a68c2: gParkStatusRange = 2; hit = true; break;
        case 0x4a68c3: gParkStatusRange = 0; hit = true; break;
        case 0x4a68c4: gParkStatusRange = 1; hit = true; break;
        default: break;
        }
        if (hit && b == 1) {
            ParkStatusXRange();
            ParkStatusRedraw(window);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
