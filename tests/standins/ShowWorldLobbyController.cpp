// The weak stand-ins of the unit library tpw_unit_ShowWorldLobbyController
// (cmake/units/ShowWorldLobbyController.cmake), as
// src/std_less_std_basic_string_operator_ShowWorldLobbyController.cpp of before the regroup (v5L30b) defined them,
// unchanged. The regroup put each function into the src file of its original source file, where these defaults would
// sit next to the real definitions, so it dropped them; the unit library compiles them from here, so that it builds
// what it built before.

#include <cstdint>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "MslWStringTree.h"
#include "TPGlobals.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "lobby.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"

namespace wstring_less_callee {
__attribute__((weak)) uint32_t Size(const MslWideString*) { return 0; }
__attribute__((weak)) const char16_t* Begin(const MslWideString*) { return nullptr; }
}  // namespace wstring_less_callee
