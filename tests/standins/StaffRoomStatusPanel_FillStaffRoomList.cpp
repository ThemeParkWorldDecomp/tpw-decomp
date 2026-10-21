// The weak stand-ins of the unit library tpw_unit_StaffRoomStatusPanel_FillStaffRoomList
// (cmake/units/StaffRoomStatusPanel_FillStaffRoomList.cmake), as
// src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_FillStaffRoomList.cpp of before
// the regroup (v5L30b) defined them, unchanged. The regroup put each function into the src file of its original
// source file, where these defaults would sit next to the real definitions, so it dropped them; the unit library
// compiles them from here, so that it builds what it built before.

#include <cstdint>
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ESP_sprites.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

// Untranslated neighbours, weak stand-ins so the program links (the test library defines the real fakes).
__attribute__((weak)) int32_t StaffRoomListFiller(CThing*) { return 0; }       // 0x101c30fc
__attribute__((weak)) InterfaceWindow* StaffRoomLCFormat() { return nullptr; }   // 0x101c3360
