// __sinit_ui_FinancePanel_cpp 0x101a0dbc: the finance panel's static initialiser.
#include <cstdint>

#include "FinanceGraphPanelMsg.h"
#include "LbPalette.h"

static void SetColour(uint8_t* c, uint8_t r, uint8_t g, uint8_t b) {
    c[1] = r;
    c[2] = g;
    c[3] = b;
    c[0] = 0xff;
    c[4] = LbPalette_FindColour(&_lbGlobalPalette, r, g, b);
}

void __sinit_ui_FinancePanel_cpp() {
    SetColour(gFinancePanelColours[7], 0xff, 0xff, 0);
    SetColour(gFinancePanelColours[6], 0xff, 0, 0xff);
    SetColour(gFinancePanelColours[5], 0xff, 0, 0);
    SetColour(gFinancePanelColours[4], 0, 0xbd, 0xec);
    SetColour(gFinancePanelColours[3], 0xff, 0x90, 0);
    SetColour(gFinancePanelColours[2], 0x90, 0xd3, 0);
    SetColour(gFinancePanelColours[1], 0xff, 0x75, 0x75);
    SetColour(gFinancePanelColours[0], 0x95, 0x58, 0xd2);
    CUIGrapher_Construct(&gFinanceGrapher);
}
