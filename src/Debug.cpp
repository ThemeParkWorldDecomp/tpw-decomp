// Debug.cpp (main app of SimTheme Park 2.04b3): PARTIAL, GetUniqueID, CTRACE and TRACE so far.
#include "Debug.h"

// 0x1006fc5c
uint32_t GetUniqueID(eIDClass idClass, uint32_t index) { return static_cast<uint32_t>(idClass) + index; }

// 0x1006fcbc (unit v5L21). As TRACE: an empty function in this build that only spills the variadic argument registers.
void CTRACE(uint32_t, const char*, ...) {}

// 0x1006fcfc. An empty function in this build: it only spills the variadic argument registers to the
// caller's parameter area. The game's trace output is gone, so nothing is shown.
void TRACE(const char*, ...) {}

