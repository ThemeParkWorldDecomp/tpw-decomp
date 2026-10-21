// The weak stand-ins of the unit library tpw_unit_CFSOnlineSystem_CompleteFTPDownload
// (cmake/units/CFSOnlineSystem_CompleteFTPDownload.cmake), as
// src/FS_Online_init_CFSOnlineSystem_CompleteFTPDownload.cpp of before the regroup (v5L30b) defined them, unchanged.
// The regroup put each function into the src file of its original source file, where these defaults would sit next
// to the real definitions, so it dropped them; the unit library compiles them from here, so that it builds what it
// built before.

#include <cstring>
#include <new>
#include "FS_Online.h"
#include "FS_System.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "Text.h"
#include "platform/samsutils.h"

namespace ftpdl_callee {
__attribute__((weak)) CFSOnlineServer* ReturnFTPDownloadServer(CFSOnlineSystemServerList*) { return nullptr; }
}  // namespace ftpdl_callee
