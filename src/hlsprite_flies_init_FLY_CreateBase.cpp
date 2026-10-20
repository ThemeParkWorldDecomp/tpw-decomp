// FLY_CreateBase 0x100753e8 (352 bytes) and EmployEric 0x10074d9c (1208 bytes): a new fly base and the starting of one car of it.
// A fly base is the 0x122c-byte block of gFlyBases (layout: HLSprite.h, sFlyBase); the code reads it by Mac offset in 32-bit words.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Debug.h"
#include "analyser.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "rsse.h"

namespace {

// The texts at 0x1023dc85 (the first, then +0x2f and +0x72 of it).
const char kFlyMessageAnimFailed[] = "FLY: Failed to trigger a flying car anim (%ld)";
const char kFlyMessageThingZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kFlyMessageNoHeadNodes[] = "FLY: No headnodes on car mesh";

// A 32-bit word of a block at a Mac byte offset.
inline int32_t& Wd(uint8_t* block, uint32_t offset) { return *reinterpret_cast<int32_t*>(block + offset); }

// The Mac's labs on a 32-bit long (wraps for the most negative number).
inline int32_t Labs32(uint32_t value) {
    int32_t v = static_cast<int32_t>(value);
    return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
}

inline int32_t Rand(CWorld* world) { return Labs32(world->GetRand()); }

}  // namespace

// Starts car `slotNo` of base `index`: the first free slot (word 0x80 is 0) of the 20 gets its first position (outside the fly zone, at a
// random edge), a sound, a ride instance of the thing's ride type and its animation count. Answers 1, or 0 when all 20 slots are in use.
int32_t EmployEric(int32_t index) {
    void** const entry = &gFlyBases[index];
    uint8_t* const base = static_cast<uint8_t*>(*entry);
    int32_t k = 0;
    uint8_t* p = nullptr;
    for (;; ++k) {
        if (k >= 0x14) return 0;
        p = base + k * 0xe4 + 0x5c;
        if (Wd(p, 0x80) == 0) break;
    }
    Wd(p, 0xe0) = 0;
    Wd(p, 0) = index;
    Wd(p, 0x18) = 0;
    Wd(p, 0x14) = 0;
    Wd(p, 0x74) = 0;
    Wd(p, 0x70) = 0;
    Wd(p, 0x78) = 0;
    int32_t r = Rand(gGameWorld);
    Wd(p, 0x7c) = Wd(base, 0x28) + (r - (r / 100) * 100);
    Wd(p, 0x64) = 0;
    Wd(p, 0x68) = 0;
    Wd(p, 0x6c) = 0;
    Wd(p, 0x80) = 4;
    Wd(p, 0xb8) = 0;
    Wd(p, 0xb4) = 0;
    Wd(p, 0x8c) = 0;
    Wd(p, 0xc4) = 0;
    Wd(p, 0xc8) = 1;
    Wd(p, 0xc0) = 0;
    Wd(p, 0x50) = 0x36b0;
    if ((Rand(gGameWorld) & 1) != 0) {
        const int32_t m = Rand(gGameWorld) * 100;
        const int32_t span = gFlyZoneD - gFlyZoneB;
        Wd(p, 0x4c) = gFlyZoneB + (m - (m / span) * span);
        if ((Rand(gGameWorld) & 1) != 0)
            Wd(p, 0x54) = gFlyZoneA + 0x10000 - 0x5038;
        else
            Wd(p, 0x54) = gFlyZoneC - 0x10000 + 0x5038;
    } else {
        const int32_t m = Rand(gGameWorld) * 100;
        const int32_t span = gFlyZoneA - gFlyZoneC;
        Wd(p, 0x54) = gFlyZoneC + (m - (m / span) * span);
        if ((Rand(gGameWorld) & 1) != 0)
            Wd(p, 0x4c) = gFlyZoneD + 0x10000 - 0x5038;
        else
            Wd(p, 0x4c) = gFlyZoneB - 0x10000 + 0x5038;
    }
    Wd(p, 4) = Wd(p, 0x4c);
    Wd(p, 8) = Wd(p, 0x50);
    Wd(p, 0xc) = Wd(p, 0x54);
    Wd(p, 0x84) = 0;
    Wd(p, 0x58) = Wd(p, 0x4c) - 0x4e20;
    Wd(p, 0x5c) = Wd(p, 0x50);
    Wd(p, 0x60) = Wd(p, 0x54);
    Wd(base, 0xc) = Wd(base, 0xc) + 1;
    Wd(p, 0xcc) = -1;
    Wd(p, 0xd0) = -1;
    Wd(p, 0xd4) = -1;
    Wd(p, 0xe0) = SND_SendIndirectedEvent(Wd(base, 0x54), 0, gSoundBanks[7], 0, Wd(p, 4) / 300, Wd(p, 8) / 300, Wd(p, 0xc) / 300);
    Wd(p, 0xd8) = Rand(gGameWorld);
    if (Wd(base, 0xc) == 1) {
        Wd(p, 0x80) = 1;
        Wd(p, 4) = Wd(base, 0x14);
        Wd(p, 8) = Wd(base, 0x18);
        Wd(p, 0xc) = Wd(base, 0x1c);
        Wd(p, 0x7c) = 0;
        Wd(p, 0x10) = Wd(base, 0x40);
        Wd(base, 0x3c) = 1;
    }
    uint16_t thingNo;
    std::memcpy(&thingNo, base + 0x58, 2);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kFlyMessageThingZero);
    uint16_t rideType;
    std::memcpy(&rideType, static_cast<uint8_t*>(gThingArray[thingNo].mThing) + 0xe, 2);
    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{rideType});
    uint32_t rideNo;
    std::memcpy(&rideNo, reinterpret_cast<uint8_t*>(info) + 0x7b8, 4);
    Wd(p, 0xc0) = static_cast<int32_t>(InstanceRide(rideNo, 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0));
    uint8_t* q = static_cast<uint8_t*>(*entry) + k * 0xe4 + 0x5c;
    if (Wd(q, 0xc0) != 0 && (Wd(q, 0xcc) != 0 || Wd(q, 0xd0) != 0 || Wd(q, 0xd4) != 0)) {
        int32_t result;
        if (Wd(q, 0xc0) == 0)
            result = 0;
        else
            result = static_cast<int32_t>(SetCurRideAnimDuration(gRideAnimSets[static_cast<uint32_t>(Wd(q, 0xc0))], 0, 0, 0, 1.0f, 0));
        Wd(q, 0xcc) = 0;
        Wd(q, 0xd0) = 0;
        Wd(q, 0xd4) = 0;
        GAME_ASSERTMSG(result, kFlyMessageAnimFailed, 0);
    }
    int32_t count = 0;
    for (;;) {
        const uint32_t found =
            FindDataNodeIndexByID(gRideAnimSets[static_cast<uint32_t>(Wd(p, 0xc0))], 0x80, static_cast<uint32_t>(count + 1));
        if (found == 0xffffffffu) break;
        ++count;
    }
    Wd(p, 0xdc) = count;
    if (count == 0) GAME_ASSERTMSG(0, kFlyMessageNoHeadNodes);
    return 1;
}

// Makes a fly base in the first free slot of gFlyBases (1 to 99): answers its index, or -1 when all are in use. `thingWord` carries the thing
// id in its high half. Like the original, a failed allocation is not checked.
int32_t FLY_CreateBase(uint32_t thingWord, int32_t handle, int32_t x, int32_t y, int32_t z, int32_t angle, int32_t a) {
    int32_t i = 1;
    while (i < 100 && gFlyBases[i] != nullptr) ++i;
    if (i >= 100) return -1;
    uint8_t* base = static_cast<uint8_t*>(LbMem_Alloc(0x122c));
    gFlyBases[i] = base;
    if (base != nullptr) {
        Wd(base, 0x2c) = 0;
        Wd(base, 0x14) = x;
        Wd(base, 0x18) = y;
        Wd(base, 0x1c) = z;
        Wd(base, 0) = 1;
        Wd(base, 4) = 1;
        Wd(base, 0xc) = 0;
        Wd(base, 0x10) = 300;
        Wd(base, 0x3c) = 0;
        Wd(base, 0x40) = 0x400 - angle;
        Wd(base, 0x44) = 0;
        Wd(base, 0x2c) = 1;
        Wd(base, 0x38) = 1;
        Wd(base, 0x48) = 0;
        Wd(base, 0x20) = 0x400;
        Wd(base, 0x24) = 0x400;
        const uint16_t thingNo = static_cast<uint16_t>(thingWord >> 16);
        std::memcpy(base + 0x58, &thingNo, 2);
        Wd(base, 0x4c) = a;
        Wd(base, 0x54) = handle;
        for (int32_t s = 0; s < 0x14; ++s) Wd(base, s * 0xe4 + 0xdc) = 0;
        Wd(base, 0x28) = 300;
        Wd(base, 0x50) = 5;
        EmployEric(i);
    }
    Wd(base, 0x30) = Wd(base, 0x4c) != 0 ? 0x9c4 : 0x1388;
    Wd(base, 0x34) = 1;
    return i;
}
