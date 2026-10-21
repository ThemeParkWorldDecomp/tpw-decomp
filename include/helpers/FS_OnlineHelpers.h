// FS_OnlineHelpers.h: file-local helpers that src/game/online/FS_Online.cpp shares with FS_OnlineList.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TPMain.h"

// The Mac's "if (gOnlineSystem == 0) make it", inlined at every use.
static inline CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) {
        CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(::operator new(0x404));
        if (system != nullptr) new (system) CFSOnlineSystem();
        gOnlineSystem = system;
    }
    return gOnlineSystem;
}
