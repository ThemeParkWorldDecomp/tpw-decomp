// CWorld::Unregister, the six overloads (main app, SimTheme Park 2.04b3, World.cpp; unit w178): 0x1013efb8 CHandyman, 0x1013f1e8 CMechanic,
// 0x1013f41c CEntertainer, 0x1013f650 CGuard, 0x1013f880 CResearcher and 0x1013fab4 CObject, 344 bytes each, all the same code.
//
// The reverse of CWorld::Register (src/World_register.cpp): the world's list of one kind of thing runs from the newest to the oldest through a
// halfword thing id in each member (+0x20c for the five staff kinds, +0xc for an object). Unregister takes the thing out of the list: when it is
// the head the head becomes its link; otherwise the walk from the head finds the member just before it and gives that member the thing's link.
// Every step of the walk (and the unlinking) first asserts that gThingArray[0] holds no thing, with the message below. The order of the reads
// around those assertions is the listing's: the head is read before the first assertion and its member's link after it; the thing's link is
// read after the last assertion.
#include "World.h"

#include <cstdint>

#include "Debug.h"
#include "ThingReceivers.h"
#include "analyser.h"

namespace {

constexpr uintptr_t kStaffNextOffset = 0x20c;  // the staff block's link to the next staff member of its kind
constexpr uintptr_t kObjectNextOffset = 0xc;   // the object's link

void AssertRowZeroEmpty() {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
}

uint16_t& LinkOf(uint16_t id, uintptr_t nextOffset) {
    return *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(gThingArray[id].mThing) + nextOffset);
}

template <class T>
void UnregisterThing(T* thing, uint16_t& head, uintptr_t nextOffset) {
    const uint16_t id = reinterpret_cast<CThing*>(thing)->GetThingId().mIndex;
    uint16_t current = head;
    if (current == id) {
        const uint16_t first = head;
        AssertRowZeroEmpty();
        head = LinkOf(first, nextOffset);
    } else {
        uint16_t previous;
        do {
            previous = current;
            AssertRowZeroEmpty();
            current = LinkOf(previous, nextOffset);
        } while (current != id);
        AssertRowZeroEmpty();
        void* object = gThingArray[id].mThing;
        AssertRowZeroEmpty();
        const uint16_t link = *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(object) + nextOffset);
        LinkOf(previous, nextOffset) = link;
    }
}

}  // namespace

void CWorld::Unregister(CHandyman* staff) { UnregisterThing(staff, mF1DA73C, kStaffNextOffset); }
void CWorld::Unregister(CMechanic* staff) { UnregisterThing(staff, mF1DA73E, kStaffNextOffset); }
void CWorld::Unregister(CEntertainer* staff) { UnregisterThing(staff, mF1DA740, kStaffNextOffset); }
void CWorld::Unregister(CGuard* staff) { UnregisterThing(staff, mF1DA744, kStaffNextOffset); }
void CWorld::Unregister(CResearcher* staff) { UnregisterThing(staff, mF1DA742, kStaffNextOffset); }
void CWorld::Unregister(CObject* object) { UnregisterThing(object, mF1DA746, kObjectNextOffset); }
