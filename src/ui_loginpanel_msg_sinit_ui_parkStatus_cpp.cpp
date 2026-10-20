// ui_parkStatus.cpp: UI_ParkStatusPanelDateChange 0x101ada64 and the static initialiser __sinit_ui_parkStatus_cpp 0x101adbe4.
#include <cstdint>

#include "FinanceGraphPanelMsg.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LbPalette.h"
#include "World.h"
#include "ui_main.h"

CUIGrapher gParkStatusGrapher;                  // 0x10394a28
uint8_t gParkStatusColours[5][5];               // 0x10394a6c
int32_t gParkStatusRange;                       // 0x10394a88
InterfaceWindow* gParkStatusPanel;              // 0x10394a8c
uint32_t gParkStatusFlags = 8;                  // 0x102abbac
uint8_t gParkInfoLines[16];                     // 0x10267ca8 (its Mac initial contents are not modelled)

// 0x101ada64: the Y range of the graph is the largest of the selected histories; the argument is unused.
void UI_ParkStatusPanelDateChange(int32_t month) {
    (void)month;
    if (gParkStatusPanel == nullptr) return;
    int32_t maxValue = 0;
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    uint32_t flags = gParkStatusFlags;
    if (flags & 1) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x20cc0), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 2) MaxAnalyserValue(reinterpret_cast<CHistory<float, 144>*>(analyser + 0x20f10), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 4) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x21160), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 8) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x205d0), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 0x10) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x213b0), maxValue, gParkStatusRange);
    gParkStatusGrapher.SetYRange(0, maxValue, 10);
    InterfaceWindow* window = gParkStatusPanel->GetWindow(0x4a68bb);
    DrawMeGraph(window, gParkStatusGrapher, reinterpret_cast<void (*)(CUIGrapher&)>(gParkInfoLines));
}

static void SetColour(uint8_t* c, uint8_t r, uint8_t g, uint8_t b) {
    c[1] = r;
    c[2] = g;
    c[3] = b;
    c[0] = 0xff;
    c[4] = LbPalette_FindColour(&_lbGlobalPalette, r, g, b);
}

// 0x101adbe4: five colours (yellow, magenta, red, blue, orange) looked up in the global palette, then the grapher.
void __sinit_ui_parkStatus_cpp() {
    SetColour(gParkStatusColours[4], 0xff, 0xff, 0);
    SetColour(gParkStatusColours[3], 0xff, 0, 0xff);
    SetColour(gParkStatusColours[2], 0xff, 0, 0);
    SetColour(gParkStatusColours[1], 0, 0xbd, 0xec);
    SetColour(gParkStatusColours[0], 0xff, 0x90, 0);
    CUIGrapher_Construct(&gParkStatusGrapher);
}
