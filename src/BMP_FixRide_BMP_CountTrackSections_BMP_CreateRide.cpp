// BMP_CreateRide 0x1002756c: takes a ride record (a free one, or the one an id names), copies the kind's template into it, sets
// it up by the kind (collision objects, buoys, performance) and answers its id (kind << 8 | index), 0 on a dead world.
// The ride record is read by Mac offset (words). The listing's jump tables (0x1029ca64 on the kind, 0x1029ca2c on the kind again)
// are kTop and kSub below.
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPAdvisor.h"
#include "rsse.h"

extern int32_t gHostSineTable[512];   // 0x102b4ec0

namespace {

enum Target { kEnd = 0, kShip, kSub, kTrack, kSingle, kS1, kS2, kS3 };

// The jump tables of the listing, indexed by kind + 14.
const uint8_t kTop[14] = { kSub, kSingle, kSingle, kSub, kTrack, kTrack, kSingle, kTrack, kSub, kSingle, kTrack, kSub, kShip, kSub };
const uint8_t kSubTable[14] = { kS3, kS3, kS3, kS2, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS1 };

int32_t MulHw(int32_t a, int32_t b) { return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 32); }

// srawi n, 8 then addze: a division by 256 cut toward zero.
int32_t Div256(int32_t n) {
    int32_t q = n >> 8;
    if (n < 0 && (n & 0xff) != 0) q++;
    return q;
}

// The quarter turn step of the buoy rings: (a, b) rotated by the sine table's c and s.
void Rotate(int32_t& a, int32_t& b, int32_t c, int32_t s) {
    uint32_t ua = static_cast<uint32_t>(a), ub = static_cast<uint32_t>(b);
    uint32_t uc = static_cast<uint32_t>(c), us = static_cast<uint32_t>(s);
    int32_t na = Div256(static_cast<int32_t>(ua * uc + ub * us));
    int32_t nb = Div256(static_cast<int32_t>(ub * uc - ua * us));
    a = na;
    b = nb;
}

uint32_t* Words(void* p) { return reinterpret_cast<uint32_t*>(p); }
uint32_t* World() { return Words(&gColList); }
uint32_t* Rides() { return reinterpret_cast<uint32_t*>(reinterpret_cast<tagBUMPWORLD*>(&gColList)->rides.get()); }

// kind << 8 | index of the ride in the list, 0 when the world is dead (the listing leaves a stale value there).
int32_t IdOf(uint32_t* ride) {
    if (World()[0] != 0x4a454647u) return 0;
    int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ride)) - static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Rides())));
    int32_t q = MulHw(0x4ec4ec4f, diff) >> 6;
    q += static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    return static_cast<int32_t>((ride[0] << 8) | static_cast<uint32_t>(q));
}

uint32_t* RideOfId(int32_t id) {
    if (World()[0] != 0x4a454647u) return nullptr;
    return reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(Rides()) + (static_cast<uint32_t>(id) & 0xff) * 0xd0);
}

// The performance step at the end of every kind: the ride's word 0x1c clamped, traced, and the four blends 0x20.. = lo + v * (hi - lo) / 100.
void SetPerformance(uint32_t* ride) {
    int32_t v = static_cast<int32_t>(ride[0x1c / 4]);
    int32_t id = IdOf(ride);
    uint32_t* r = RideOfId(id);
    if (id != IdOf(r)) return;
    const int32_t* pick = &gRidePerfMax;
    if (!(gRidePerfMax < v)) pick = &v;
    if (!(gRidePerfMin < *pick)) pick = &gRidePerfMin;
    v = *pick;
    r[0x1c / 4] = static_cast<uint32_t>(v);
    TRACE("Set ride performance to %d\n", static_cast<int32_t>(r[0x1c / 4]));
    for (int k = 0; k < 4; k++) {
        int32_t lo = static_cast<int32_t>(r[0x30 / 4 + k]);
        int32_t hi = static_cast<int32_t>(r[0x40 / 4 + k]);
        int32_t prod = static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(hi - lo));
        int32_t h = MulHw(0x51eb851f, prod) >> 5;
        h += static_cast<int32_t>(static_cast<uint32_t>(h) >> 31);
        r[0x20 / 4 + k] = static_cast<uint32_t>(lo + h);
    }
}

// The ring of eight buoys around (x, y) shared by the three sub kinds.
void RingOfBuoys(uint32_t* ride, int32_t x, int32_t y, int32_t dx, int32_t dy) {
    ride[0x90 / 4] = static_cast<uint32_t>(x);
    ride[0x74 / 4] = static_cast<uint32_t>(x);
    ride[0x94 / 4] = static_cast<uint32_t>(y);
    ride[0x78 / 4] = static_cast<uint32_t>(y);
    for (int i = 8; i != 0; i--) {
        BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + dx, y + dy);
        Rotate(dx, dy, gHostSineTable[0x300 / 4], gHostSineTable[0x100 / 4]);
    }
    ride[0x50 / 4] = 0;
    SetPerformance(ride);
}

// The corner offsets the track kinds use from the ride's rotation word (bit 0 swaps, bit 1 negates the first, bit 2 the second).
void Orient(uint32_t rot, int32_t& a, int32_t& b) {
    if (rot & 1) { a = 0; b = -0xc00; } else { a = -0xc00; b = 0; }
    if (rot & 2) a = -a;
    if (rot & 4) b = -b;
}

void Dispatch(int target, uint32_t* ride, int32_t x, int32_t y);

void Ship(uint32_t* ride) {
    const int32_t base = 0x10000;
    BMP_StartColObject(base - 0x4c00, base - 0x6a00, 0x1e00);
    tabCOLOBJECT* o = BMP_StartColObject(base - 0x4c00, base - 0x6a00, 0x1400);
    o->flags |= 0x40;
    BMP_StartColObject(base - 0x7000, 0x7200, 0x2600);
    int32_t a = 0, b = -0x1800;
    for (int i = 7; i != 0; i--) {
        BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), a + base - 0x4c00, b + base - 0x6a00);
        Rotate(a, b, gHostSineTable[0x100 / 4], gHostSineTable[0x700 / 4]);
    }
    a = 0;
    b = 0x2000;
    for (int i = 7; i != 0; i--) {
        BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), a + base - 0x7000, b + 0x7200);
        Rotate(a, b, gHostSineTable[0x100 / 4], gHostSineTable[0x700 / 4]);
    }
    ride[0x50 / 4] = 2;
}

void Track(uint32_t* ride, int32_t x, int32_t y) {
    ride[0x90 / 4] = static_cast<uint32_t>(x);
    ride[0x74 / 4] = static_cast<uint32_t>(x);
    ride[0x94 / 4] = static_cast<uint32_t>(y);
    ride[0x78 / 4] = static_cast<uint32_t>(y);
    for (int32_t i = 0; i < static_cast<int32_t>(ride[0x64 / 4]); i++) {
        int32_t n = static_cast<int32_t>(ride[0x64 / 4]);
        uint32_t rot = ride[0xb0 / 4];
        int32_t a = (n - i) * 1024 - 0x800;
        int32_t b = 0x400 - ((i << 11) & 0x800);
        if (rot & 1) { int32_t t = a; a = b; b = t; }
        if (rot & 2) a = -a;
        if (rot & 4) b = -b;
        uint32_t* buoy = Words(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + a, y + b));
        buoy[0] |= 2;
    }
    int32_t a, b;
    Orient(ride[0xb0 / 4], a, b);
    uint32_t* buoy = Words(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + a, y + b));
    buoy[0] |= 4;
    ride[0x50 / 4] = 0;
    SetPerformance(ride);
}

void Single(uint32_t* ride, int32_t x, int32_t y) {
    ride[0x74 / 4] = static_cast<uint32_t>(x);
    ride[0x78 / 4] = static_cast<uint32_t>(y);
    int32_t a, b;
    Orient(ride[0xb0 / 4], a, b);
    uint32_t* buoy = Words(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + a, y + b));
    buoy[0] |= 4;
    ride[0x90 / 4] = ride[0x74 / 4] + static_cast<uint32_t>(MulHw(0x55555556, a) + static_cast<int32_t>(static_cast<uint32_t>(MulHw(0x55555556, a)) >> 31));
    ride[0x94 / 4] = ride[0x78 / 4] + static_cast<uint32_t>(MulHw(0x55555556, b) + static_cast<int32_t>(static_cast<uint32_t>(MulHw(0x55555556, b)) >> 31));
    ride[0x50 / 4] = 0;
    SetPerformance(ride);
}

void Dispatch(int target, uint32_t* ride, int32_t x, int32_t y) {
    switch (target) {
    case kShip: Ship(ride); break;
    case kSub: {
        uint32_t idx = ride[0] + 14;
        Dispatch(idx > 13 ? kS3 : kSubTable[idx], ride, x, y);
        break;
    }
    case kTrack: Track(ride, x, y); break;
    case kSingle: Single(ride, x, y); break;
    case kS1:
    case kS2:
    case kS3: {
        x += 0x600;
        y += 0x600;
        int32_t shape = target == kS1 ? 0x1200 : target == kS2 ? 0x1100 : 0x1600;
        int32_t dy = target == kS1 ? 0xc00 : target == kS2 ? 0xb00 : 0x1000;
        BMP_StartColObjectForRide(reinterpret_cast<tagBUMPRIDE*>(ride), x, y, shape);
        RingOfBuoys(ride, x, y, 0, dy);
        break;
    }
    default: break;
    }
}

}  // namespace

int32_t BMP_CreateRide(int32_t kind, int32_t x, int32_t y, int32_t rot, int32_t id, int32_t f10) {
    uint32_t* world = World();
    if (world[0] != 0x4a454647u) return 0;
    uint32_t* ride;
    if (id != 0) {
        ride = RideOfId(id);
        kind = id >> 8;
    } else {
        ride = nullptr;
        uint32_t* p = Rides();
        if (p != nullptr) {
            for (int i = 0; i < 0x40; i++, p += 0x34) {
                if (p[0] == 0) { ride = p; break; }
            }
        }
    }
    // A NULL ride (no free record) is read at the end like the original does: the Mac reads low memory, the host would fault.
    if (ride != nullptr) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(reinterpret_cast<uintptr_t>(gRideTemplates) + static_cast<uint32_t>(-kind) * 0xd0u - 0xd0u);   // the listing copies from base + (-kind) * 0xd0 - 0xd0
        for (int i = 0; i < 0x34; i++) ride[i] = src[i];
        world[3] = world[3] + 1;
        ride[0x5c / 4] = 0;
        ride[0xac / 4] = 0;
        ride[0xa8 / 4] = 0;
        ride[0xc0 / 4] = 0;
        ride[0xbc / 4] = 0;
        ride[0xb4 / 4] = static_cast<uint32_t>(x);
        ride[0xb8 / 4] = static_cast<uint32_t>(y);
        ride[0xb0 / 4] = static_cast<uint32_t>(rot);
        ride[0xcc / 4] = 0;
        ride[0x58 / 4] = 0;
        ride[0x10 / 4] = static_cast<uint32_t>(f10);
        uint32_t idx = ride[0] + 14;
        if (idx <= 13) Dispatch(kTop[idx], ride, x, y);
    }
    return IdOf(ride);
}
