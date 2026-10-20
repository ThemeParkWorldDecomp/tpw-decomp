// Test library for the leaves of src/gameplay_leaves.cpp (unit v5L07; libtpw_unit_GameplayLeaves.dylib,
// tests/test_GameplayLeaves.py): CellIsRide, LocatorByeByeKid, CObject::IsCoaster and CAdvisor::FinalDestroy.
// The staff panels' thing and the ride database (raw storage under its symbol name) are defined here;
// RemoveSlideoutPanel, CRideDatabase::GetRideInfo and CThing::FinalDestroy are fakes that record.
#include <cstdint>
#include <cstring>

#include "AdvisorQueue.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "calllog.h"
#include "ui_main.h"

using namespace calllog;

CThing* gStaffPanelThing;

alignas(16) uint8_t gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");
alignas(16) uint8_t gRideInfoStorage[sizeof(RideInfo)];
alignas(16) uint8_t gObjectStorage[sizeof(CObject)];
alignas(16) uint8_t gAdvisorStorage[0x100];

void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }

// The record the case gives every type.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(gRideInfoStorage);
}

void CThing::FinalDestroy() { ev("CThing::FinalDestroy", {lab(this)}); }

extern "C" {

// The staff panels' thing.
void tpw_gl_set(uint64_t thing) {
    gStaffPanelThing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing));
}

int32_t tpw_gl_cell_is_ride(int32_t mode) { return CellIsRide(static_cast<CELL_MODES>(mode)); }
void tpw_gl_bye_bye(uint64_t thing) { LocatorByeByeKid(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }

void* tpw_gl_database() { return gRideDatabaseStorage; }
void* tpw_gl_advisor() { return gAdvisorStorage; }

// The object's type and its record's track type.
int32_t tpw_gl_is_coaster(uint32_t type, int32_t track) {
    std::memset(gObjectStorage, 0, sizeof gObjectStorage);
    std::memset(gRideInfoStorage, 0, sizeof gRideInfoStorage);
    CObject* object = reinterpret_cast<CObject*>(gObjectStorage);
    object->mObjectType = static_cast<uint16_t>(type);
    reinterpret_cast<RideInfo*>(gRideInfoStorage)->Value(kRideSlot_WhichTrackType).mInteger = track;
    return object->IsCoaster();
}

void tpw_gl_advisor_final_destroy() { reinterpret_cast<CAdvisor*>(gAdvisorStorage)->FinalDestroy(); }

}  // extern "C"
