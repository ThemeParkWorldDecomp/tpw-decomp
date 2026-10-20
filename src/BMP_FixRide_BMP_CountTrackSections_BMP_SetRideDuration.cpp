// Unit v51036: ride commands on the bump world (main app): BMP_SetRideDuration, BMP_RideWorn, BMP_GetCameraMesh, BMP_StartBump,
// BMP_StartBuoy, BMP_AddBuoyToRide, and the static initialiser of HoardingsBalance.cpp.
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "AdvisorScoring.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "HoardingsBalance.h"
#include "TPAdvisor.h"
#include "rsse.h"

namespace {

constexpr uint32_t kWorldMagic = 0x4a454647;

uint8_t* World() { return reinterpret_cast<uint8_t*>(&gColList); }

uint32_t& W(void* base, uint32_t offset) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(base) + offset); }

Ptr32<uint8_t>& P(void* base, uint32_t offset) {
    return *reinterpret_cast<Ptr32<uint8_t>*>(static_cast<uint8_t*>(base) + offset);
}

// The ride record the id names (NULL when the world is not live) and the id it holds: kind << 8 | index.
uint8_t* LookupRide(int32_t ride, int32_t* id) {
    uint8_t* world = World();
    uint8_t* record = nullptr;
    if (W(world, 0) == kWorldMagic) record = P(world, 8).get() + static_cast<uint32_t>(ride & 0xff) * 0xd0;
    if (W(world, 0) == kWorldMagic) {
        int32_t index = static_cast<int32_t>(record - P(world, 8).get()) / 0xd0;
        *id = static_cast<int32_t>((W(record, 0) << 8) | static_cast<uint32_t>(index));
    } else {
        *id = 0;
    }
    return record;
}

}  // namespace

// 0x10026b14
int32_t BMP_SetRideDuration(int32_t ride, int32_t duration) {
    int32_t id;
    uint8_t* record = LookupRide(ride, &id);
    if (ride == id) {
        W(record, 4) = static_cast<uint32_t>(duration);
        TRACE("Set Ride Duration to %d\n", duration);   // the original leaves the duration in r4
    }
    return 0;
}

// 0x100263f4
int32_t BMP_RideWorn(int32_t ride) {
    int32_t id;
    uint8_t* record = LookupRide(ride, &id);
    if (ride == id) {
        W(record, 0x54) = 1;
        TRACE("Ride worn out\n");
    }
    return 0;
}

// 0x1002cf30
int32_t BMP_GetCameraMesh(int32_t ride) {
    int32_t id;
    uint8_t* record = LookupRide(ride, &id);
    if (ride == id) {
        uint8_t* mesh = P(record, 0x58).get();
        if (mesh != nullptr && (W(mesh, 0) & 0x400000) != 0) return static_cast<int32_t>(W(mesh, 8));
    }
    return 0;
}

// 0x1002eaac
tagBUMP* BMP_StartBump(int32_t x, int32_t y, int32_t type) {
    uint8_t* world = World();
    uint8_t* bump = P(world, 0x10).get();
    if (bump != nullptr) {
        int32_t i = 0;
        for (; i < 0x100; ++i, bump += 0xac)
            if ((W(bump, 0) & 1) == 0) break;
        if (i >= 0x100) bump = nullptr;
    }
    if (bump == nullptr) return nullptr;
    W(world, 0x14) += 1;
    std::memset(bump, 0, 0xac);
    W(bump, 0) = 1;
    W(bump, 0x34) = static_cast<uint32_t>(x);
    W(bump, 0x38) = static_cast<uint32_t>(y);
    W(bump, 0x64) = static_cast<uint32_t>(type);
    W(bump, 0x80) = 0xffffffffu;
    W(bump, 0x7c) = 0xffffffffu;
    W(bump, 0x2c) = 0xffffffffu;
    W(bump, 0x28) = 0xffffffffu;
    W(bump, 0x24) = 0xffffffffu;
    W(bump, 0x20) = 0;
    W(bump, 0x30) = 0;
    W(bump, 0xa4) = 0;
    W(bump, 0xa8) = 0;
    float zero = 0.0f;
    std::memcpy(bump + 0xa0, &zero, 4);
    return reinterpret_cast<tagBUMP*>(bump);
}

// 0x1002ec84
tabBUOY* BMP_StartBuoy(int32_t x, int32_t y) {
    uint8_t* world = World();
    uint8_t* buoy = P(world, 0x20).get();
    bool found = false;
    if (buoy != nullptr) {
        for (uint32_t i = 0; static_cast<int32_t>(i) < 0x400; ++i, buoy += 0x1c) {
            if ((W(buoy, 0) & 1) == 0) {
                W(buoy, 4) = i;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        TRACE("Out of Buoys\n");
        return nullptr;
    }
    W(world, 0x24) += 1;
    W(buoy, 0) = 1;
    W(buoy, 8) = static_cast<uint32_t>(x);
    W(buoy, 0xc) = static_cast<uint32_t>(y);
    W(buoy, 0x10) = 0;
    W(buoy, 0x14) = 0;
    W(buoy, 0x18) = 0;
    return reinterpret_cast<tabBUOY*>(buoy);
}

// 0x1002ebd0: the original leaves the buoy (or 0) in r3.
tabCOLOBJECT* BMP_AddBuoyToRide(tagBUMPRIDE* ride, int32_t x, int32_t y) {
    uint8_t* r = reinterpret_cast<uint8_t*>(ride);
    uint8_t* buoy = reinterpret_cast<uint8_t*>(BMP_StartBuoy(x, y));
    if (buoy != nullptr) {
        P(buoy, 0x10).set(r);
        W(r, 0xac) += 1;
        uint8_t* last = P(r, 0xa8).get();
        if (last == nullptr) {
            P(r, 0xa8).set(buoy);
        } else {
            while (P(last, 0x14).get() != nullptr) last = P(last, 0x14).get();
            P(last, 0x14).set(buoy);
        }
    }
    return reinterpret_cast<tabCOLOBJECT*>(buoy);
}

// 0x10021074: the vtable stores are the constructors' (the objects are built by the host's static initialisation).
// Stand-ins for the destructor vector 0x10265fa0 and the registration nodes 0x102b455c, 0x102b4340.
uint8_t gHoardingsDtorVector[16];
uint8_t gHoardingsNode434C[16];
uint8_t gHoardingsNode4130[16];

void __sinit_HoardingsBalance_cpp() {
    gHoardingsBalance434C.Init();
    __register_global_object(&gHoardingsBalance434C, gHoardingsDtorVector, gHoardingsNode434C);
    gHoardingsBalance4130.Init();
    __register_global_object(&gHoardingsBalance4130, gHoardingsDtorVector, gHoardingsNode4130);
}
