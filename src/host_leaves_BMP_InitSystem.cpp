// Unit v5659: the advisor's bump world (main app, TPAdvisor.cpp): BMP_InitSystem 0x10025a94, BMP_CloseSystem 0x10025cb0, BMP_FreeAll 0x10025db8,
// BMP_OpenRide 0x10025ee4, BMP_CloseRide 0x10026118. The world is the global at 0x102b56c0 (gColList in BMP_InsideArea.h); its 0x3c bytes are
// viewed here by Mac offset (pointer fields are Ptr32).
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Advisor.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "Debug.h"
#include "Ptr32.h"
#include "World.h"
#include "effect_manager.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"

int32_t Rand();   // 0x10239148 rand (ui_menu.h)

namespace {

constexpr uint32_t kMagic = 0x4a454647;

struct Link {                // 0x14 bytes
    uint32_t pad00[4];       // 0x00
    Ptr32<Link> next;        // 0x10
};

struct BumpWorld {
    uint32_t magic;          // 0x00
    uint32_t tick;           // 0x04
    Ptr32<uint8_t> rides;    // 0x08 0xd0 bytes each
    int32_t rideCount;       // 0x0c
    Ptr32<uint8_t> bumps;    // 0x10 0xac bytes each
    int32_t bumpCount;       // 0x14
    Ptr32<uint8_t> block18;  // 0x18
    uint32_t pad1c;          // 0x1c
    Ptr32<uint8_t> block20;  // 0x20
    uint32_t pad24;          // 0x24
    Ptr32<uint8_t> block28;  // 0x28
    uint32_t pad2c;          // 0x2c
    Ptr32<Link> links;       // 0x30 1024 records of 0x14 bytes
    Ptr32<Link> freeHead;    // 0x34
    int32_t seed;            // 0x38
};
static_assert(sizeof(BumpWorld) == 0x3c, "BumpWorld");

BumpWorld& World() { return *reinterpret_cast<BumpWorld*>(&gColList); }

template <class T>
T& At(void* base, uint32_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

// The ride record at the id's low byte, NULL when the world is not live.
uint8_t* RideOf(int32_t ride) {
    BumpWorld& world = World();
    if (world.magic != kMagic) return nullptr;
    return world.rides.get() + static_cast<uint32_t>(ride & 0xff) * 0xd0;
}

// The id of a ride record: kind << 8 | index, 0 when the world is not live.
int32_t IdOf(uint8_t* record) {
    BumpWorld& world = World();
    if (world.magic != kMagic) return 0;
    int32_t index = static_cast<int32_t>(record - world.rides.get()) / 0xd0;
    return (At<int32_t>(record, 0) << 8) | index;
}

// Links the 1024 records into a chain, each pointing at the next, the last at NULL.
void RebuildLinks() {
    BumpWorld& world = World();
    Link* previous = nullptr;
    for (int32_t i = 0x3ff; i >= 0; --i) {
        Link* link = reinterpret_cast<Link*>(reinterpret_cast<uint8_t*>(world.links.get()) + i * 0x14);
        link->next = previous;
        previous = link;
    }
}

Ptr32<uint8_t> Allocate(uint32_t size) {
    Ptr32<uint8_t> result;
    result.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalAlloc(0x40, size)));
    return result;
}

// The jump tables (0x1029c98c, 0x1029c9b4): what each kind does.
// Open, by kind + 13 (kinds -13 to -4): 0x10026014 sets state 2, 0x10025fa4 state 1 (both only from state 0, after SetBuoySequence), 0x10026084 state 1.
enum OpenAction : uint8_t { kOpenStart1, kOpenStart2, kOpenDefault };
const OpenAction kOpenTable[10] = {
    kOpenStart2, kOpenStart2, kOpenDefault, kOpenStart1, kOpenStart1,
    kOpenStart2, kOpenStart1, kOpenDefault, kOpenStart2, kOpenStart1,
};
// Close, by kind + 14 (kinds -14 to -1): true for the kinds -14, -11, -6, -3 and -1, which release the ride's bumps.
const bool kCloseTable[14] = {true, false, false, true, false, false, false, false, true, false, false, true, false, true};

}  // namespace

void BMP_InitSystem() {
    BumpWorld& world = World();
    for (int32_t i = 0; i < 0x200; ++i) {
        const double angle = 3.14159 * static_cast<double>(i << 1) * 0.001953125;
        gHostSineTable[i] = ppc_fctiwz(256.0 * std::sin(angle));
    }
    std::memset(&world, 0, 0x3c);
    world.magic = kMagic;
    world.tick = 0;
    world.rides = Allocate(0x3400);
    world.bumps = Allocate(0xac00);
    world.block18 = Allocate(0x10000);
    world.block20 = Allocate(0x7000);
    world.block28 = Allocate(0xa000);
    world.links.set(reinterpret_cast<Link*>(Allocate(0x5000).get()));
    RebuildLinks();
    world.freeHead = world.links;
    world.seed = Rand();
}

void BMP_FreeAll() {
    BumpWorld& world = World();
    if (world.magic != kMagic) return;
    int32_t remaining = world.rideCount;
    uint8_t* record = world.rides.get();
    while (remaining != 0) {
        if (At<int32_t>(record, 0) != 0) {
            --remaining;
            At<int32_t>(record, 0x50) = 4;
            BMP_DestroyRide(IdOf(record));
        }
        record += 0xd0;
    }
    RebuildLinks();
    world.freeHead = world.links;
}

void BMP_CloseSystem() {
    BumpWorld& world = World();
    if (world.magic != kMagic) return;
    BMP_FreeAll();
    world.block28.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.block28.get()))));
    world.block20.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.block20.get()))));
    world.block18.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.block18.get()))));
    world.bumps.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.bumps.get()))));
    world.rides.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.rides.get()))));
    world.links.set(reinterpret_cast<Link*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.links.get()))));
    std::memset(&world, 0, 0x3c);
}

int32_t BMP_OpenRide(int32_t ride) {
    BumpWorld& world = World();
    uint8_t* record = RideOf(ride);
    if (ride != IdOf(record)) return 0;
    TRACE("Open ride %d\n", ride);
    uint32_t index = static_cast<uint32_t>(At<int32_t>(record, 0) + 13);
    OpenAction action = index <= 9 ? kOpenTable[index] : kOpenDefault;
    if (action == kOpenDefault) {
        At<int32_t>(record, 0x50) = 1;
    } else {
        if (At<int32_t>(record, 0x50) != 0) return 0;
        SetBuoySequence(reinterpret_cast<tagBUMPRIDE*>(record));
        At<int32_t>(record, 0x50) = action == kOpenStart1 ? 1 : 2;
    }
    BMP_SetRidePerformance(IdOf(record), At<int32_t>(record, 0x1c));
    (void)world;
    return 0;
}

int32_t BMP_CloseRide(int32_t ride) {
    BumpWorld& world = World();
    uint8_t* record = RideOf(ride);
    if (ride != IdOf(record)) return 0;
    if (At<int32_t>(record, 0x50) == 0) return 0;
    At<int32_t>(record, 0x50) = 0;
    TRACE("Close ride %d\n", ride);
    Link* tail = reinterpret_cast<Ptr32<Link>*>(record + 0xc8)->get();
    Ptr32<Link>& source = At<Ptr32<Link>>(record, 0xc4);
    if (tail == nullptr) {
        At<Ptr32<Link>>(record, 0xc8) = source;
    } else {
        while (tail->next.get() != nullptr) tail = tail->next.get();
        tail->next = source;
    }
    source.mOffset = 0;
    uint32_t index = static_cast<uint32_t>(At<int32_t>(record, 0) + 14);
    if (index <= 13 && kCloseTable[index]) {
        uint8_t* bump = world.bumps.get();
        if (bump != nullptr) {
            int32_t remaining = world.bumpCount;
            while (remaining != 0) {
                uint32_t& flags = At<uint32_t>(bump, 0);
                if ((flags & 1) != 0) {
                    --remaining;
                    if (At<Ptr32<uint8_t>>(bump, 0x9c).get() == record) {
                        flags &= ~0x4000u;
                        flags &= 0xffffff81u;
                        flags |= 0x10000u;
                        flags |= 0x20u;
                        At<int32_t>(bump, 0x88) = 0;
                    }
                }
                bump += 0xac;
            }
        }
    }
    return 0;
}
