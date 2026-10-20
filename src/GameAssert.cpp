// GAME_ASSERTMSG (main app of SimTheme Park 2.04b3, 0x1000dde8; the inventory files it under
// Advisor.cpp). The release build reports nothing: the original only spills the registers of
// a variable argument list into the caller's frame and returns. Every balance table error
// goes through it.
#include "Debug.h"

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {}
