// analyser.cpp (main app 0x100d9478, unit w114): SCR_KillScript.
#include "analyser.h"

#include <cstdint>

#include "LbFile.h"
#include "Ptr32.h"

// Frees the script block of `handle` and clears its table entry (first), when the script system is on and 0 < handle < the table's size
// (signed compares: a negative size lets nothing through). The table holds 32-bit words (Ptr32 offsets, like src/ScriptTick.cpp reads them);
// the block goes back through SCR_FreeScriptBlock (the Mac's LbMem_Free, src/ScriptBlocks.cpp).
void SCR_KillScript(int32_t handle) {
    if (gScriptInitialised != 0 && handle > 0 && handle < gScriptTableSize) {
        Ptr32<tagSCRIPT>* entry = static_cast<Ptr32<tagSCRIPT>*>(gScriptTable) + handle;
        tagSCRIPT* script = *entry;
        if (script != nullptr) {
            entry->mOffset = 0;
            SCR_FreeScriptBlock(script);
        }
    }
}
