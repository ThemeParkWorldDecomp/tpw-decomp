// CPerson queue states and the nearest-thing searches (unit v5245):
//   FindNearestReachableLitterBin 0x1011e9ec, FindNearestEntertainer 0x1011ebc4,
//   ProcessStateBeingUnloaded 0x1011e0e4, ProcessStateBeingLoaded 0x1011dcc0.
// The person is reached by Mac offset (+4 / +6 cell bytes, +0x30 the CThoughts, +0x1dc ride halfword, +0x1de minor destination halfword).
#include <cstdint>
#include <cstdlib>

#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"

// Halfword sentinels (NEW, no other file defines them); the Mac's bss is zero at start.
uint16_t gNoMinorDest;      // 0x102a1000
uint16_t gNoQueueBack;      // 0x102a1002
uint16_t gLitterListEnd;    // 0x102a1004
uint16_t gEntertainerEnd;   // 0x102a1006

namespace {

constexpr char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

uint8_t* Bytes(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }
uint16_t& H(uint8_t* base, std::size_t off) { return *reinterpret_cast<uint16_t*>(base + off); }

uint8_t* ThingAt(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZeroText);
    return Bytes(gThingArray[id].mThing);
}

uint32_t CellOf(const uint8_t* p) {
    return static_cast<uint16_t>(p[4] + (static_cast<uint32_t>(p[6]) << 7) + 1) - 1u;
}

uint32_t Distance(uint32_t a, uint32_t b) {
    int32_t dx = static_cast<int32_t>(std::labs(static_cast<int32_t>((a & 0x7fu) - (b & 0x7fu))));
    int32_t dy = static_cast<int32_t>(std::labs(static_cast<int32_t>((a >> 7) - (b >> 7))));
    return static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
}

}  // namespace

// 0x1011e9ec: the hidden result pointer is r3, `this` r4 and the range r5 in the listing.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CPerson::FindNearestReachableLitterBin(uint32_t range) {
    uint8_t* self = Bytes(this);
    uint32_t bestDistance = 0xffffffffu;
    uint16_t current = H(Bytes(gWorld), 0x1da746);
    uint16_t best = 0;
    while (current != gLitterListEnd) {
        uint8_t* thing = ThingAt(current);
        if ((H(thing, 0x2e) & 0x40u) != 0) {
            uint32_t distance = Distance(CellOf(thing), CellOf(self));
            if (distance < range * range && distance < bestDistance) {
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
                cell.mIndex = H(thing, 0x32);
                if (reinterpret_cast<CPeep*>(this)->SetDest(cell) != 0) {
                    best = current;
                    bestDistance = distance;
                }
            }
        }
        current = H(thing, 0xc);
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = best;
    return result;
}

// 0x1011ebc4: the hidden result pointer is r3, `this` r4.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CPerson::FindNearestEntertainer() {
    uint8_t* self = Bytes(this);
    uint32_t bestDistance = 0xffffffffu;
    uint16_t current = H(Bytes(gWorld), 0x1da740);
    uint16_t best = 0;
    while (current != gEntertainerEnd) {
        uint8_t* thing = ThingAt(current);
        uint32_t distance = Distance(CellOf(thing), CellOf(self));
        if (distance < bestDistance) {
            best = current;
            bestDistance = distance;
        }
        current = H(thing, 0x20c);
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = best;
    return result;
}

// 0x1011e0e4
void CPerson::ProcessStateBeingUnloaded() {
    uint8_t* self = Bytes(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 0) {
        if (H(self, 0x1de) != gNoMinorDest) {
            H(self, 0x1dc) = H(self, 0x1de);
            H(self, 0x1de) = 0;
            uint16_t ride = H(self, 0x1dc);
            uint8_t* object = ThingAt(ride);
            if (object[2] == 3) {
                uint8_t* again = ThingAt(H(self, 0x1dc));
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> back = reinterpret_cast<CObject*>(again)->GetBackOfQueue();
                if (back.mIndex != gNoQueueBack && reinterpret_cast<CPeep*>(this)->SetDest(back) != 0) {
                    CTRACE(2, "Left minor destination, found old major one again!");
                    SetState(static_cast<PersonState>(10));
                    return;
                }
            } else {
                CTRACE(2, "Deleted major dest while I was doing minor dest!\n");
                H(self, 0x1dc) = 0;
            }
        }
        uint16_t ride = H(self, 0x1dc);
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, "Person %d: successfully left ride %d, becoming idle\n", static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
        SetState(static_cast<PersonState>(6));
    } else if (walks == 2) {
        H(self, 0x1dc) = 0;
        SetState(static_cast<PersonState>(6));
    }
}

// 0x1011dcc0
void CPerson::ProcessStateBeingLoaded() {
    uint8_t* self = Bytes(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 2) {
        CTRACE(2, "Person %d: Got stuck in middle of very short queue step - this shouldn't happen\n");
        walks = 0;
    }
    if (walks != 0) return;
    uint8_t* raw = ThingAt(H(self, 0x1dc));
    CObject* object = reinterpret_cast<CObject*>(raw);
    if (TooPricey(object)) {
        uint16_t ride = H(self, 0x1dc);
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, "Person %d: Object %d is too expensive, I'm leaving the queue\n", static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
        CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(10), reinterpret_cast<CThing*>(object)->GetThingId());
        LowerHappiness(static_cast<Amount>(1));
        object->WalkedAway();
        object->PersonLeftDuringAdmittance(me->GetThingId());
        object->RemovePersonFromQueue(me->GetThingId());
        DismissFromQueue();
        H(self, 0x1dc) = 0;
        SetState(static_cast<PersonState>(6));
        return;
    }
    if (object->AdmitPerson(me->GetThingId()) != 0) {
        uint16_t ride = H(self, 0x1dc);
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, "Person %d been AdmitPerson'd to ride %d, now waiting for script to admit me\n", static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
        SetState(static_cast<PersonState>(14));
        return;
    }
    if (FindQueueDestination() == 0) {
        CTRACE(2, "Couldn't rejoin FOQ even!\n");
        object->RemovePersonFromQueue(me->GetThingId());
        DismissFromQueue();
        SetState(static_cast<PersonState>(6));
    }
}
