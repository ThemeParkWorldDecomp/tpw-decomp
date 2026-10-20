// Test library for UIMain_GetObjectState (unit v5694; libtpw_unit_UIMain_GetObjectState.dylib, tests/test_UIMain_GetObjectState.py), main app:
//   src/ui_main_panel_UIMain_GetObjectState.cpp   UIMain_GetObjectState
//
// The callees outside the unit are recording fakes that answer from the test's queues; the globals of other files the unit reads are defined
// here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "World.h"
#include "ui_main.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

CWorld* gGameWorld;
CRideDatabase gRideDatabase;
uint16_t gUIMainId7858;
uint16_t gUIMainId785A;
uint16_t gUIMainId785C;
uint16_t gUIMainId785E;
uint16_t gUIMainId7860;
uint16_t gUIMainId7862;
uint16_t gUIMainId7864;

namespace {
RideInfo* gFakeRide;
}

// ---- the fakes
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {id.mIndex});
    return gFakeRide;
}
int32_t CObject::IsCondemned() const {
    ev("IsCondemned", {});
    return static_cast<int32_t>(ret("IsCondemned"));
}
int32_t IsTrackClosed(void*) {
    ev("IsTrackClosed", {});
    return static_cast<int32_t>(ret("IsTrackClosed"));
}
int32_t IsTrackValid(void*) {
    ev("IsTrackValid", {});
    return static_cast<int32_t>(ret("IsTrackValid"));
}
int32_t CObject::RequiresUpgrade() const {
    ev("RequiresUpgrade", {});
    return static_cast<int32_t>(ret("RequiresUpgrade"));
}
CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {});
    return reinterpret_cast<CStaffHQ*>(static_cast<uintptr_t>(0x1000));
}
int32_t CStaffHQ::IsStriking(StaffType type) {
    ev("IsStriking", {static_cast<int64_t>(type)});
    return static_cast<int32_t>(ret("IsStriking"));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetAssignedStaffMember() {
    ev("GetAssignedStaffMember", {});
    return {static_cast<uint16_t>(ret("GetAssignedStaffMember"))};
}
int32_t CObject::IsBeingWorkedUpon() {
    ev("IsBeingWorkedUpon", {});
    return static_cast<int32_t>(ret("IsBeingWorkedUpon"));
}
int32_t CObject::IsFlaggedForMaintenance() const {
    ev("IsFlaggedForMaintenance", {});
    return static_cast<int32_t>(ret("IsFlaggedForMaintenance"));
}
bool CObject::IsBrokenDown() const {
    ev("IsBrokenDown", {});
    return ret("IsBrokenDown") != 0;
}
int32_t CObject::IsBOQConnected() {
    ev("IsBOQConnected", {});
    return static_cast<int32_t>(ret("IsBOQConnected"));
}
bool CObject::IsClosed() const {
    ev("IsClosed", {});
    return ret("IsClosed") != 0;
}
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    ev("UpgradesWhat", {});
    return {static_cast<uint16_t>(ret("UpgradesWhat"))};
}
int32_t CObject::IsExitCellConnected() {
    ev("IsExitCellConnected", {});
    return static_cast<int32_t>(ret("IsExitCellConnected"));
}

extern "C" {

uint64_t tpw_gos_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},       {"id7858", &gUIMainId7858}, {"id785A", &gUIMainId785A}, {"id785C", &gUIMainId785C},
        {"id785E", &gUIMainId785E},   {"id7860", &gUIMainId7860}, {"id7862", &gUIMainId7862}, {"id7864", &gUIMainId7864},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gos_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_gos_setride(uint64_t ride) { gFakeRide = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(ride)); }

int32_t tpw_gos_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73C));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73E));
    return n;
}

int32_t tpw_gos_state(uint64_t object, int32_t flag) {    return UIMain_GetObjectState(reinterpret_cast<CObject*>(static_cast<uintptr_t>(object)), flag);
}

}  // extern "C"
