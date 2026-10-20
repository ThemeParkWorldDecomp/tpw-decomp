// CWorld::Register, the six overloads (main app, SimTheme Park 2.04b3, World.cpp; unit w139): 0x1013ef14 CHandyman, 0x1013f144 CMechanic,
// 0x1013f374 CEntertainer, 0x1013f5ac CGuard, 0x1013f7d8 CResearcher and 0x1013fa10 CObject, 116 bytes each, all the same code.
//
// The world keeps a singly linked list of each kind of thing, newest first: the head is a halfword thing id in the world. Register reads the
// old head, asks the thing for its id (CThing::GetThingId), makes that id the new head, and only then reads the list's end sentinel (a
// halfword constant in the data, 0 on the Mac: 0x102a46c8 and the next five), and, when the old head was not the sentinel, writes the old head
// into the new thing as its link: +0x20c (a halfword) for the five staff kinds, +0xc for an object.
#include "World.h"

#include <cstdint>

#include "ThingReceivers.h"
#include "analyser.h"

namespace {

constexpr uint16_t kNoThing = 0;               // the Mac reads it from the data (the same value for every list)
constexpr uintptr_t kStaffNextOffset = 0x20c;  // the staff block's link to the next staff member of its kind
constexpr uintptr_t kObjectNextOffset = 0xc;   // the object's link

// The staff classes have no members (the staff block is only reached through the thing), so the thing is viewed by address.
template <class T>
void RegisterThing(T* thing, uint16_t& head, uintptr_t nextOffset) {
    const uint16_t old = head;
    head = reinterpret_cast<CThing*>(thing)->GetThingId().mIndex;
    if (old != kNoThing) {
        *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(thing) + nextOffset) = old;
    }
}

}  // namespace

void CWorld::Register(CHandyman* staff) { RegisterThing(staff, mF1DA73C, kStaffNextOffset); }
void CWorld::Register(CMechanic* staff) { RegisterThing(staff, mF1DA73E, kStaffNextOffset); }
void CWorld::Register(CEntertainer* staff) { RegisterThing(staff, mF1DA740, kStaffNextOffset); }
void CWorld::Register(CGuard* staff) { RegisterThing(staff, mF1DA744, kStaffNextOffset); }
void CWorld::Register(CResearcher* staff) { RegisterThing(staff, mF1DA742, kStaffNextOffset); }
void CWorld::Register(CObject* object) { RegisterThing(object, mF1DA746, kObjectNextOffset); }
