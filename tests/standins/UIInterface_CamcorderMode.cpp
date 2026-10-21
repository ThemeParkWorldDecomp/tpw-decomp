// The weak stand-ins of the unit library tpw_unit_UIInterface_CamcorderMode
// (cmake/units/UIInterface_CamcorderMode.cmake), as src/UI_pump_UIInterface_CamcorderMode.cpp of before the regroup
// (v5L30b) defined them, unchanged. The regroup put each function into the src file of its original source file,
// where these defaults would sit next to the real definitions, so it dropped them; the unit library compiles them
// from here, so that it builds what it built before.

#include <cstdint>
#include <cstring>
#include <new>
#include "Debug.h"
#include "FS_Online.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "TbITable.h"
#include "Text.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "World.h"
#include "platform/msl.h"
#include "std_deque_errorthingy.h"
#include "ui_main.h"

// 0x101a7830 (16 bytes, untranslated): the main panel's research button. A weak stand-in until a unit translates it.
__attribute__((weak)) void MainPanel_EnableResearchButton(bool) {}
