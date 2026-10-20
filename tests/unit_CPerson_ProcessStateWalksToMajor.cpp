// Test library for CPerson::ProcessStateWalksToMajor and CPerson::DoSomeThinking (unit v5563; libtpw_unit_CPerson_ProcessStateWalksToMajor.dylib,
// tests/test_CPerson_ProcessStateWalksToMajor.py), main app:
//   src/CPerson_..._CPerson_ProcessStateWalksToMajor.cpp   both functions, and the globals gNoMajorRide, gNoMajorCell
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files (gThingArray, gWorld, gSoundBanks) are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "World.h"
#include "ThingParts2.h"
#include "TPSound.h"

using namespace calllog;

CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld = nullptr;
int32_t gSoundBanks[8];

namespace {

alignas(16) uint8_t sPerson[0x240];
alignas(16) uint8_t sObject[0x40];
alignas(16) uint8_t sWorld[0x40];
alignas(16) uint8_t sAnalyser[0x40];
alignas(16) uint8_t sCell[0x40];

uint32_t Sum(const char* text) {
    uint32_t sum = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) sum += *p;
    return sum;
}

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingTag;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> MapTag;

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {static_cast<int64_t>(static_cast<uint32_t>(condition)), static_cast<int64_t>(Sum(format))});
}

void CTRACE(uint32_t level, const char* format, ...) {
    int count = 0;
    for (const char* p = format; *p; ++p)
        if (*p == '%') ++count;
    va_list args;
    va_start(args, format);
    int64_t a = 0, b = 0;
    if (count >= 1) a = va_arg(args, uint32_t);
    if (count >= 2) b = va_arg(args, uint32_t);
    va_end(args);
    if (count == 0) ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(Sum(format))});
    else if (count == 1) ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(Sum(format)), a});
    else ev("CTRACE", {static_cast<int64_t>(level), static_cast<int64_t>(Sum(format)), a, b});
}

int32_t CPeep::PeepWalks() {
    ev("PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("PeepWalks"));
}
int32_t CPeep::SetDest(MapTag cell) {
    ev("SetDest", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("SetDest"));
}
void CPeep::PlaySound(int32_t category, int32_t sound) {
    ev("PlaySound", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(category)), sound});
}

MapTag CObject::GetBackOfQueue() {
    ev("GetBackOfQueue", {lab(this)});
    return MapTag{static_cast<uint16_t>(ret("GetBackOfQueue"))};
}
int32_t CObject::HasFreeQueueSpace() const {
    ev("HasFreeQueueSpace", {lab(this)});
    return static_cast<int32_t>(ret("HasFreeQueueSpace"));
}
uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {lab(this), flag});
    return static_cast<uint8_t>(ret("GetExcitementLevel"));
}
bool CObject::QueueTooLong() const {
    ev("QueueTooLong", {lab(this)});
    return ret("QueueTooLong") != 0;
}
void CObject::AddPersonToQueue(ThingTag person) { ev("AddPersonToQueue", {lab(this), person.mIndex}); }
void CObject::RemovePersonFromQueue(ThingTag person) { ev("RemovePersonFromQueue", {lab(this), person.mIndex}); }

ThingTag CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    return ThingTag{static_cast<uint16_t>(lab(this) == 1 ? ret("ThingId1") : ret("ThingId2"))};
}

void CThoughts::RecordEvent(EventType type, ThingTag thing) { ev("RecordEvent", {lab(this), static_cast<int64_t>(type), thing.mIndex}); }
void CThoughts::ShowThoughtBubble(ThoughtType type, bool always) { ev("ShowThoughtBubble", {lab(this), static_cast<int64_t>(type), always}); }

void CPerson::LowerHappiness(Amount amount) { ev("LowerHappiness", {lab(this), static_cast<int64_t>(amount)}); }
void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int64_t>(state)}); }
void CPerson::DismissFromQueue() { ev("DismissFromQueue", {lab(this)}); }
int32_t CPerson::FindQueueDestination() {
    ev("FindQueueDestination", {lab(this)});
    return static_cast<int32_t>(ret("FindQueueDestination"));
}
int32_t CPerson::ScoreOnCloserInspection(CObject* object) {
    ev("ScoreOnCloserInspection", {lab(this), lab(object)});
    return static_cast<int32_t>(ret("Score"));
}
void CPerson::AddRideToRecentTemporaryHistory(ThingTag ride) { ev("AddRide", {lab(this), ride.mIndex}); }
void CPerson::LookForMinorDest() { ev("LookForMinorDest", {lab(this)}); }

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(this)});
    return static_cast<int32_t>(ret("ParkClosed"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(sAnalyser);
}
CParkAnalyserCell* CParkAnalyser::GetAnalyserCell(MapTag id) const {
    ev("GetAnalyserCell", {lab(this), id.mIndex});
    return reinterpret_cast<CParkAnalyserCell*>(sCell);
}
int32_t CParkAnalyserCell::GetNumWalkableCells(uint32_t maxAge) const {
    ev("GetNumWalkableCells", {lab(this), maxAge});
    return static_cast<int32_t>(ret("Walkable"));
}
int32_t CParkAnalyserCell::GetNumLitteredCells(uint32_t maxAge) const {
    ev("GetNumLitteredCells", {lab(this), maxAge});
    return static_cast<int32_t>(ret("Littered"));
}

extern "C" {

uint64_t tpw_wtm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"person", sPerson},          {"object", sObject},          {"world", sWorld},
        {"analyser", sAnalyser},      {"cell", sCell},              {"gThingArray", gThingArray},
        {"gWorld", &gWorld},          {"gSoundBanks", gSoundBanks}, {"gNoMajorRide", &gNoMajorRide},
        {"gNoMajorCell", &gNoMajorCell},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_wtm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_wtm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(gSoundBanks[0]));
    return n;
}

void tpw_wtm_walks(uint64_t person) { reinterpret_cast<CPerson*>(person)->ProcessStateWalksToMajor(); }
void tpw_wtm_think(uint64_t person) { reinterpret_cast<CPerson*>(person)->DoSomeThinking(); }

}  // extern "C"
