// The weak stand-ins of the unit library tpw_unit_UIStatusPanel_EnableNameClick
// (cmake/units/UIStatusPanel_EnableNameClick.cmake), as
// src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_UIStatusPanel_EnableNameClick.cpp of before the regroup
// (v5L30b) defined them, unchanged. The regroup put each function into the src file of its original source file,
// where these defaults would sit next to the real definitions, so it dropped them; the unit library compiles them
// from here, so that it builds what it built before.

#include <cstdint>
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "UI_Base.h"
#include "analyser.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"

namespace SamsUtilities {
// Weak: another unit (rsse_particles_init.cpp) defines the same instances at another address.
__attribute__((weak)) void USwapInplace32(int32_t& word) { word = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(word))); }
__attribute__((weak)) void USwapInplace16(int16_t& half) { half = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(half))); }
}  // namespace SamsUtilities
