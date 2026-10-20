#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "Debug.h"
#include "FS_System.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "platform/samsutils.h"
#include "RideDB.h"
#include "TPSettings.h"
#include "TPCoaster.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"

// Unit v5934. The object list's end marker (halfword at 0x102b12e6; its neighbour 0x102b12e8 is the GetAnimStats jump table).
uint16_t gRideSignListEnd = 0;

namespace {
inline uint32_t WordAt(const void* base, size_t offset) {
    return *reinterpret_cast<const uint32_t*>(static_cast<const char*>(base) + offset);
}
inline uint16_t HalfAt(const void* base, size_t offset) {
    return *reinterpret_cast<const uint16_t*>(static_cast<const char*>(base) + offset);
}
}  // namespace

// 0x101eeb9c (400 bytes). The object is reached by offset (+0x1c the ride instance). Answers 0 for no record, no instance or no local
// path, 1 when the renderer is software (nothing drawn), else GenerateRideSign's answer; the set is flagged 0x8000000 (when its bit 0
// is clear) in every case that got that far. When UniStringToString answers 0 the original answers its caller's r31; here 0.
void RegenerateRideSign(void* object, const char16_t* signA, const char16_t* signB) {
    RegenerateRideSignResult(object, signA, signB);
}

int32_t RegenerateRideSignResult(void* object, const char16_t* signA, const char16_t* signB) {
    const uint32_t instance = WordAt(object, 0x1c);
    RideInfo* info = static_cast<CObject*>(object)->GetDBInfo();
    if (info == nullptr || instance == 0) return 0;
    const char* path = gTPFileInfo.GetLocalPath().mData;
    if (path == nullptr) return 0;
    char fontPath[1024];
    const size_t length = std::strlen(path);
    if (path[length - 1] == ':') std::sprintf(fontPath, "%s%s", path, "Data:Fonts");
    else std::sprintf(fontPath, "%s:%s", path, "Data:Fonts");
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    int32_t result = 0;
    if (gPlayerSettings.mCurrent.mRenderer == 0) {
        result = 1;
    } else {
        TbDynamicStringTemplate<char16_t> wide(info->mItemName);
        TbDynamicStringTemplate<char> narrow;
        if (UniStringToString(narrow, wide) != 0)
            result = GenerateRideSign(set, info->mFolder, const_cast<char*>(narrow.mData), fontPath, signA, signB);
    }
    if (set != nullptr && (set->mFlags & 1) == 0) set->mFlags |= 0x8000000;
    return result;
}

// 0x101eeeb4 (240 bytes). Walks the object list (head: the world's halfword +0x1da746, links at +0xc of each object, ends at the marker)
// and regenerates the sign of each ride whose set has flag 0x8000000.
void RegenerateRideSigns() {
    char16_t signA[256];
    char16_t signB[256];
    uint16_t id = gGameWorld->mF1DA746;
    while (id != gRideSignListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        CObject* object = static_cast<CObject*>(gThingArray[id].mThing);
        tag_sRideAnimSet* set = gRideAnimSets[WordAt(object, 0x1c)];
        if (set != nullptr && (set->mFlags & 0x8000000) != 0) {
            object->GetSignNameA(signA, 0x100);
            object->GetSignNameB(signB, 0x100);
            RegenerateRideSign(object, signA, signB);
        }
        id = HalfAt(object, 0xc);
    }
}

// 0x101eefd0 (76 bytes). No NULL check on the set: the instance's set is dereferenced.
void SetRideScriptID(uint32_t instance, uint32_t script) {
    if (instance == 0) return;
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    set->mFD4 = static_cast<int32_t>(script);
    if ((set->mFlags & 0x40000) != 0) SetCoasterScriptID(instance, static_cast<int32_t>(script));
}

// 0x101ef044 (300 bytes). Finds the group (of the first 12) and the place in it whose entry holds the set's animation number `index`;
// answers the group's name ("Failure" when none).
void GetAnimStats(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other) {
    GetAnimStatsName(set, index, kind, place, other);
}

const char* GetAnimStatsName(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other) {
    static const char* const kNames[15] = {"Create", "Destroy", "Idle", "Load", "Start", "Main", "End", "Unload",
                                           "Worn", "Break", "Repair", "Other", "None", "Pause", "Failure"};
    for (int32_t group = 0; group < 12; ++group) {
        const sRideAnimGroup& g = set->mGroups[group];
        for (uint32_t i = 0; i < g.mCount; ++i) {
            if (set->mAnimations[index] == g.mEntries[i].mAnimation) {
                if (kind != nullptr) *kind = static_cast<uint32_t>(group);
                if (place != nullptr) *place = i;
                if (other != nullptr) *other = g.mCount;
                if (static_cast<uint32_t>(group) < 15) return kNames[group];
            }
        }
    }
    return "Failure";
}

// 0x101ef1f4 (212 bytes). count: live entries, firstFree: holes below the top, lowest: the next free slot. `extra` other than
// 0xffffffff just stores at that slot. Answers 0 when the table is full (no hole and count at the limit).
uint32_t AllocateObjectInstanceToTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, uint32_t flags, void** table,
                                       void* object, uint32_t extra) {
    if (extra != 0xffffffffu) {
        table[extra] = object;
        return extra;
    }
    if (firstFree == 0 && count >= flags) return 0;
    const uint32_t slot = lowest;
    table[slot] = object;
    const uint32_t top = count;
    if (lowest - 1 == top) {
        lowest = lowest + 1;
        count = count + 1;
    } else {
        for (uint32_t i = lowest; i <= top + firstFree; ++i) {
            if (table[i + 1] == nullptr) {
                lowest = i + 1;
                break;
            }
        }
        firstFree = firstFree - 1;
        count = count + 1;
    }
    return slot;
}

// 0x101ef310 (220 bytes).
void RemoveObjectInstanceFromTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, void** table, uint32_t id) {
    if (id == 0) return;
    table[id] = nullptr;
    const uint32_t holes = firstFree;
    if (holes == 0) {
        if (id == count) {
            lowest = lowest - 1;
            count = count - 1;
            return;
        }
        firstFree = holes + 1;
        lowest = id;
        count = count - 1;
        return;
    }
    const uint32_t top = count;
    if (id == top + holes) {
        count = top - 1;
        for (;;) {
            if (firstFree == 0) return;
            if (table[static_cast<uint32_t>(count + firstFree)] != nullptr) return;
            firstFree = firstFree - 1;
        }
    }
    firstFree = holes + 1;
    count = count - 1;
    if (id >= lowest) return;
    lowest = id;
}
