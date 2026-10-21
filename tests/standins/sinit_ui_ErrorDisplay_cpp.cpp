// The weak stand-ins of the unit library tpw_unit_sinit_ui_ErrorDisplay_cpp
// (cmake/units/sinit_ui_ErrorDisplay_cpp.cmake), as src/sinit_ui_ErrorDisplay_cpp.cpp of before the regroup (v5L30b)
// defined them, unchanged. The regroup put each function into the src file of its original source file, where these
// defaults would sit next to the real definitions, so it dropped them; the unit library compiles them from here, so
// that it builds what it built before.

#include <cstdint>
#include <cstring>
#include "UI_pump.h"
#include "std_deque_errorthingy.h"

namespace errsinit_callee {
__attribute__((weak)) void DequeCtor(void*, void*) {}
__attribute__((weak)) void DequeCopy(void*, const void*) {}
}  // namespace errsinit_callee
