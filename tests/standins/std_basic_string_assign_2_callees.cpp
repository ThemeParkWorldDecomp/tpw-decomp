// A callee of the unit library tpw_unit_std_basic_string_assign_2 (cmake/units/std_basic_string_assign_2.cmake) that
// the regroup (v5L30b) made external. src/game/online/FS_OnlineList.cpp declares the explicit specialization
// TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::RemoveHead() for its own callers, and since the regroup the weak
// onlinelist3_callee::ThemeRemoveHead sits below that declaration, so it calls the function instead of inlining
// the template. No file of src/ defines that specialization (the program links the template's own instance), so
// this instantiates the template here, as the copy before the regroup did inline.

#include "FS_OnlineList.h"
#include "TbTList.h"

template CFSOnlineTheme* TbTList<CFSOnlineTheme*, CFSOnlineTheme*>::RemoveHead();
