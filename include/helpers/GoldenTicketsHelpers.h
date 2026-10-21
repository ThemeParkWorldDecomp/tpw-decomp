// GoldenTicketsHelpers.h: file-local helpers that src/game/park/GoldenTickets.cpp shares with FS_MasterSave.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "FS_MasterSave.h"

// The master save, made on the first use the way src/game/GameSys.cpp does (operator new 0x4c, the constructor 0x101694dc).
static inline CFSGlobalMasterSave* TicketMasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}
