// Debug.cpp (main app of SimTheme Park 2.04b3): PARTIAL, GetUniqueID and TRACE so far.
#include "Debug.h"

// 0x1006fc5c
uint32_t GetUniqueID(eIDClass idClass, uint32_t index) { return static_cast<uint32_t>(idClass) + index; }

// 0x1006fcfc. An empty function in this build: it only spills the variadic argument registers to the
// caller's parameter area. The game's trace output is gone, so nothing is shown.
void TRACE(const char*, ...) {}

