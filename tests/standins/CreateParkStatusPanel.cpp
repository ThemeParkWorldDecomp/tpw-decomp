// The weak stand-ins of the unit library tpw_unit_CreateParkStatusPanel (cmake/units/CreateParkStatusPanel.cmake),
// as src/CreateParkStatusPanel.cpp of before the regroup (v5L30b) defined them, unchanged. The regroup put each
// function into the src file of its original source file, where these defaults would sit next to the real
// definitions, so it dropped them; the unit library compiles them from here, so that it builds what it built before.

#include <algorithm>
#include <cstdint>
#include <new>
#include "FinanceGraphPanelMsg.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

// Untranslated message functions and data this file needs (weak stand-ins; the real ones override them at the link).
__attribute__((weak)) int32_t ThoughtMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }      // 0x101ab968
__attribute__((weak)) int32_t GraphPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }   // 0x101ac044
