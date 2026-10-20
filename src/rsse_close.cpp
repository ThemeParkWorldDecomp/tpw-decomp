// rsse.cpp range of the main app (SimTheme Park 2.04b3): the script system's shutdown. RSSE_Init (0x100d32e8, untranslated)
// sets the state; the state and the malloc count are defined here with the first function that uses them.
#include "rsse.h"

#include "Debug.h"

RSSEState gRSSEState = {};        // 0x102f7bec
int32_t gRSSEMallocCount = 0;     // 0x102f7bdc

// Kills every live script (KillScript answers the next one), forgets the list, marks the system closed (also when it was not
// open) and complains when the scripts left malloced blocks behind (the assert only reports).
void RSSE_Close() {
    if (gRSSEState.mInitialised != 0) {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = KillScript(script, 0)) {
        }
        gRSSEState.mScripts = nullptr;
    }
    gRSSEState.mInitialised = 0;
    if (gRSSEMallocCount != 0) GAME_ASSERTMSG(0, "RSSE: Arrooga - Something malloced but not freed - bad boy - fix it !");
}
