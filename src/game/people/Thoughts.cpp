// Thoughts.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Thoughts code, named after it.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "helpers/rsseHelpers.h"
#include "LbSCBack.h"
#include "platform/arena.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// CThoughts (TrainingCentre.cpp in the original): the thought bubble and event history of a person or staff
// member, the bytes at +0x30 of the peep. The class declares no data, so the fields are read by Mac offset:
//   +0x00 mLastThought, +0x04 mEventHistory[32] (two halfwords each), +0x84 mThoughtScript (script handle),
//   +0x88 mActionHistIndex, +0x8c mTimeBubbleShown (turn).

// The 22 script blocks at 0x1029fb20, 0x18 bytes apart (the Mac's words are pointers into the TOC area and small numbers).
alignas(8) uint8_t gThoughtScripts[22][24];
// The event history element constructor table (TVector at 0x10266cc0), passed to __construct_array.
alignas(8) uint8_t gEventHistCtorVec[16];

namespace {

struct ThoughtCase {
    int32_t script;     // index into gThoughtScripts
    int32_t multiplier; // the minimum gap is multiplier * 20 turns
};

// The jump table at 0x102a439c: one entry per ThoughtType 0 to 0x16; script -1 is the default (CTRACE).
const ThoughtCase kThoughts[23] = {
    {-1, 0}, {8, 1}, {10, 1}, {12, 1}, {13, 1}, {16, 0}, {9, 0}, {14, 1}, {5, 3}, {0, 3}, {1, 3}, {3, 3},
    {4, 0}, {2, 0}, {6, 1}, {7, 0}, {11, 0}, {15, 0}, {17, 3}, {18, 2}, {19, 0}, {20, 0}, {21, 0},
};

inline uint8_t* Bytes(CThoughts* t) { return reinterpret_cast<uint8_t*>(t); }
inline const uint8_t* Bytes(const CThoughts* t) { return reinterpret_cast<const uint8_t*>(t); }

}  // namespace

CThoughts::CThoughts() {
    uint8_t* self = Bytes(this);
    *reinterpret_cast<int32_t*>(self) = 0;
    __construct_array(self + 4, gEventHistCtorVec, nullptr, 4, 0x20);
    *reinterpret_cast<int32_t*>(self + 0x84) = 0;
    *reinterpret_cast<int32_t*>(self + 0x8c) = 0;
    for (int32_t i = 0; i < 0x20; ++i) {
        *reinterpret_cast<uint16_t*>(self + 4 + i * 4) = 0;
        *reinterpret_cast<uint16_t*>(self + 6 + i * 4) = 0;
    }
    *reinterpret_cast<int32_t*>(self + 0x88) = 0;
}

// The world's load (main app of SimTheme Park 2.04b3, World.cpp; unit w207): CWorld::Load, the two LOADVAR instances it needs for its own
// enums (CWorld::WorldState 0x101415b4 and CThing::ThingModel 0x101416f0) and CThing::ForceThingId (Thing.cpp). The matching save is
// src/World_save.cpp (unit w205).
//
// The load reads what Save wrote: a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list
// of the thing table. It starts by resetting the table (InitThingHeap). The used list is rebuilt from the head's index and each entry's next
// index, and for every used entry the load MAKES the thing of the entry's kind in place (operator new of the kind's size, the part of the
// class's constructor the Mac inlines here, the kind byte, the entry's index as the thing's id) before the thing's own Serialise reads it.
// The Mac's operator new is the game heap, so the host takes every thing's block from the arena (unit v5L31): the lists keep a thing's
// address in a 32-bit word.
// Every callee answers 0 for success; the first nonzero answer ends Load with 0 at once (nothing is called after it, and a thing that was
// made is not entered in its row), and a load that read everything relinks the free list, switches the sprite drawing by the world state
// and answers 1. The strings are at 0x10247498 + the offset in the comment.

// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.

// 0x1012ed84: the script handle is the word at +0x84 of the bytes the peep holds.
void CThoughts::FinalDestroy() {
    int32_t handle = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(this) + 0x84);
    if (handle != 0) SCR_KillScript(handle);
}

int32_t CThoughts::Serialise(void* file, int32_t save, int32_t version, uint32_t) {
    uint8_t* self = Bytes(this);
    static const char kIndex[] = "mActionHistIndex";
    static const char kHist[] = "mEventHistory[i]";
    static const char kLast[] = "mLastThought";
    static const char kScript[] = "mThoughtScript";
    static const char kTime[] = "mTimeBubbleShown";
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVAR<uint32_t>(file, reinterpret_cast<uint32_t*>(self + 0x88), kIndex, 0) != 0) return 1;
            for (int32_t i = 0; i < 0x20; ++i)
                if (SAVEVARSTRUCT<EventHistType>(file, reinterpret_cast<EventHistType*>(self + 4 + i * 4), kHist, 0) != 0) return 1;
            if (SAVEVAR<ThoughtType>(file, reinterpret_cast<ThoughtType*>(self), kLast, 0) != 0) return 1;
            if (SAVEVAR<MacLong>(file, reinterpret_cast<MacLong*>(self + 0x84), kScript, 0) != 0) return 1;
            if (SAVEVAR<MacULong>(file, reinterpret_cast<MacULong*>(self + 0x8c), kTime, 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVAR<uint32_t>(file, reinterpret_cast<uint32_t*>(self + 0x88), kIndex, 0) != 0) return 1;
            for (int32_t i = 0; i < 0x20; ++i)
                if (LOADVARSTRUCT<EventHistType>(file, reinterpret_cast<EventHistType*>(self + 4 + i * 4), kHist, 0) != 0) return 1;
            if (LOADVAR<ThoughtType>(file, reinterpret_cast<ThoughtType*>(self), kLast, 0) != 0) return 1;
            if (LOADVAR<MacLong>(file, reinterpret_cast<MacLong*>(self + 0x84), kScript, 0) != 0) return 1;
            if (LOADVAR<MacULong>(file, reinterpret_cast<MacULong*>(self + 0x8c), kTime, 0) != 0) return 1;
        }
    }
    return 0;
}
void CThoughts::ProcessThoughtBubbles() {
    uint32_t shown = *reinterpret_cast<uint32_t*>(Bytes(this) + 0x8c);
    if (gGameWorldRef->mTurn > shown + 0xc) RemoveThoughtBubble();
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) {
    uint8_t* self = Bytes(this);
    *reinterpret_cast<int32_t*>(self) = type;
    if ((gFirstPersonFlags & 0x16) != 0) return;
    if (static_cast<int32_t>(TheGameType().mType) == 1 && !always) return;
    if (*reinterpret_cast<int32_t*>(self + 0x84) != 0) RemoveThoughtBubble();
    const uint32_t t = static_cast<uint32_t>(type);
    int32_t script = -1;
    int32_t multiplier = 3;
    if (t <= 0x16) {
        script = kThoughts[t].script;
        multiplier = kThoughts[t].multiplier;
    }
    if (script < 0) {
        CTRACE(2, "Not a known thought!\n");  // above 0x16, and type 0 whose table entry is the default too
        return;
    }
    uint32_t now = gGameWorldRef->mTurn;
    if (now < *reinterpret_cast<uint32_t*>(self + 0x8c) + static_cast<uint32_t>(multiplier * 5 * 4)) return;
    *reinterpret_cast<uint32_t*>(self + 0x8c) = now;
    *reinterpret_cast<int32_t*>(self + 0x84) =
        ESP_SpawnScript(gThoughtScripts[script], static_cast<ESP_Sprites>(9), 0, static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f);
}

void CThoughts::RemoveThoughtBubble() {
    uint8_t* self = Bytes(this);
    if (*reinterpret_cast<int32_t*>(self + 0x84) != 0) {
        SCR_KillScript(*reinterpret_cast<int32_t*>(self + 0x84));
        *reinterpret_cast<int32_t*>(self + 0x84) = 0;
    }
}

void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    uint8_t* self = Bytes(this);
    const uint16_t* recent = reinterpret_cast<const uint16_t*>(GetRecentEvent(1));
    if (static_cast<int32_t>(type) == static_cast<int32_t>(recent[0])) {
        const uint16_t* again = reinterpret_cast<const uint16_t*>(GetRecentEvent(1));
        if (again[1] == thing.mIndex) return;
    }
    uint32_t index = *reinterpret_cast<uint32_t*>(self + 0x88);
    *reinterpret_cast<uint16_t*>(self + index * 4 + 4) = static_cast<uint16_t>(type);
    *reinterpret_cast<uint16_t*>(self + index * 4 + 6) = thing.mIndex;
    *reinterpret_cast<uint32_t*>(self + 0x88) = (*reinterpret_cast<uint32_t*>(self + 0x88) + 1) & 0x1f;
}

const CThoughtEvent* CThoughts::GetRecentEvent(uint32_t index) const {
    GAME_ASSERTMSG(index < 0x20, "Haven't got that much event history\n");
    const uint8_t* self = Bytes(this);
    uint32_t current = *reinterpret_cast<const uint32_t*>(self + 0x88);
    uint32_t slot = current < index ? current + 0x20 - index : current - index;
    return reinterpret_cast<const CThoughtEvent*>(self + (slot * 4 + 4));
}
