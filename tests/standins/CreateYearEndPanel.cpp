// The weak stand-ins of the unit library tpw_unit_CreateYearEndPanel (cmake/units/CreateYearEndPanel.cmake), as
// src/UpgradeStatusMsg_UpgradeStatusPanel_InitStatusWindow_2_CreateYearEndPanel.cpp of before the regroup (v5L30b)
// defined them, unchanged. The regroup put each function into the src file of its original source file, where these
// defaults would sit next to the real definitions, so it dropped them; the unit library compiles them from here, so
// that it builds what it built before.

#include <cstdint>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "WindowScripts.inc"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

// Untranslated (0x101c67b4): a weak stand-in so that the program links.
__attribute__((weak)) int32_t YearEndPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    (void)window; (void)message; (void)a; (void)b;
    return 0;
}
