// Test library for CPeep::CPeep (unit v5556; libtpw_unit_CPeep_CPeep.dylib, tests/test_CPeep_CPeep.py), main app:
//   src/CPeep_..._CPeep_CPeep.cpp   the constructor.
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "World.h"
#include "CMessageCentre.h"
#include "LbSCBack.h"

using namespace calllog;

uint16_t gPersonTable[2] = {0, 25};
CMessageCentre* gMessageCentre;
CWorld* gWorld;

// operator new is the library's own (the arena replaces it); the PowerPC "new" stub still records the size, the host cannot.

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count) {
    (void)ctor;
    (void)dtor;
    ev("array", {lab(array), size, count});
}

CThing::CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, ThingModel model) {
    ev("CThing", {lab(this), static_cast<int64_t>(cell.mIndex), static_cast<int64_t>(static_cast<uint32_t>(model))});
}
CDelayedScript::CDelayedScript() { ev("CDelayedScript", {lab(this)}); }
CThoughts::CThoughts() { ev("CThoughts", {lab(this)}); }
Vehicle::~Vehicle() {}  // referenced by the Pilot placement new's cleanup; never run
Pilot::Pilot() { ev("Pilot", {lab(this)}); }
void Pilot::reset(int32_t x, int32_t y, int32_t vx, int32_t vy) {
    ev("reset", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(x)), static_cast<int64_t>(static_cast<uint32_t>(y)),
                 static_cast<int64_t>(static_cast<uint32_t>(vx)), static_cast<int64_t>(static_cast<uint32_t>(vy))});
}
void Pilot::get_pos(int32_t& x, int32_t& y) {
    ev("get_pos", {lab(this), lab(&x), lab(&y)});
    x = static_cast<int32_t>(ret("gx"));
    y = static_cast<int32_t>(ret("gy"));
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), static_cast<int64_t>(static_cast<uint32_t>(type))});
    return false;
}

extern "C" {

uint64_t tpw_peep_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gMessageCentre", &gMessageCentre},
        {"gPersonTable", &gPersonTable[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_peep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_peep_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>));
    return n;
}

void tpw_peep_ctor(uint64_t self, uint32_t cell, int32_t model) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(cell);
    ::new (reinterpret_cast<void*>(static_cast<uintptr_t>(self))) CPeep(tag, static_cast<CThing::ThingModel>(model));
}

}  // extern "C"
