// The weak stand-ins of the unit library tpw_unit_sinit_ui_RideStatus_cpp
// (cmake/units/sinit_ui_RideStatus_cpp.cmake), as src/sinit_ui_RideStatus_cpp.cpp of before the regroup (v5L30b)
// defined them, unchanged. The regroup put each function into the src file of its original source file, where these
// defaults would sit next to the real definitions, so it dropped them; the unit library compiles them from here, so
// that it builds what it built before.

#include "ui_controls.h"
#include "ui_main.h"

// UIStatusPanel::UIStatusPanel (0x101929e8) is not translated yet: an empty weak stand-in; the unit test
// defines its own recording fake.
__attribute__((weak)) void UIStatusPanel_Construct(UIStatusPanel*) {}
