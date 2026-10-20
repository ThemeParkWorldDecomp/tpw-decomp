// Host only (unit w163): the script blocks of the script system. The Mac allocates a block with LbMem_Alloc(0x118) in SCR_SpawnScript and
// ESP_SpawnScript and frees it with LbMem_Free in SCR_KillScript. The table's words are Ptr32s (see src/ScriptTick.cpp), so a block has to
// come from the arena. Unit libraries never link this file: they fake the pair with the events the Mac's allocator calls would log.
#include "analyser.h"
#include "platform/arena.h"

// The Mac's LbMem_Alloc(0x118): contents undefined, nullptr when the arena is used up.
tagSCRIPT* SCR_NewScriptBlock() { return static_cast<tagSCRIPT*>(tpw::ArenaAlloc(sizeof(tagSCRIPT))); }

// The Mac's LbMem_Free of a script block.
void SCR_FreeScriptBlock(tagSCRIPT* script) { tpw::ArenaFree(script); }
