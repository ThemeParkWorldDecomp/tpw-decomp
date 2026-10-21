// Test library for the CPerson queue states and searches (unit v5245; libtpw_unit_CPerson_FindNearestReachableLitterBin.dylib,
// tests/test_CPerson_FindNearestReachableLitterBin.py), main app:
//   src/game/people/PersonQueue.cpp
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue.
// gThingArray and gWorld (defined by other files) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;

extern uint16_t gNoMinorDest;
extern uint16_t gNoQueueBack;
extern uint16_t gLitterListEnd;
extern uint16_t gEntertainerEnd;

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void CTRACE(uint32_t level, const char* format, ...) {
    int n = 0;
    for (const char* p = format; *p; ++p)
        if (p[0] == '%' && p[1] == 'd') ++n;
    // The stuck message has a %d but the original passes no argument: the PowerPC fake records r5 and r6 only when it reads them.
    va_list ap;
    va_start(ap, format);
    int64_t a = 0, b = 0;
    if (n >= 2) {
        a = va_arg(ap, uint32_t);
        b = va_arg(ap, uint32_t);
    }
    va_end(ap);
    if (n >= 2) ev("CTRACE", {level, a, b}, {format});
    else ev("CTRACE", {level}, {format});
}

int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}

int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("SetDest", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("SetDest"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetBackOfQueue() {
    ev("GetBackOfQueue", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetBackOfQueue"));
    return t;
}

void CObject::WalkedAway() { ev("WalkedAway", {lab(this)}); }
void CObject::PersonLeftDuringAdmittance(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) { ev("PersonLeftDuringAdmittance", {lab(this), person.mIndex}); }
void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) { ev("RemovePersonFromQueue", {lab(this), person.mIndex}); }
int32_t CObject::AdmitPerson(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("AdmitPerson", {lab(this), person.mIndex});
    return static_cast<int32_t>(ret("AdmitPerson"));
}

void CPerson::DismissFromQueue() { ev("DismissFromQueue", {lab(this)}); }
int32_t CPerson::FindQueueDestination() {
    ev("FindQueueDestination", {lab(this)});
    return static_cast<int32_t>(ret("FindQueueDestination"));
}
void CPerson::LowerHappiness(Amount amount) { ev("LowerHappiness", {lab(this), static_cast<int32_t>(amount)}); }
void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int32_t>(state)}); }
bool CPerson::TooPricey(CObject* object) const {
    ev("TooPricey", {lab(this), lab(object)});
    return ret("TooPricey") != 0;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = *reinterpret_cast<const uint16_t*>(this);
    return t;
}

void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) { ev("ShowThoughtBubble", {lab(this), static_cast<int32_t>(type), always ? 1 : 0}); }
void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) { ev("RecordEvent", {lab(this), static_cast<int32_t>(type), thing.mIndex}); }

extern "C" {

uint64_t tpw_pq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"nominor", &gNoMinorDest},
        {"noback", &gNoQueueBack},
        {"litter_end", &gLitterListEnd},
        {"ent_end", &gEntertainerEnd},
        {"array", &gThingArray[0]},
        {"world", &gWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(gThingArray) / sizeof(CThingArrayEntry));
    return n;
}

uint32_t tpw_pq_litter(uint64_t self, uint32_t range) {
    return reinterpret_cast<CPerson*>(static_cast<uintptr_t>(self))->FindNearestReachableLitterBin(range).mIndex;
}
uint32_t tpw_pq_entertainer(uint64_t self) { return reinterpret_cast<CPerson*>(static_cast<uintptr_t>(self))->FindNearestEntertainer().mIndex; }
void tpw_pq_unloaded(uint64_t self) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(self))->ProcessStateBeingUnloaded(); }
void tpw_pq_loaded(uint64_t self) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(self))->ProcessStateBeingLoaded(); }

}  // extern "C"
