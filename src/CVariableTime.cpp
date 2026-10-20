// CVariableTime (main app of SimTheme Park 2.04b3): the constructor, 0x1016886c (the
// inventory's FS_MasterSave.cpp range). The rate members and ReadVariableTime
// (0x101688d4..0x10168a3c) are in GameClock.cpp.
#include "CVariableTime.h"

#include <new>

// The CFixedTime it starts with, then the rate at +0x18: 1.0. The reading and the
// milliseconds at +0x0c and +0x10 are not written.
CVariableTime::CVariableTime() {
    new (mFixedTimeStorage) CFixedTime;
    mRate = 1.0;
}
