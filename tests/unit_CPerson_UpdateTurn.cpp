// Test library for CPerson::UpdateTurn and CPerson::FindNearestFireworks (unit v5217; libtpw_unit_CPerson_UpdateTurn.dylib,
// tests/test_CPerson_UpdateTurn.py), main app:
//   src/CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_UpdateTurn.cpp   (the two functions; gThingListEnd, gQueueLinkA/B)
//
// The callees outside the unit are fakes that record what the real ones read ("this" as an offset from the person) and answer from the
// test's queues. The globals of other files the unit reads are defined here (gWorld, gWorldMap, gThingArray, gPersonTable).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
uint16_t gPersonTable[2] = {0, 25};

// ---- what the fakes compare "this" with (set by the module)
static void* gTestPerson;
static void* gTestMap;
static void* gTestCell;

namespace {

int64_t Off(const void* p, const void* base) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(base)); }

}  // namespace

// ---- the fakes
void CPeep::UpdateTurn() { ev("CPeep::UpdateTurn", {Off(this, gTestPerson)}); }

int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {Off(this, gTestPerson)});
    return static_cast<int32_t>(ret("IsInsidePark"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Off(this, gTestPerson)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}

CRegionEffectCell* CMap::GetRegionalEffectCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("GetRegionalEffectCell", {Off(this, gTestMap), cell.mIndex});
    return static_cast<CRegionEffectCell*>(gTestCell);
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {Off(this, gWorld)});
    return static_cast<uint32_t>(ret("GetRand"));
}

void CPerson::DoSomeThinking() { ev("DoSomeThinking", {Off(this, gTestPerson)}); }
void CPerson::MakeBalloonPop() { ev("MakeBalloonPop", {Off(this, gTestPerson)}); }
void CPerson::UpdateTemporaryHistory() { ev("UpdateTemporaryHistory", {Off(this, gTestPerson)}); }
int32_t CPerson::InQueueingState() const {
    ev("InQueueingState", {Off(this, gTestPerson)});
    return static_cast<int32_t>(ret("InQueueingState"));
}
void CThoughts::ProcessThoughtBubbles() { ev("ProcessThoughtBubbles", {Off(this, gTestPerson)}); }

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    return reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(0x5000 + (script & 0xff)));
}

int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(script)), index});
    return static_cast<int32_t>(ret("RSSE_Getvar"));
}

// The text is logged as the sum of its bytes; the Person text takes four more arguments.
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t sum = 0;
    for (const char* c = format; *c != 0; ++c) sum += static_cast<uint8_t>(*c);
    if (std::strncmp(format, "Person", 6) == 0) {
        va_list args;
        va_start(args, format);
        int32_t a = va_arg(args, int32_t);
        int32_t b = va_arg(args, int32_t);
        int32_t c = va_arg(args, int32_t);
        int32_t d = va_arg(args, int32_t);
        va_end(args);
        ev("GAME_ASSERTMSG", {condition, sum, a, b, c, d});
    } else {
        ev("GAME_ASSERTMSG", {condition, sum});
    }
}

extern "C" {

uint64_t tpw_ut_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gWorldMap", &gWorldMap},
        {"gThingArray", gThingArray},
        {"gPersonTable", gPersonTable},
        {"gThingListEnd", &gThingListEnd},
        {"gQueueLinkA", &gQueueLinkA},
        {"gQueueLinkB", &gQueueLinkB},
        {"tperson", &gTestPerson},
        {"tmap", &gTestMap},
        {"tcell", &gTestCell},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ut_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ut_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<CWorld*>(0x1000)->mTurn) - 0x1000);
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    return n;
}

void tpw_ut_update(uint64_t person) { reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->UpdateTurn(); }

uint32_t tpw_ut_fireworks(uint64_t person) {
    return reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->FindNearestFireworks().mIndex;
}

}  // extern "C"
