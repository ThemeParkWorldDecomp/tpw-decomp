// Test library for the advisor's bump launch and collision functions (unit v5062; libtpw_unit_SetupHeadIDs.dylib, tests/test_SetupHeadIDs.py), main app:
//   src/SetupHeadIDs.cpp   SetupHeadIDs, BMP_AddJoiningListToBump, BMP_LAUNCH, BMP_Collision
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs): FindDataNodeIndexByID,
// EnableRideHead, PAR_InitEmitter, SetControlType, PointInColObject, BMP_StartBump, CRideDatabase::GetRideInfo, InstanceRide,
// SetCurRideAnim. Pointers into the test's world block are logged as their offset in it. The globals of other files the unit reads are
// defined here: gColList, gRideAnimSets, gRideDatabase, gHostSineTable.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "BMP_InsideArea.h"
#include "RideDB.h"
#include "analyser.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "effect_manager.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
tabCOLLIST gColList;
tag_sRideAnimSet* gRideAnimSets[16];
CRideDatabase gRideDatabase;
int32_t gHostSineTable[512];

namespace {

uint64_t gW = 0;          // the world block (the test sets it)
uint64_t gStartRet = 0;   // BMP_StartBump's answer
uint64_t gInfoRet = 0;    // GetRideInfo's answer
const uint64_t kWSize = 0x4000;

int64_t Wl(const void* p) {
    const uint64_t v = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
    if (v >= gW && v < gW + kWSize) return static_cast<int64_t>(v - gW);
    return static_cast<int64_t>(v);
}

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("Find", {Wl(set), static_cast<int32_t>(mask), static_cast<int32_t>(id)});
    return static_cast<uint32_t>(ret("Find"));
}

void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) {
    ev("Enable", {Wl(mesh), static_cast<int32_t>(index), static_cast<int32_t>(peep)});
}

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("Emitter", {type, a, b, c});
    return 0;
}

void SetControlType(tagBUMPRIDE* ride, tagBUMP* bump) { ev("SetControlType", {Wl(ride), Wl(bump)}); }

int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object) {
    ev("Point", {x, y, Wl(object)});
    return static_cast<int32_t>(ret("Point"));
}

tagBUMP* BMP_StartBump(int32_t x, int32_t y, int32_t type) {
    ev("StartBump", {x, y, type});
    return reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(gStartRet));
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {this == &gRideDatabase ? 1 : 0, id.mIndex});
    return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(gInfoRet));
}

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e,
                      uint32_t f, int32_t g) {
    ev("InstanceRide", {static_cast<int32_t>(type), static_cast<int32_t>(flags), Wl(params), static_cast<int32_t>(a), b, Wl(c), d, e,
                        static_cast<int32_t>(f), g});
    return static_cast<uint32_t>(ret("Instance"));
}

void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {Wl(set), static_cast<int32_t>(a), static_cast<int32_t>(anim), static_cast<int32_t>(loop), CanonBits(speed),
                          static_cast<int32_t>(b)});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"list", &gColList},         {"sets", gRideAnimSets}, {"sine", gHostSineTable}, {"W", &gW},
        {"start", &gStartRet},       {"info", &gInfoRet},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The arena base: the words that hold addresses hold offsets from it.
uint64_t tpw_sh_base() {
    Ptr32<char> pointer;
    pointer.mOffset = 1;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer.get())) - 1;
}

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_sh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mSubRideTypes));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    out[n++] = static_cast<int64_t>(sizeof(tabCOLLIST));
    out[n++] = static_cast<int64_t>(sizeof(gRideAnimSets[0]));
    return n;
}

void tpw_sh_SetupHeadIDs(uint64_t bump, int32_t flag) { SetupHeadIDs(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)), flag); }
uint32_t tpw_sh_BMP_AddJoiningListToBump(int32_t ride) { return static_cast<uint32_t>(BMP_AddJoiningListToBump(ride)); }
uint32_t tpw_sh_BMP_LAUNCH(int32_t ride, int32_t flags) { return static_cast<uint32_t>(BMP_LAUNCH(ride, flags)); }
void tpw_sh_BMP_Collision(uint64_t bump) { BMP_Collision(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump))); }

}  // extern "C"
