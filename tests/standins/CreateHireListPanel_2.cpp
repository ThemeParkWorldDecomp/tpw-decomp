// The weak stand-ins of the unit library tpw_unit_CreateHireListPanel_2 (cmake/units/CreateHireListPanel_2.cmake),
// as src/HirePanelLCHandler_CreateHireListPanel_2.cpp of before the regroup (v5L30b) defined them, unchanged. The
// regroup put each function into the src file of its original source file, where these defaults would sit next to
// the real definitions, so it dropped them; the unit library compiles them from here, so that it builds what it
// built before.

#include <algorithm>
#include <new>
#include "LoadPanelMsg.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "WindowScripts.inc"

// The hire list's row maker and the panel's message function: not translated yet (stand-ins).
__attribute__((weak)) InterfaceWindow* HireListLCFormat() { return nullptr; }   // 0x101a1798
__attribute__((weak)) int32_t HirePanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }   // 0x101a1314
