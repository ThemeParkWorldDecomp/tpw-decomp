// CFixedTime (main app of SimTheme Park 2.04b3): the constructor, 0x10147bdc (the
// inventory's Game.cpp range). ReadFixedTime (0x10147c18) is in GameClock.cpp.
#include "CFixedTime.h"

// A reading of 0 and, unaligned at +4, a double of 0.0.
CFixedTime::CFixedTime() : mLastClock(0), mMilliseconds(0.0) {}
