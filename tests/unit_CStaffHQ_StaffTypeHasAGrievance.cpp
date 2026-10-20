// Test library for CStaffHQ::StaffTypeHasAGrievance (unit v5947; libtpw_unit_CStaffHQ_StaffTypeHasAGrievance.dylib,
// tests/test_CStaffHQ_StaffTypeHasAGrievance.py), main app:
//   src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise_CStaffHQ_StaffTypeHasAGrievance.cpp
//   the function and gStaffHQGrievanceEnd.
//
// The callees outside the unit (CWorld::GetParkAnalyser, CParkAnalyser::GetNumWalkableCells / GetNumLitteredCells, GAME_ASSERTMSG, CTRACE)
// are recording fakes answering from the test's queues. gWorld and gThingArray are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

CWorld* gWorld;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

// The ids the Python module gives the texts (the same table).
const char* const kTexts[] = {
    "Thing has been allocated in ThingArray index 0 - shouldn't happen\n",
    "%ss are striking through fatigue\n",
    "##Type",
    "%ss are striking through unhappiness\n",
    "Unknown staff type in StaffTypeHasAGrievance\n",
};

int64_t TextId(const char* text) {
    if (!text) return -2;
    for (int i = 0; i < 5; i++)
        if (std::strcmp(kTexts[i], text) == 0) return i;
    return -1;
}

}  // namespace

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {this == gWorld ? 1 : 0});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x1000));
}

int32_t CParkAnalyser::GetNumWalkableCells(uint32_t maxAge) const {
    ev("GetNumWalkableCells", {static_cast<int64_t>(maxAge)});
    return static_cast<int32_t>(ret("walk"));
}

int32_t CParkAnalyser::GetNumLitteredCells(uint32_t maxAge) const {
    ev("GetNumLitteredCells", {static_cast<int64_t>(maxAge)});
    return static_cast<int32_t>(ret("litter"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {condition, TextId(format)});
}

void CTRACE(uint32_t level, const char* format, ...) {
    // the third argument is the first variadic one: the type name text
    __builtin_va_list args;
    __builtin_va_start(args, format);
    const char* arg = __builtin_va_arg(args, const char*);
    __builtin_va_end(args);
    ev("trace", {static_cast<int64_t>(static_cast<int32_t>(level)), TextId(format), TextId(arg)});
}

extern "C" {

uint64_t tpw_sg_addr(const char* name) {
    if (std::strcmp(name, "ends") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gStaffHQGrievanceEnd));
    return 0;
}

uint64_t tpw_sg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ, mF64));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73C));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73E));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA740));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA742));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA744));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    return n;
}

void tpw_sg_set_thing(uint32_t index, uint64_t thing) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }

void tpw_sg_set_world(uint64_t world) { gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }

int32_t tpw_sg_run(uint64_t hq, int32_t type) {
    return reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(hq))->StaffTypeHasAGrievance(static_cast<StaffType>(type));
}

}  // extern "C"
