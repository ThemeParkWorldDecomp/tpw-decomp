// Test library for the litter cheats (unit v5299; libtpw_unit_place_litter.dylib, tests/test_place_litter.py), main app:
//   src/ui_Postcard_panel_spawn_handyman_place_litter.cpp   place_litter, place_bomb
//
// CWorld::GetRand and CMapCell::PlaceLitter are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "World.h"
#include "intrface.h"
#include "keytable_actions.h"

using namespace calllog;

uint32_t gLitterKindCount;
CIntrface gIntrface;
CWorld* gGameWorld;

uint32_t CWorld::GetRand() {
    ev("GetRand", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this))});
    return static_cast<uint32_t>(ret("GetRand"));
}

void CMapCell::PlaceLitter(LitterType type) {
    int32_t diff = static_cast<int32_t>(reinterpret_cast<char*>(this) - reinterpret_cast<char*>(gGameWorld));
    ev("PlaceLitter", {diff, static_cast<int32_t>(type)});
}

extern "C" {

uint64_t tpw_pl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"count", &gLitterKindCount},
        {"intrface", &gIntrface},
        {"world", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CIntrface, mF24));
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    return n;
}

uint32_t tpw_pl_litter() { return static_cast<uint32_t>(place_litter()); }
uint32_t tpw_pl_bomb() { return static_cast<uint32_t>(place_bomb()); }

}  // extern "C"
