// LbLog.h: bullfrog library text logs, functions at 0x10014e2c..0x10015504. The library
// names no file here; LbLog.cpp follows the functions' prefix.
//
// Every LbLog call opens the file, writes the optional headers and the message, and
// closes it again, so a crash loses nothing. The TbError-returning functions take the
// hidden result buffer in r3 and the log in r4. Field names are invented.
#pragma once
#include <cstdint>

#include "TbError.h"

struct TbLog {
    int32_t mInitialised;     // 0x000
    char mFilename[0x104];    // 0x004  full project file name
    char mPrefix[0x20];       // 0x108  written before each message, as a format string
    uint32_t mFlags;          // 0x128  kLog* below
    int32_t mOpened;          // 0x12c  set when LbLog first opens the file
    int32_t mSuspended;       // 0x130
};
static_assert(sizeof(TbLog) == 0x134, "TbLog layout");

enum : uint32_t {
    kLogOverwrite = 0x01,    // the first LbLog truncates the file ("w"); later ones append
    kLogHeader = 0x02,       // the first LbLog writes a "LOG CREATED" / "LOG APPENDED" line
    kLogHeaderDate = 0x04,   // with the date
    kLogHeaderTime = 0x08,   // and the time
    kLogDate = 0x10,         // every message starts with the date
    kLogTime = 0x20,         // and the time
};

// Fails unless the directory of the project file name exists.
TbError LbLog_Setup(TbLog* log, const char* filename, uint32_t flags);   // 0x10014e2c
TbError LbLog(TbLog* log, const char* format, ...);                       // 0x10014f9c
TbError LbLog_Close(TbLog* log);                                          // 0x10015244
TbError LbLog_SetPrefix(TbLog* log, const char* prefix);                  // 0x10015294
TbError LbLog_Delete(TbLog* log);                                         // 0x10015348
TbError LbLog_Suspend(TbLog* log);                                        // 0x1001541c
TbError LbLog_Restart(TbLog* log);                                        // 0x10015474
int32_t LbLog_IsInitialised(TbLog* log);                                  // 0x100154c8
int32_t LbLog_IsSuspended(TbLog* log);                                    // 0x10015500
