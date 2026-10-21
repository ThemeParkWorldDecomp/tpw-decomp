// The world's sequences and small helpers (main app, SimTheme Park 2.04b3, World.cpp; unit w194): CWorld::PopulatePark,
// StartingPopulationForOnline, StartBankruptcySequence, StartWinningSequence, IsPlayingWinningAnim, EndWinningSequence, GetPrevUsedThing,
// SeedRand, ReconstructFreeList, GetNewPersonIndex, FlagItemAsSelected, ConvertDLRidesUsedToIndices, RestoreEngineObjects and
// DeleteEngineObjects, and the pending list's TbTList::RemoveHead and AddHead (the list of thing ids that CThing::KillMe queues).
//
// The three engine walks (the bankruptcy, Restore and Delete) follow the used list of the thing table exactly as src/World_turn.cpp's
// UpdateThingsInEngine does: each row asserts that entry 0 is free, then the thing is tested, then the walk follows the link of the row
// named by the THING'S OWN ID (GetThingId is called even for a NULL thing). The release GAME_ASSERTMSG does nothing.
#include "World.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "ESP_sprites.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "WorldThings.h"
#include "analyser.h"
#include "platform/arena.h"
#include "platform/macdoze.h"

// The two globals of World.cpp this unit owns (original names unknown): the id of the thing that plays the winning animation (the "End"
// feature's object; 0 for none) and the ride database name of that feature. __sinit_World_cpp (0x10141c14) builds the string on the Mac;
// until it is translated it is defined here with its text, as World_setup.cpp does for the gates and the lights.
uint16_t gWinningAnimThing = 0;                                // 0x1034cad4
TbDynamicStringTemplate<char16_t> gWorldEndName(u"End");       // 0x1034cad8

namespace {

// The "invalid thing id" that ends a walk. The Mac reads it from separate 16 bit words in initialised data (0x102a4618 for
// StartBankruptcySequence, 0x102a461a for IsPlayingWinningAnim, 0x102a461c for EndWinningSequence, 0x102a46c4 for RestoreEngineObjects and
// 0x102a46c6 for DeleteEngineObjects), all 0 and never written.
constexpr uint16_t kNoThing = 0;

// The strings at 0x10247498 + the offset in the comment.
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x151
const char kNoFeatureText[] = "Could not find the %s feature\n";                                          // +0x2d1
const char kEndBeforeStartText[] = "Calling EndWinningSequence before StartWinningSequence\n";           // +0x2f0
const char kVisitorText[] = "Your park has received its %dth visitor!\n";                                 // +0x77f
const char kNotInDatabaseText[] = "You've managed to build a ride without it being in the ride database";   // +0x7a9 (no newline)
const char kDigits[] = "0123456789";                                                                      // +0x7ee

// The index word of an entry, cut to 16 bits (the Mac loads the whole word and stores a halfword); 0 for no entry.
uint16_t IndexOf(const CThingArrayEntry* entry) { return entry != nullptr ? static_cast<uint16_t>(entry->mF04[0]) : uint16_t(0); }

int32_t ReadWord(const CThing* thing, std::size_t offset) {
    int32_t word;
    std::memcpy(&word, reinterpret_cast<const unsigned char*>(thing) + offset, sizeof word);
    return word;
}

// The staff kinds 4 to 8 (the Mac builds this as a chain of tests on 4, 5, 6, 8 and 7).
bool IsStaffKind(uint8_t kind) { return kind >= 4 && kind <= 8; }

// The world's generator as the Mac inlines it into PopulatePark and StartingPopulationForOnline (the same as CWorld::GetRand, which these
// two do not call): state = state * 0x19660d + 0x3c6ef35f, stored; the answer is the state's absolute value.
uint32_t NextRand(uint32_t& state) {
    state = state * 0x0019660du + 0x3c6ef35fu;
    return static_cast<int32_t>(state) >= 0 ? state : 0u - state;
}

// A thing of `size` bytes (the Mac's size: the host classes are placeholders), made by its constructor over the block; the result is dropped
// (the thing enters the thing table itself). The Mac tests the allocation for NULL and skips the constructor then; the host's never fails.
template <class T, class... Args> void MakeThing(std::size_t size, Args... args) {
    void* block = tpw::ArenaAlloc(size);   // the Mac's game heap: the arena (a list keeps a thing's address in 32 bits)
    new (block) T(args...);
}

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

// The first ride whose item name is `name` (ignoring case), walking the database from its first ride; NULL when none is. _wcsicmp is not
// called for a NULL ride.
RideInfo* FindRide(const TbDynamicStringTemplate<char16_t>& name) {
    RideInfo* ride = gRideDatabase.GetFirstRide();
    while (ride != nullptr && NS_MacDoze::_wcsicmp(ride->mItemName, name.mData) != 0) ride = gRideDatabase.GetNextRide();
    return ride;
}

}  // namespace

// 0x1013aeb0 (112 bytes). The id of the used entry before the one of the thing's own id, 0 at the head of the list (the twin of
// GetNextUsedThing). The thing is not tested for NULL and the world is not read.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetPrevUsedThing(const CThing* thing) const {
    const uint16_t id = thing->GetThingId().mIndex;
    return {IndexOf(gThingArray[id].mPrevUsed)};
}

// 0x1013b244 (12 bytes). Seeds the generator.
void CWorld::SeedRand(uint32_t seed) { mStartTime = seed; }

// 0x1013dd14 (96 bytes). Rebuilds the free list from the table: every entry from 1 to 0x27ff gets its next-free link cleared and, when it
// holds no thing, is chained after the previous free one (the first one becomes the head). Entry 0 is never touched, and the head is not
// written when no entry is free.
void CWorld::ReconstructFreeList() {
    uint32_t last = 0xffffffffu;
    for (uint32_t i = 1; i < 0x2800; ++i) {
        gThingArray[i].mNextFree = nullptr;
        if (gThingArray[i].mThing == nullptr) {
            if (last == 0xffffffffu) gFreeThingHead = &gThingArray[i];
            else gThingArray[last].mNextFree = &gThingArray[i];
            last = i;
        }
    }
}

// 0x10140e64 (80 bytes). Counts the people made: the trace shows the count and the answer is the count read back.
int32_t CWorld::GetNewPersonIndex() {
    ++mPersonCount;
    CTRACE(2, kVisitorText, mPersonCount);
    return static_cast<int32_t>(mPersonCount);
}

// 0x10140ee4 (104 bytes). Sets the bit of the ride's downloaded ride number (1 based) in the park's ride bitmap; a ride that has no number
// (0) sets nothing. The ride record is not tested for NULL.
void CWorld::FlagItemAsSelected(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    const int32_t number = gRideDatabase.GetRideInfo(id)->Value(kRideSlot_DownloadedRideNumber).mInteger;
    if (number != 0) gParkRequiredRides[static_cast<uint32_t>(number - 1) >> 3] |= static_cast<uint8_t>(1 << ((number - 1) & 7));
}

// 0x1014113c (304 bytes). Writes, for every bit set in the park's ride bitmap, the number found in the last folder name of the ride that
// has that downloaded ride number ("x\ride12.wad" gives 12), in bitmap order; the rest of the 0x200 byte array is zeroed first. A bit
// whose ride is not in the database asserts and adds nothing. The answer is always 0. The folder must hold a backslash and, after the last
// one, a digit (strrchr and strpbrk are not tested for NULL, as on the Mac).
int32_t CWorld::ConvertDLRidesUsedToIndices(uint16_t* out) {
    std::memset(out, 0, 0x200);
    int32_t k = 0;
    for (int32_t byte = 0; byte < 0x20; ++byte) {
        for (int32_t bit = 0; bit < 8; ++bit) {
            if (((1 << bit) & static_cast<int32_t>(static_cast<signed char>(gParkRequiredRides[byte]))) == 0) continue;
            RideInfo* ride = gRideDatabase.GetFirstRide();
            while (ride != nullptr && ride->Value(kRideSlot_DownloadedRideNumber).mInteger != bit + byte * 8 + 1)
                ride = gRideDatabase.GetNextRide();
            GAME_ASSERTMSG(ride != nullptr, kNotInDatabaseText);
            if (ride != nullptr) {
                const char* name = std::strrchr(ride->mFolder, '\\');
                name = std::strpbrk(name, kDigits);
                char* end;
                out[k++] = static_cast<uint16_t>(std::strtol(name, &end, 10));
            }
        }
    }
    return 0;
}

// 0x1013bdd0 (40 bytes). True while the "End" feature's object plays its animation.
bool CWorld::IsPlayingWinningAnim() { return gWinningAnimThing != kNoThing; }

// 0x1013bc68 (308 bytes). Looks the "End" feature up in the ride database and makes its object (a CObject, 0x448 bytes, on map cell 1);
// its thing id is kept in gWinningAnimThing. When the database has none the assertion fires (its copy of the name is passed to the
// assertion by value on the Mac) and the id is cleared. The congratulation window is told either way, with a NULL object when none.
void CWorld::StartWinningSequence() {
    CObject* object = nullptr;
    RideInfo* ride = FindRide(gWorldEndName);
    if (ride == nullptr) {
        const TbDynamicStringTemplate<char16_t> copy(gWorldEndName);
        GAME_ASSERTMSG(0, kNoFeatureText, copy.mData);
        gWinningAnimThing = 0;
    } else {
        void* block = tpw::ArenaAlloc(0x448);
        object = new (block) CObject(MapId{1}, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride->mId}, 0, 0);
        gWinningAnimThing = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
    }
    gUIInterface.Congratulations(object);
}

// 0x1013be2c (188 bytes). Kills the winning animation's thing (the table row of gWinningAnimThing; not tested for NULL), clears the id and
// leaves the first person view of a ride.
void CWorld::EndWinningSequence() {
    GAME_ASSERTMSG(gWinningAnimThing != kNoThing, kEndBeforeStartText);
    const uint16_t id = gWinningAnimThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    static_cast<CThing*>(gThingArray[id].mThing)->KillMe();
    gWinningAnimThing = 0;
    StopRidingRideIn1stPerson();
}

// 0x1013baa4 (396 bytes). The state word becomes 4, the congratulation window of bankruptcy is told (with the gates object, NULL when its
// row is empty) and the sprites stop drawing; every live ride (kind 3) whose flag 4 (choosable) is set is asked to break down; the pending
// things are deleted and the park is closed.
void CWorld::StartBankruptcySequence() {
    const uint16_t gatesId = mGatesThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
    CObject* gates = reinterpret_cast<CObject*>(gThingArray[gatesId].mThing);
    mF1DA738 = 4;
    gUIInterface.Bankrupt(gates);
    ESP_DisableSpriteDraw();
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0 && thing->mKind == 3) {
            CObject* object = reinterpret_cast<CObject*>(thing);
            if ((object->mFlags & 4) != 0) object->RequestBreakdown();
        }
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
    DeletePendingThings();
    SetParkOpenOrClosed(0, 1);
}

// 0x1013e210 (292 bytes). Every live ride (kind 3) is told to update its engine object (flag 1).
void CWorld::RestoreEngineObjects() {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0 && thing->mKind == 3) reinterpret_cast<CObject*>(thing)->UpdateEngine(1);
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013e368 (528 bytes). Every live thing of kind 1 (a person), 4 to 8 (the staff) or 0x12 whose word at +0xc (its CDelayedScript's
// script handle) is not 0 gets its sprite script killed (the host classes do not name the member: the address of +0xc is a
// CDelayedScript); every other live ride (kind 3) gives its ride instance (the word at +0x1c, CObject::mF1C) back.
void CWorld::DeleteEngineObjects() {
    uint16_t cur = IndexOf(gUsedThingHead);
    while (cur != kNoThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing != nullptr && thing->mDead == 0) {
            const uint8_t kind = thing->mKind;
            if (kind == 1 || IsStaffKind(kind) || kind == 0x12) {
                if (ReadWord(thing, 0xc) != 0)
                    reinterpret_cast<CDelayedScript*>(reinterpret_cast<unsigned char*>(thing) + 0xc)->KillSpriteScript();
            } else if (kind == 3) {
                DestroyRideInstance(reinterpret_cast<CObject*>(thing)->mF1C);
            }
        }
        const uint16_t id = thing->GetThingId().mIndex;
        cur = IndexOf(gThingArray[id].mNextUsed);
    }
}

// 0x1013b6ac (436 bytes). Fills the park with its first people: when the gates are not open (status other than 1) the park is only opened
// (SetParkOpenOrClosed(1, 0)). Else every map cell (x outer, y inner) that is inside the level and is a park entrance makes a mechanic
// (even random) or a handyman (odd), and a path cell makes a person; nothing else does. The things enter the thing table themselves.
void CWorld::PopulatePark() {
    if (ParkGates_GetStatus() != 1) {
        SetParkOpenOrClosed(1, 0);
        return;
    }
    for (int32_t x = 0; x < 0x80; ++x) {
        for (int32_t y = 0; y < 0x80; ++y) {
            if (CMap::IsXYValid(x, y) == 0) continue;
            const uint16_t cell = static_cast<uint16_t>(x + y * 0x80 + 1);
            const CMapCell& c = mMapCells[cell - 1];
            if (c.IsEntrance() != 0) {
                if ((NextRand(mStartTime) & 1) == 0) MakeThing<CMechanic>(0x218, MapId{cell});
                else MakeThing<CHandyman>(0x218, MapId{cell});
            } else if (c.IsPath() != 0) {
                MakeThing<CPerson>(0x22c, MapId{cell});
            }
        }
    }
}

// 0x1013b88c (476 bytes). The people of an online park: the park is opened, then 25 people at the ticket booths, then for every ride in the
// park (the analyser's count; none for 0 or less) a mechanic and an entertainer at the park entrances. The order of each is: the block, the
// random number, the position, the constructor.
void CWorld::StartingPopulationForOnline() {
    SetParkOpenOrClosed(1, 0);
    for (int32_t i = 0; i < 25; ++i) {
        void* block = tpw::ArenaAlloc(0x22c);
        const uint32_t r = NextRand(mStartTime);
        new (block) CPerson(GetMap()->GetTicketBoothPos(static_cast<int32_t>(r & 1)));
    }
    const int32_t rides = GetParkAnalyser()->NumberOfObjectTypeInPark(kUIObjRide, 0);
    for (int32_t i = 0; i < rides; ++i) {
        void* mechanic = tpw::ArenaAlloc(0x218);
        uint32_t r = NextRand(mStartTime);
        new (mechanic) CMechanic(GetMap()->GetParkEntrancePos(static_cast<int32_t>(r & 1)));
        void* entertainer = tpw::ArenaAlloc(0x214);
        r = NextRand(mStartTime);
        new (entertainer) CEntertainer(GetMap()->GetParkEntrancePos(static_cast<int32_t>(r & 1)));
    }
}

// 0x101414a4 (136 bytes). The pending list's RemoveHead: the head node leaves the list, the list counts down, the node is freed and the
// thing id it held is answered. An empty list answers a zero id (the Mac answers without writing its result: the caller sees whatever its
// result slot held).
template <>
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::RemoveHead() {
    if (mHead == nullptr) return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{};
    CNode* node = mHead;
    mHead = node->mNext;
    if (mHead == nullptr) mTail = nullptr;
    else mHead->mPrev = nullptr;
    const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> value = node->mData;
    --mCount;
    delete node;
    return value;
}

// 0x10141ad4 (140 bytes). The pending list's AddHead: a new node (10 bytes on the Mac) becomes the head and the list counts up.
template <>
void TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::AddHead(
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> value) {
    CNode* node = new CNode(value);
    node->mNext = mHead;
    if (mHead == nullptr) mTail = node;
    else mHead->mPrev = node;
    mHead = node;
    ++mCount;
}
