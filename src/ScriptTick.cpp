// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w132: the script system's tick (SCR_ScriptTick, 0x100d907c, 224 bytes), its
// close (SCR_CloseScriptSystem, 0x100d8e4c, 148 bytes) and the list of the sprites the scripts own (SCR_BuildScriptSpriteList, 0x100d9184,
// 152 bytes). The table gScriptTable holds 32-bit words (100 of them, SCR_InitScriptSystem); here a word is an arena offset (Ptr32), as the
// records of the ride scripts' list are. SCR_KillScript (src/analyser_kill.cpp, unit w114) still reads a word as a raw address: no translated
// code stores a script block in the table, so the two never meet yet. The interpreter (DoScript), the sprite list functions and the
// clock are untranslated or other units'.
#include <cstdint>

#include "Clock.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "Ptr32.h"
#include "analyser.h"

int32_t gScriptTickTotal;   // 0x102f7c08
int32_t gScriptTickTime;    // 0x102f7c0c

namespace {
// The table's entry for `index` (a 32-bit word the table is indexed by, re-read from the global each time as the Mac does).
Ptr32<tagSCRIPT> Entry(int32_t index) { return static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[index]; }
}  // namespace

// Zeroes the total and, with the system on, reads the clock once and visits entries 1 to the table's size (re-read each time): an entry that is
// set and whose next time is before the clock (signed) is killed when its state is 4 (finished), else run with DoScript, the result added to
// the total, and given its next time (the clock plus its period).
void SCR_ScriptTick() {
    gScriptTickTotal = 0;
    if (gScriptInitialised != 0) {
        gScriptTickTime = gClock.Read();
        for (int32_t index = 1; index < gScriptTableSize; ++index) {
            tagSCRIPT* script = Entry(index);
            if (script != nullptr && gScriptTickTime > script->mNextTime) {
                if (script->mState == 4) {
                    SCR_KillScript(index);
                } else {
                    const int32_t result = DoScript(script);
                    gScriptTickTotal = static_cast<int32_t>(static_cast<uint32_t>(gScriptTickTotal) + static_cast<uint32_t>(result));
                    script->mNextTime = static_cast<int32_t>(static_cast<uint32_t>(gScriptTickTime) + static_cast<uint32_t>(script->mPeriod));
                }
            }
        }
    }
}

// With the system on: clears the flag, kills every entry (SCR_KillScript does nothing now that the flag is clear), frees the table, forgets
// it and its size, and closes the check table. Off: nothing.
void SCR_CloseScriptSystem() {
    if (gScriptInitialised != 0) {
        gScriptInitialised = 0;
        for (int32_t index = 0; index < gScriptTableSize; ++index) SCR_KillScript(index);
        LbMem_Free(gScriptTable);
        gScriptTable = nullptr;
        gScriptTableSize = 0;
        SCR_CloseCheckTable();
    }
}

// With the system on: resets the high level sprites and adds the sprite of every script of entries 1 to the table's size. The sprites are
// drawn whether the system is on or not.
void SCR_BuildScriptSpriteList() {
    if (gScriptInitialised != 0) {
        ResetHLSprites();
        for (int32_t index = 1; index < gScriptTableSize; ++index) {
            tagSCRIPT* script = Entry(index);
            if (script != nullptr) ESP_AddHLSpriteFromScript(script);
        }
    }
    DrawHLSprites();
}
