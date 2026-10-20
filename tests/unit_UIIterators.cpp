// Differential library for unit w227. The world walks, object queries and callback record every receiver and argument.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_iterators.h"
#include "calllog.h"

using namespace calllog;

CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gGameWorld;

namespace {
constexpr int kSlots = 8;
constexpr int kGuard = 16;
constexpr int kThingSize = 0x210;
constexpr int kThingBlock = kThingSize + 2 * kGuard;
alignas(16) unsigned char gThings[kSlots][kThingBlock];
alignas(16) unsigned char gRides[kSlots][sizeof(RideInfo) + 2 * kGuard];
alignas(16) unsigned char gWorlds[2][sizeof(CWorld) + 2 * kGuard];
uint16_t gUpgrades[kSlots];
uint16_t gNext[kSlots];
uint16_t gPrev[kSlots];
uint16_t gHead;
int32_t gCallbackAnswer;
int32_t gCallbackMode;
bool gSwap;

CThing* Thing(int i) { return reinterpret_cast<CThing*>(gThings[i] + kGuard); }
RideInfo* Ride(int i) { return reinterpret_cast<RideInfo*>(gRides[i] + kGuard); }
CWorld* World(int i) { return reinterpret_cast<CWorld*>(gWorlds[i] + kGuard); }
int Slot(const void* p) {
    for (int i = 0; i < kSlots; ++i) {
        if (p == Thing(i)) {
            return i;
        }
    }
    return -1;
}
void SwapWorld() {
    if (gSwap) {
        gGameWorld = gGameWorld == World(0) ? World(1) : World(0);
    }
}
int Callback(CThing* thing) {
    ev("Callback", {lab(thing)});
    if (gCallbackMode & 1) {
        *reinterpret_cast<uint16_t*>(reinterpret_cast<unsigned char*>(thing) + 0x20c) = 0;
    }
    if (gCallbackMode & 2) {
        thing->mKind = 1;
    }
    if (gCallbackMode & 4) {
        reinterpret_cast<CObject*>(thing)->mFlags ^= 3;
    }
    SwapWorld();
    return gCallbackAnswer;
}
} // namespace

// All fakes also optionally replace gGameWorld to expose cached-world errors in the translated walks.
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
    SwapWorld();
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {lab(this)});
    SwapWorld();
    return {gHead};
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {lab(this), lab(thing)});
    SwapWorld();
    return {gNext[Slot(thing)]};
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetPrevUsedThing(const CThing* thing) const {
    ev("GetPrevUsedThing", {lab(this), lab(thing)});
    SwapWorld();
    return {gPrev[Slot(thing)]};
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    SwapWorld();
    const int slot = Slot(this);
    return {slot < 0 ? uint16_t(0) : Thing(slot)->mThingId};
}
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    ev("UpgradesWhat", {lab(this)});
    SwapWorld();
    return {gUpgrades[Slot(this)]};
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    SwapWorld();
    return Ride(Slot(this));
}

extern "C" {
// Export native offsets for the two layouts whose pointers can change the host offsets.
uint32_t tpw_uii_layout(int32_t n) {
    const uint32_t values[] = {sizeof(CWorld), offsetof(CWorld, mF1DA73C), offsetof(CWorld, mF1DA73E),
        offsetof(CWorld, mF1DA744), offsetof(CWorld, mF1DA740), offsetof(CWorld, mF1DA742), sizeof(RideInfo),
        offsetof(RideInfo, mUIType), offsetof(CObject, mFlags),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&Ride(0)->Value(kRideSlot_UpgradeType).mInteger) - reinterpret_cast<uintptr_t>(Ride(0)))};
    return values[n];
}
void* tpw_uii_addr(int32_t n) {
    if (n < 2) {
        return World(n);
    }
    if (n < 18) {
        return n < 10 ? static_cast<void*>(Thing(n - 2)) : static_cast<void*>(Ride(n - 10));
    }
    return gGameWorld;
}
void tpw_uii_reset(int32_t head, int32_t answer, int32_t mode, int32_t swap) {
    std::memset(gThingArray, 0, sizeof gThingArray);
    std::memset(gThings, 0xa5, sizeof gThings);
    std::memset(gRides, 0xa5, sizeof gRides);
    std::memset(gWorlds, 0xa5, sizeof gWorlds);
    gGameWorld = World(0);
    gHead = static_cast<uint16_t>(head);
    gCallbackAnswer = answer;
    gCallbackMode = mode;
    gSwap = swap != 0;
}
void tpw_uii_thing(int32_t i, int32_t id, int32_t kind, int32_t flags, int32_t link, int32_t up, int32_t ui, int32_t upgradeType) {
    Thing(i)->mThingId = static_cast<uint16_t>(id);
    Thing(i)->mKind = static_cast<uint8_t>(kind);
    reinterpret_cast<CObject*>(Thing(i))->mFlags = static_cast<uint16_t>(flags);
    *reinterpret_cast<uint16_t*>(reinterpret_cast<unsigned char*>(Thing(i)) + 0x20c) = static_cast<uint16_t>(link);
    gUpgrades[i] = static_cast<uint16_t>(up);
    Ride(i)->mUIType = ui;
    Ride(i)->Value(kRideSlot_UpgradeType).mInteger = upgradeType;
}
void tpw_uii_links(int32_t i, int32_t next, int32_t prev) {
    gNext[i] = static_cast<uint16_t>(next);
    gPrev[i] = static_cast<uint16_t>(prev);
}
void tpw_uii_row(int32_t id, int32_t slot) { gThingArray[id].mThing = slot < 0 ? nullptr : Thing(slot); }
void tpw_uii_head(int32_t world, int32_t type, int32_t id) {
    const size_t offsets[] = {offsetof(CWorld, mF1DA73C), offsetof(CWorld, mF1DA73E), offsetof(CWorld, mF1DA744),
        offsetof(CWorld, mF1DA740), offsetof(CWorld, mF1DA742)};
    *reinterpret_cast<uint16_t*>(reinterpret_cast<unsigned char*>(World(world)) + offsets[type]) = static_cast<uint16_t>(id);
}
// All searches return a label; the counting walk returns its signed count.
int32_t tpw_uii_call(int32_t which, int32_t type, int32_t start) {
    CThing* const thing = start < 0 ? nullptr : Thing(start);
    const UIIteratorTypes iter = static_cast<UIIteratorTypes>(type);
    const UIObjType object = static_cast<UIObjType>(type);
    CThing* result;
    switch (which) {
        case 0: return UI_IterateThingList(iter, Callback);
        case 1: result = NextObjectOfType(object, thing); break;
        case 2: result = NextUpgrade(thing); break;
        case 3: result = NextNonTrackUpgrade(thing); break;
        case 4: result = NextBog(thing); break;
        case 5: result = NextFeatureNotBogOrStaffRoom(thing); break;
        case 6: result = NextStaffRoom(thing); break;
        case 7: result = UI_IterateThingNext(iter, thing); break;
        case 8: result = PreviousObjectOfType(object, thing); break;
        case 9: result = PreviousUpgrade(thing); break;
        case 10: result = PreviousNonTrackUpgrade(thing); break;
        case 11: result = PreviousBog(thing); break;
        case 12: result = PreviousFeatureNotBogOrStaffRoom(thing); break;
        case 13: result = PreviousStaffRoom(thing); break;
        case 14: result = UI_IterateThingPrevious(iter, thing); break;
        case 15: result = UI_WrapIterateThingNext(iter, thing); break;
        default: result = UI_WrapIterateThingPrevious(iter, thing); break;
    }
    return lab(result);
}
void tpw_uii_sinit(uint32_t* out) {
    out[0] = gUIIterFileWord10396A4C;
    out[1] = gUIIterFileWord10396A50;
}
}
