// Debug.cpp: name proven by __sinit_Debug_cpp in the main application.

#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "lobby.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "TbDynamicStringTemplate.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "ui_main.h"
#include "UIInterface.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Debug.cpp (main app of SimTheme Park 2.04b3): PARTIAL, GetUniqueID, CTRACE and TRACE so far.

// 0x1006fc5c
uint32_t GetUniqueID(eIDClass idClass, uint32_t index) { return static_cast<uint32_t>(idClass) + index; }

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

uint32_t gV51077Debug5944;                                  // 0x102da944
uint32_t gV51077Debug5948;                                  // 0x102da948

void DEBUG_MESSAGE(const char* message, uint32_t flags) {}

// 0x1006fcbc (unit v5L21). As TRACE: an empty function in this build that only spills the variadic argument registers.
void CTRACE(uint32_t, const char*, ...) {}

// 0x1006fcfc. An empty function in this build: it only spills the variadic argument registers to the
// caller's parameter area. The game's trace output is gone, so nothing is shown.
void TRACE(const char*, ...) {}

// A ride's working copy of its animation set (the main app's TPRide.cpp, TPEmbed.cpp, rsse.cpp and
// ui_ErrorDisplay.cpp ranges, as the inventory files them): InstanceRideAnimSet and what it calls
// (InstanceMesh, InstanceAnim, InstanceDynamicMeshSet, DisableDataNodesRender and its sibling), and the
// small functions LoadRideType calls on a loaded set (CorrectFaceFlags1, CorrectNodeFlags2, SetupRideHeads,
// GetFontPath, OverrideActiveChannels) with ERRORBox.
//
// The copies are made the way the original makes them: one block from the allocator (MReleaseAllocF), the
// pieces laid after each other in it, then every pointer that led into the original's tables made to lead
// into the copies. The mesh and animation records keep the file's layout (Ptr32 fields, arena offsets), so
// the original's arithmetic on addresses is arithmetic on offsets here. The game's own records (tag_sMesh,
// the embedded set and its records, tag_sRideAnimSet and its entries, tag_sDMeshSet) have native pointers
// and are larger than on the Mac, so the block's fixed parts are larger too (see InstanceMesh).

// 0x1006fd3c, 32 bytes. The original is an empty variadic function: it spills the argument registers into
// the caller's frame and returns, so every message is dropped (and the debug routine next to it, UDebug,
// is empty too). The port writes the text to the log through UDebug, which the platform layer prints.
// Nothing else happens: no dialog, no exit.
void ERRORBox(const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    SamsUtilities::UDebug("ERRORBox: %s", text);
}

void __sinit_Debug_cpp() {
    gV51077Debug5948 = 0;
    gV51077Debug5944 = 0x10000;
}
