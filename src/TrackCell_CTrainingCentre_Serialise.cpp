// CTrainingCentre::Serialise and UpdateMonthly (unit CTrainingCentre_Serialise).
#include <cstdint>

#include "analyser.h"
#include "Debug.h"
#include "Serialise.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "World.h"

int32_t gTrainingCounts[5] = {0, 0, 0, 0, 0};   // 0x102a4408
uint16_t gTrainingEnd1 = 0;                      // 0x102a441c
uint16_t gTrainingEnd2 = 0;                      // 0x102a441e: the null thing id, like gTrainingEnd1 (the data image's halfword is 0;
                                                 // the 0x1013 that stood here is the next halfword, and the walk ran past the list's end)

// 0x10130008, 332 bytes.
int32_t CTrainingCentre::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    const char* name = "mBudget[i]";   // 0x10246afc; "mHaveEverTrained" follows at +0xb
    if (save != 0) {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 5; ++i) {
                if (SAVEVAR<int32_t>(file, reinterpret_cast<int32_t*>(&mF00[i]), name, 0) != 0) return 1;
            }
            if (SAVEVAR<bool>(file, reinterpret_cast<bool*>(&mF14), "mHaveEverTrained", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 5; ++i) {
                if (LOADVAR<int32_t>(file, reinterpret_cast<int32_t*>(&mF00[i]), name, 0) != 0) return 1;
            }
            if (LOADVAR<bool>(file, reinterpret_cast<bool*>(&mF14), "mHaveEverTrained", 0) != 0) return 1;
        }
    }
    return 0;
}

namespace {
const char* kAllocZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10246b18

// The index into the count words for a staff kind (5, 4, 6, 7, 8), or -1.
int32_t SlotOfKind(uint8_t kind) {
    switch (kind) {
        case 5: return 0;
        case 4: return 1;
        case 6: return 2;
        case 7: return 3;
        case 8: return 4;
    }
    return -1;
}
}  // namespace

// 0x1013018c, 928 bytes.
void CTrainingCentre::UpdateMonthly() {
    int32_t counts[5];
    for (int32_t i = 0; i < 5; ++i) counts[i] = gTrainingCounts[i];
    uint16_t cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gTrainingEnd1) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAllocZero);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        int32_t slot = SlotOfKind(thing->mKind);
        if (slot >= 0) counts[slot] += 1;
        cur = gGameWorld->GetNextUsedThing(thing).mIndex;
    }
    cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gTrainingEnd2) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAllocZero);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        int32_t slot = SlotOfKind(thing->mKind);
        if (slot >= 0) {
            int32_t amount = static_cast<int32_t>(mF00[slot]) / counts[slot];
            reinterpret_cast<CStaff*>(thing)->TrainMe(amount);
        }
        cur = gGameWorld->GetNextUsedThing(thing).mIndex;
    }
}
