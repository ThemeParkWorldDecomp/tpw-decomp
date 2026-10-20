#include <cstdint>
#include "BMP_InsideArea.h"
#include "TPAdvisor.h"
#include "Debug.h"
#include "TPSound.h"
#include "World.h"

namespace {

uint8_t* WorldBytes() { return reinterpret_cast<uint8_t*>(&gColList); }
uint32_t& Wd(uint8_t* base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }

// The id of a ride record (kind << 8 | index), index = (record - records) / 0xd0 by the compiler's multiply.
uint32_t IdOfRide(uint8_t* world, uint32_t rideOffset) {
    Ptr32<uint8_t> recs{Wd(world, 8)};
    int32_t diff = static_cast<int32_t>(rideOffset - recs.mOffset);
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x4ec4ec4f) * diff) >> 32) >> 6;
    q += static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    Ptr32<uint8_t> ride{rideOffset};
    return (Wd(ride.get(), 0) << 8) | static_cast<uint32_t>(q);
}

// The record slot of an id (null when the world is dead) and its recomputed id (0 when dead).
uint8_t* RideOf(int32_t id, uint32_t* outId) {
    uint8_t* world = WorldBytes();
    if (Wd(world, 0) == 0x4a454647u) {
        Ptr32<uint8_t> recs{Wd(world, 8)};
        uint8_t* ride = recs.get() + (static_cast<uint32_t>(id) & 0xff) * 0xd0;
        *outId = IdOfRide(world, recs.mOffset + (static_cast<uint32_t>(id) & 0xff) * 0xd0);
        return ride;
    }
    *outId = 0;
    return nullptr;
}

int32_t RideEventHandle(uint32_t rideId) {
    uint16_t thing;
    TrackRideIdToThingId(&thing, static_cast<int32_t>(rideId));
    if (thing == gAdvisorThingId) return 0;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    return static_cast<int32_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gThingArray[thing].mThing) + 0x20));
}

uint32_t RideIdOfBump(uint8_t* bump) {
    uint8_t* world = WorldBytes();
    if (Wd(world, 0) == 0x4a454647u) return IdOfRide(world, Wd(bump, 0x9c));
    return 0;
}

}  // namespace

void BMP_ControlSound(tagBUMP* b) {
    uint8_t* bump = reinterpret_cast<uint8_t*>(b);
    uint8_t* world = WorldBytes();
    uint32_t pending = Wd(bump, 0x14);
    if (pending != 0) {
        if (Wd(bump, 0x18) < pending) Wd(bump, 0x18) = pending;
        Wd(bump, 0x14) = 0;
    }
    int32_t state = static_cast<int32_t>(Wd(bump, 0x18));
    int32_t pt[2];
    switch (state) {
        case 3:
            SingleSoundAtBump(b, 0xc);
            TRACE("CRUNCH\n");
            Wd(bump, 0x18) = 0;
            break;
        case 1:
            SingleSoundAtBump(b, 0x18);
            Wd(bump, 0x18) = 0;
            break;
        case 2: {
            int32_t seed = static_cast<int32_t>(Wd(world, 0x38)) * 0x343fd + 0x269ec3;
            Wd(world, 0x38) = static_cast<uint32_t>(seed);
            if (((seed >> 16) & 7) == 0) {
                MineToSystemCords(static_cast<int32_t>(Wd(bump, 0x34)), static_cast<int32_t>(Wd(bump, 0x38)),
                                  reinterpret_cast<NS_MacDoze::tagPOINT*>(pt));
                int32_t handle = RideEventHandle(RideIdOfBump(bump));
                SND_SendIndirectedEvent(handle, 0, gSoundBanks[7], 3, pt[0], 0, pt[1]);
                TRACE("TOOT!\n");
            }
            Wd(bump, 0x18) = 0;
            break;
        }
        case 4: {
            MineToSystemCords(static_cast<int32_t>(Wd(bump, 0x34)), static_cast<int32_t>(Wd(bump, 0x38)),
                              reinterpret_cast<NS_MacDoze::tagPOINT*>(pt));
            int32_t handle = RideEventHandle(RideIdOfBump(bump));
            SND_SendIndirectedEvent(handle, 0, gSoundBanks[7], 4, pt[0], 0, pt[1]);
            Wd(bump, 0x18) = 0;
            break;
        }
        case 5:
            SingleSoundAtBump(b, 0xc);
            Wd(bump, 0x18) = 0;
            break;
        default:
            break;
    }
}

void FreeBuoysForRide(tagBUMPRIDE* r, int32_t n) {
    uint8_t* ride = reinterpret_cast<uint8_t*>(r);
    uint8_t* world = WorldBytes();
    Ptr32<uint8_t> headp{Wd(ride, 0xa8)};
    uint8_t* cur = headp.get();
    if (cur == nullptr) return;
    uint8_t* keep = nullptr;
    while (cur != nullptr) {
        if (n == 0 || (Wd(cur, 0) & static_cast<uint32_t>(n)) == 0) {
            Wd(ride, 0xac) -= 1;
            Wd(world, 0x24) -= 1;
            Wd(cur, 0) &= 0xfffffffeu;
        } else if (keep == nullptr) {
            keep = cur;
        } else {
            uint8_t* p = keep;
            for (;;) {
                Ptr32<uint8_t> nx{Wd(p, 0x14)};
                if (nx.get() == nullptr) break;
                p = nx.get();
            }
            Ptr32<uint8_t> link;
            link.set(cur);
            Wd(p, 0x14) = link.mOffset;
        }
        Ptr32<uint8_t> next{Wd(cur, 0x14)};
        Wd(cur, 0x14) = 0;
        cur = next.get();
    }
    Ptr32<uint8_t> nh;
    nh.set(keep);
    Wd(ride, 0xa8) = nh.mOffset;
}

int32_t BMP_GetCarsOnRide(int32_t ride) {
    uint32_t id;
    uint8_t* rec = RideOf(ride, &id);
    if (static_cast<uint32_t>(ride) != id) return 0;
    return static_cast<int32_t>(Wd(rec, 0x5c));
}

void BMP_SetRideModelSelection(int32_t ride, int32_t a, int32_t b) {
    uint32_t id;
    uint8_t* rec = RideOf(ride, &id);
    if (static_cast<uint32_t>(ride) != id) return;
    Wd(rec, 0x14) = static_cast<uint32_t>(a);
    Wd(rec, 0x18) = static_cast<uint32_t>(b);
}
