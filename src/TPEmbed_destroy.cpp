// TPEmbed.cpp (main app 0x1020039c and 0x101ffa58, unit w114): turning a ride head off and giving a mesh's embedded data back.
#include "TPEmbed.h"

#include "TPEngine.h"
#include "analyser.h"

// Turns the embedded entry `index` of the mesh's set off: the set's counter (word +8, the enabled heads as far as is known) goes down by
// one (wrapping) and, when it is then 0, the set's flag 4 (TickRideHeads runs) is cleared; the entry loses flag 2 (a head) whatever the
// counter is, its script is killed and its handle set to 0xffffffff. Nothing is bounds checked. The original also loads the mesh header's
// halfwords at 0x46 and 0x44 and compares index plus the first with the second, with no branch on the result (a compiled-out
// assertion); it is left out, which only matters for a mesh with no header.
void DisableRideHead(tag_sMesh* mesh, uint32_t index) {
    sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
    set->mF08 = set->mF08 - 1;
    sEmbeddedRecord* record = set->mRecords + index;
    if (static_cast<sEmbeddedSet*>(mesh->mF28)->mF08 == 0) set->mFlags = set->mFlags & ~4u;
    record->mFlags = record->mFlags & ~2u;
    SCR_KillScript(static_cast<int32_t>(record->mF0C));
    record->mF0C = 0xffffffff;
}

// Gives the mesh's embedded set back: the blocks, the records and the set itself (MReleaseFreeF each, also for a NULL pointer, which it
// ignores), clearing the first two pointers after each call and the mesh's word at the end. A mesh without a set does nothing.
void TPDestroyEmbeddedData(tag_sMesh* mesh) {
    if (mesh->mF28 != nullptr) {
        sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
        MReleaseFreeF(set->mBlocks);
        set->mBlocks = nullptr;
        MReleaseFreeF(set->mRecords);
        set->mRecords = nullptr;
        MReleaseFreeF(mesh->mF28);
        mesh->mF28 = nullptr;
    }
}
