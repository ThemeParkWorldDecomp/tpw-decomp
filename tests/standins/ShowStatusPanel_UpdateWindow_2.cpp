// The weak stand-ins of the unit library tpw_unit_ShowStatusPanel_UpdateWindow_2
// (cmake/units/ShowStatusPanel_UpdateWindow_2.cmake), as
// src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_UpdateWindow_2.cpp of before the regroup (v5L30b) defined
// them, unchanged. The regroup put each function into the src file of its original source file, where these defaults
// would sit next to the real definitions, so it dropped them; the unit library compiles them from here, so that it
// builds what it built before.

#include <cstdint>
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "Text.h"
#include "TbITable.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "ui_controls.h"

// The year end panel's remover (0x101c73e4, 56 bytes): not translated, a weak stand-in.
__attribute__((weak)) void RemoveYearEndPanel() {}
