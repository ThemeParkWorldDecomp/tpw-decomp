// Test library for the bump AI and its helpers (unit v5926; libtpw_unit_BMP_MaskCar_2.dylib, tests/test_BMP_MaskCar_2.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_MaskCar, BMP_Thrust, SetNextBuoyTarget, BMP_AI
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "TPEngine.h"
#include "TPEmbed.h"
#include "TPSound.h"
#include "Debug.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tabCOLLIST gColList;            // 0x102b56c0
int32_t gHostSineTable[512];    // 0x102b4ec0
tag_sRideAnimSet* gRideAnimSets[8];   // 0x102c79a4
CWorld* gGameWorld;             // 0x1037ebfc

static tabCOLOBJECT* gClosest;

// ---- the fakes
int32_t BMP_Distance(int32_t dx, int32_t dy) {
    ev("BMP_Distance", {dx, dy});
    return static_cast<int32_t>(ret("BMP_Distance"));
}
int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object) {
    ev("PointInColObject", {x, y, lab(object)});
    return static_cast<int32_t>(ret("PointInColObject"));
}
tabCOLOBJECT* PointInAnyColObject(int32_t x, int32_t y) {
    ev("PointInAnyColObject", {x, y});
    return reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(ret("PointInAnyColObject")));
}
tabCOLOBJECT* ClosestIntersectingColObjectToPoint(tabCOLOBJECT* area, int32_t px, int32_t py, int32_t includeSelf) {
    ev("ClosestIntersectingColObjectToPoint", {lab(area), px, py, includeSelf});
    return gClosest;
}
int32_t SetNavRoute(tagBUMP* bump, int32_t x, int32_t y) {
    ev("SetNavRoute", {lab(bump), x, y});
    return static_cast<int32_t>(ret("SetNavRoute"));
}
void SetControlType(tagBUMPRIDE* ride, tagBUMP* bump) { ev("SetControlType", {lab(ride), lab(bump)}); }
void BMP_LeaveBump(tagBUMP* bump) { ev("BMP_LeaveBump", {lab(bump)}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
int32_t BMP_DOCK(tagBUMP* bump) {
    ev("BMP_DOCK", {lab(bump)});
    return static_cast<int32_t>(ret("BMP_DOCK"));
}
void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
static int64_t Field(HFieldXZ h) { return (static_cast<int64_t>(h.mF00) << 16) | h.mF02; }
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("DisableMeshDraw3", {lab(mesh), Field(from), Field(to)}); }
void DisableMeshDraw(tag_sMesh* mesh) { ev("DisableMeshDraw1", {lab(mesh)}); }
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) { ev("EnableMeshDraw3", {lab(mesh), Field(from), Field(to)}); }
void EnableMeshDraw(tag_sMesh* mesh) { ev("EnableMeshDraw1", {lab(mesh)}); }
void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {lab(mesh), index}); }
void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) { ev("EnableRideHead", {lab(mesh), index, peep}); }

extern "C" {

uint64_t tpw_mc2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"list", &gColList},
        {"sine", gHostSineTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_mc2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word of a host address.
uint32_t tpw_mc2_off(uint64_t address) { return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

void tpw_mc2_setworld(uint64_t world) { gGameWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world)); }
void tpw_mc2_setcc(uint64_t area) { gClosest = reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(area)); }
void tpw_mc2_setrideset(uint32_t index, uint64_t set) { gRideAnimSets[index] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set)); }
void tpw_mc2_setanim(uint64_t set, uint32_t flags, uint64_t mesh, uint32_t fc0, uint32_t fc4) {
    tag_sRideAnimSet* s = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set));
    s->mFlags = flags;
    s->mHeads = reinterpret_cast<tag_sMesh*>(static_cast<uintptr_t>(mesh));
    s->mFC0.mF00 = static_cast<uint16_t>(fc0 >> 16);
    s->mFC0.mF02 = static_cast<uint16_t>(fc0);
    s->mFC4.mF00 = static_cast<uint16_t>(fc4 >> 16);
    s->mFC4.mF02 = static_cast<uint16_t>(fc4);
}

int32_t tpw_mc2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabCOLLIST));
    return n;
}

void tpw_mc2_ai(uint64_t bump) { BMP_AI(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump))); }
void tpw_mc2_thrust(uint64_t bump, int32_t amount) { BMP_Thrust(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)), amount); }
void tpw_mc2_buoy(uint64_t buoy, uint64_t bump) {
    SetNextBuoyTarget(reinterpret_cast<tabBUOY*>(static_cast<uintptr_t>(buoy)), reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)));
}
void tpw_mc2_mask(uint64_t bump, int32_t mask) { BMP_MaskCar(reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)), mask); }

}  // extern "C"
