// Unit v5314: SetCoasterEdit 0x10058758, GetPylonFromBraceMesh 0x100587fc, GetNthBraceMesh 0x10058888, HandleCoasterButtonClick 0x10058510.
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "UIInterface.h"
#include "ui_main.h"

// 0x100585f4 (296 bytes): the overload that takes the instance.
void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag);

namespace {

inline uint32_t Word(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

}  // namespace

// 0x10058758 (124 bytes): the coaster object's word 0x1c is the entrance id. Does nothing while a recording runs.
void SetCoasterEdit(void* coaster, int32_t flag) {
    tag_sCoasterInstance* instance = nullptr;
    if (coaster != nullptr) instance = FindCoasterInstanceFromEntranceID(Word(coaster, 0x1c));
    if (gActionRecorder.mRecording == 0) {
        if (flag != 0)
            SetCoasterEdit(instance, 1);
        else
            SetCoasterEdit(static_cast<tag_sCoasterInstance*>(nullptr), 0);
    }
}

// 0x100587fc (92 bytes): answers the brace list (a 32-bit address) of ride set `id`, 0 when the set has no braces.
// *out is the list's count, less one when the list's flag 0x20 is set. The set's word 0xd4 points at a block whose word 0x100 is the list.
sBraceMeshList* GetPylonFromBraceMesh(uint32_t id, uint32_t* out) {
    tag_sRideAnimSet* set = gRideAnimSets[id];
    if ((set->mFlags & 0x80000) == 0) {
        *out = 0;
        return nullptr;
    }
    sBraceMeshList* list = static_cast<sBraceMeshBlock*>(set->mBraceBlock)->mList;
    if ((list->mFlags & 0x20) != 0)
        *out = list->mCount - 1;
    else
        *out = list->mCount;
    return list;
}

// 0x10058888 (112 bytes): the n-th brace mesh (the entry's pointer at 0x70) of a list; with flag 0x20 the entries flagged 0x2000 are skipped.
void* GetNthBraceMesh(sBraceMeshList* list, uint32_t n) {
    if ((list->mFlags & 0x20) == 0) return list->mEntries[n]->mMesh;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < list->mCount; i++) {
        sBraceMeshEntry* entry = list->mEntries[i];
        if ((entry->mFlags & 0x2000) == 0) {
            if (n == seen) return entry->mMesh;
            seen++;
        }
    }
    return nullptr;
}

// 0x10058510 (176 bytes): event bit 2 releases the buttons (only those in 0x1ff), bit 1 presses, bit 4 clears all.
void HandleCoasterButtonClick(uint32_t buttons, uint32_t event) {
    if ((event & 2) != 0) {
        if ((buttons & 0x1ff) != 0) {
            DisableCurrentPylonEditState();
            gCoasterEditDown = gCoasterEditDown & ~buttons;
        }
    } else if ((event & 1) != 0) {
        HandleCoasterButtonDown(buttons);
    } else if ((event & 4) != 0) {
        DisableCurrentPylonEditState();
        gCoasterEditEnabled = 0;
        gCoasterEditDown = 0;
    }
    if (buttons != 0) {
        GetCoasterEditAvailableButtons(nullptr);
        gUIInterface.ChangeCoasterButtonsEnabled(gCoasterEditEnabled);
    }
}
