// Test library for World.cpp's static initialiser (libtpw_unit_sinit_World_cpp.dylib, tests/test_sinit_World_cpp.py), main app:
//   src/game/World.cpp   __sinit_World_cpp
// Callees (constructors, __construct_array, __register_global_object) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "World.h"
#include "WorldThings.h"
#include "LbSCBack.h"
#include "AdvisorScoring.h"

using namespace calllog;

typedef TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>> PendingList;

template<> PendingList::TbTList() { ev("ListCtor", {lab(this)}); }
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("StringCtor", {lab(this)}, {narrow(text).c_str()});
}

typedef TbDynamicStringTemplate<char16_t> WStr;
template<> void WStr::AllocBuffer(uint32_t) {}
template<> void WStr::Format(const char16_t*, ...) {}
template<> void WStr::ResizeTo(uint32_t) {}
template<> WStr::~TbDynamicStringTemplate() {}
template<> WStr& WStr::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> WStr& WStr::operator=(const WStr&) { return *this; }
template<> WStr& WStr::operator=(const char16_t*) { return *this; }
template<> WStr& WStr::operator=(const char16_t&) { return *this; }
template<> WStr& WStr::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> WStr& WStr::operator+=(const char16_t&) { return *this; }
template<> WStr& WStr::operator+=(const char16_t*) { return *this; }

// ---- globals of other files, defined here
CThingArrayEntry gThingArray[kThingArraySize];
PendingList gPendingDeleteList;
TbDynamicStringTemplate<char16_t> gWorldGatesName;
TbDynamicStringTemplate<char16_t> gWorldLightsName;
TbDynamicStringTemplate<char16_t> gArrivalBusName;
TbDynamicStringTemplate<char16_t> gArrivalSeaplaneName;
TbDynamicStringTemplate<char16_t> gArrivalFerryName;
TbDynamicStringTemplate<char16_t> gWorldEndName;
uint16_t gWinningAnimThing;

void UIMain_DestroyWString(void*) {}
void __sinit_World_cpp();

extern uint32_t gWorldWord1037ec00, gWorldWord1037ec04;
extern char gSinitCtorVecThingEntry[8], gSinitDtorVecPendingList[8], gSinitNodeList[16], gSinitNodeBus[16], gSinitNodeSeaplane[16],
    gSinitNodeFerry[16], gSinitNodeGates[16], gSinitNodeLights[16], gSinitNodeEnd[16];


extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count) {
    ev("construct_array", {lab(array), lab(ctor), lab(dtor), size, count});
}
extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ec00", &gWorldWord1037ec00},
        {"ec04", &gWorldWord1037ec04},
        {"win", &gWinningAnimThing},
        {"array", gThingArray},
        {"ctorvec", gSinitCtorVecThingEntry},
        {"list", &gPendingDeleteList},
        {"listdtor", gSinitDtorVecPendingList},
        {"wdtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"nlist", gSinitNodeList},
        {"nbus", gSinitNodeBus},
        {"nsea", gSinitNodeSeaplane},
        {"nferry", gSinitNodeFerry},
        {"ngates", gSinitNodeGates},
        {"nlights", gSinitNodeLights},
        {"nend", gSinitNodeEnd},
        {"bus", &gArrivalBusName},
        {"sea", &gArrivalSeaplaneName},
        {"ferry", &gArrivalFerryName},
        {"gates", &gWorldGatesName},
        {"lights", &gWorldLightsName},
        {"end", &gWorldEndName},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sinit_layout(int64_t*) { return 0; }

void tpw_sinit_run() { __sinit_World_cpp(); }

}  // extern "C"
