// The world's turn and its thing lists (main app, SimTheme Park 2.04b3, World.cpp; unit w121): CWorld::DoGameTurn, DoOnlineGameTurn,
// UpdateThingsInEngine, UpdateBumperCarsInEngine, AddThingToThingList, _CThingArrayEntry::FreeMe, DeleteAllThings, DeletePendingThings,
// GetNextUsedThing, GetUsedThingHead, IsParkClosed and ParkGates_GetStatus.
//
// The thing table (gThingArray, five words an entry on the Mac) holds a doubly linked list of the used entries, newest first, and a singly
// linked list of the free ones. Every walk below follows the link of the row named by the THING'S OWN ID (what CThing::GetThingId answers),
// which is the row the thing sits in only while the id and the row agree. Every walk asserts, once per row, that entry 0 is free; the
// release GAME_ASSERTMSG does nothing, so the assert changes only the call log.
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "ActionRecorder.h"
#include "Debug.h"
#include "FS_Online.h"
#include "TPAdvisor.h"
#include "ThingReceivers.h"
#include "WorldThings.h"
#include "analyser.h"
#include "rsse.h"

namespace {

// The "invalid thing id" that ends a walk. The Mac reads it from seven separate 16 bit words in initialised data (0x102a4610, 0x102a4612,
// 0x102a4614, 0x102a4616, 0x102a46c0, 0x102a46c2, 0x102a46d8, one for each function that uses it), all 0 and never written.
constexpr uint16_t kNoThing = 0;

// Fields of the thing object that CThing does not name yet (Mac offsets): a CObject's script handle and, for a kind 3 thing, a ride id word.
constexpr std::size_t kObjectScriptOffset = 0x20;
constexpr std::size_t kObjectRideOffset = 0x24;

// The strings at 0x10247498 + the offset in the comment.
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x151
const char kUsedSlotText[] = "Trying to use already-used slot\n";                                        // +0x194
const char kFreeEmptyText[] = "Trying to free empty slot\n";                                             // +0x1b5
const char kArrayFullText[] = "FATAL ERROR - Thing Array is full!  Consider increasing its size!\n";    // +0x1d0
const char kAddingText[] = "Adding thing %d to thing list\n";                                            // +0x213
const char kRemovingText[] = "Removing thing %d from thing list\n";                                      // +0x232
const char kStaffText[] = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Counting staff outside park\n";                                       // +0x255 (94 marks)

// The index word of an entry, cut to 16 bits (the Mac loads the whole word and stores a halfword); 0 for no entry.
uint16_t IndexOf(const CThingArrayEntry* entry) { return entry != nullptr ? static_cast<uint16_t>(entry->mF04[0]) : uint16_t(0); }

int32_t ReadWord(const CThing* thing, std::size_t offset) {
    int32_t word;
    std::memcpy(&word, reinterpret_cast<const unsigned char*>(thing) + offset, sizeof word);
    return word;
}

// The staff kinds 4 to 8 (the Mac builds this as a chain of tests on 4, 5, 6, 8 and 7).
bool IsStaffKind(uint8_t kind) { return kind >= 4 && kind <= 8; }

}  // namespace

// 0x1013adb0 (40 bytes). The id of the newest used entry, 0 when the list is empty. Does not read the world.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const { return {IndexOf(gUsedThingHead)}; }

// 0x1013ae08 (112 bytes). The id of the used entry after the one of the thing's own id, 0 at the end of the list. The thing is not
// tested for NULL and the world is not read.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    const uint16_t id = thing->GetThingId().mIndex;
    return {IndexOf(gThingArray[id].mNextUsed)};
}

// 0x1013b054 (200 bytes). Takes the first free entry, makes it the head of the used list and stores the thing in it; answers the entry's
// index (0 when the table is full). The new entry's previous link is not written: a stale value survives (FreeMe clears it).
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::AddThingToThingList(CThing* thing) {
    CThingArrayEntry* entry = gFreeThingHead;
    if (entry == nullptr) {
        GAME_ASSERTMSG(0, kArrayFullText);
        return {0};
    }
    GAME_ASSERTMSG(entry->mThing == nullptr, kUsedSlotText);
    gFreeThingHead = entry->mNextFree;
    entry->mNextFree = nullptr;
    entry->mNextUsed = gUsedThingHead;
    if (gUsedThingHead != nullptr) gUsedThingHead->mPrevUsed = entry;
    gUsedThingHead = entry;
    entry->mThing = thing;
    CTRACE(2, kAddingText, gUsedThingHead->mF04[0]);
    return {static_cast<uint16_t>(gUsedThingHead->mF04[0])};
}

// 0x1013af58 (192 bytes). Unlinks the entry from the used list and pushes it on the free list, then releases the thing with the plain
// operator delete (no destructor runs) and clears the pointer. The used head is overwritten whenever the entry's previous link is NULL,
// even for an entry that is not the head (a double free).
void CThingArrayEntry::FreeMe() {
    GAME_ASSERTMSG(mThing != nullptr, kFreeEmptyText);
    if (mNextUsed != nullptr) mNextUsed->mPrevUsed = mPrevUsed;
    if (mPrevUsed != nullptr) mPrevUsed->mNextUsed = mNextUsed;
    else gUsedThingHead = mNextUsed;
    mNextUsed = nullptr;
    mPrevUsed = nullptr;
    mNextFree = gFreeThingHead;
    gFreeThingHead = this;
    ::operator delete(mThing);
    mThing = nullptr;
}

// 0x1013dda8 (148 bytes). Frees the entry of every id the pending list holds (CThing::KillMe queues them). The world is not read.
void CWorld::DeletePendingThings() {
    while (gPendingDeleteList.mCount != 0) {
        const uint16_t id = gPendingDeleteList.RemoveHead().mIndex;
        CTRACE(2, kRemovingText, static_cast<uint32_t>(id));
        gThingArray[id].FreeMe();
    }
}

// 0x1013ac4c (308 bytes). Kills every live thing of the used list (an alive thing makes three entry 0 asserts, a dead one two), then frees
// what they queued. The word 0x1da738 is 2 while this runs and 1 after, whatever it held.
void CWorld::DeleteAllThings() {
    mF1DA738 = 2;
    uint16_t cur = GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        if (static_cast<CThing*>(gThingArray[cur].mThing)->mDead == 0) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            static_cast<CThing*>(gThingArray[cur].mThing)->KillMe();
        }
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        cur = GetNextUsedThing(static_cast<CThing*>(gThingArray[cur].mThing)).mIndex;
    }
    DeletePendingThings();
    mF1DA738 = 1;
}

// 0x1013de70 (504 bytes). Moves the people and the staff (kinds 1, 4 to 8 and 0x12) that are alive; `step` goes through unchanged.
// A NULL thing in the list is skipped, but GetThingId is still asked about it (the Mac would read address 0 there).
void CWorld::UpdateThingsInEngine(float step) {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0) {
            const uint8_t kind = thing->mKind;
            if (kind == 1 || IsStaffKind(kind) || kind == 0x12) reinterpret_cast<CPeep*>(thing)->UpdatePositionInEngine(step);
        }
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013e09c (316 bytes). Draws the cars of every live kind 3 thing (a CObject) whose ride id word at +0x24 is nonzero.
void CWorld::UpdateBumperCarsInEngine(float step) {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0 && thing->mKind == 3 && ReadWord(thing, kObjectRideOffset) != 0)
            BMP_DrawCarsForRide(ReadWord(thing, kObjectRideOffset), step);
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013ff7c (12 bytes). The whole word at +0x1da710 (the constructor sets 1: the park starts closed).
int32_t CWorld::IsParkClosed() const { return static_cast<int32_t>(mF1DA710); }

// 0x1013ffb4 (148 bytes). The first variable of the "gates" object's script: -1 without a gates object (nothing else happens then), else
// whatever RSSE_Getvar answers for variable 1. The object is not tested for NULL.
int32_t CWorld::ParkGates_GetStatus() const {
    const uint16_t gate = mGatesThing;
    if (gate == kNoThing) return -1;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(ReadWord(static_cast<const CThing*>(gThingArray[gate].mThing), kObjectScriptOffset));
    return RSSE_Getvar(script, 1);
}

// 0x1013b278 (872 bytes). One turn: the counter, every live thing's Process, the closing of the park gates when it is closed, nobody is
// inside and no staff member is outside, then the pending deletions, the macro AI and the action recorder.
void CWorld::DoGameTurn() {
    mTurn = mTurn + 1;
    if (mF1DA738 != 4) {
        uint16_t cur = IndexOf(gUsedThingHead);
        while (cur != kNoThing) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
            if (thing != nullptr && thing->mDead == 0) thing->Process();
            const uint16_t id = thing->GetThingId().mIndex;
            cur = IndexOf(gThingArray[id].mNextUsed);
        }
    }
    // Every 30th turn, with a gates object, the park closed, its gates open (status 1) and no visitor inside: if no live staff member is
    // outside the park, close the gates. The scan stops at the first outsider without asking for its id, and does not test for NULL.
    if (mGatesThing != kNoThing && mTurn % 30 == 0 && IsParkClosed() != 0 && ParkGates_GetStatus() == 1 &&
        GetParkAnalyser()->NumberOfVisitorsInPark() == 0) {
        CTRACE(2, kStaffText);
        bool outsider = false;
        uint16_t cur = IndexOf(gUsedThingHead);
        while (cur != kNoThing) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
            CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
            if (thing->mDead == 0 && IsStaffKind(thing->mKind) && reinterpret_cast<CPeep*>(thing)->IsInsidePark() == 0) {
                outsider = true;
                break;
            }
            const uint16_t id = thing->GetThingId().mIndex;
            cur = IndexOf(gThingArray[id].mNextUsed);
        }
        if (!outsider) ParkGates_Close(0);
    }
    DeletePendingThings();
    reinterpret_cast<CMacroAI*>(this)->Update();
    gActionRecorder.DoTick();
}

// 0x1013b608 (116 bytes). A game turn when the game type is the online one (the game type object is built on first use).
void CWorld::DoOnlineGameTurn() {
    if (TheGameType().mType == kGameTypeOnline) DoGameTurn();
}
