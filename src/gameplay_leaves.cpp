// gameplay_leaves.cpp (main, unit v5L07): small leaves a park run reaches, from different Mac files: CellIsRide 0x1006d718
// (the inventory's Debug.cpp range), LocatorByeByeKid 0x101bf98c (ui_staffPanel.cpp), CObject::IsCoaster 0x10110cd8 (Object.cpp) and CAdvisor::FinalDestroy 0x1000a0ec (Advisor.cpp).
#include <cstdint>

#include "AdvisorQueue.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ui_main.h"

// 0x1006d718 (40 bytes): a cell mode is a ride's when any of its low four bits is set (the Mac tests 4, then 9, then 0xa).
int32_t CellIsRide(CELL_MODES mode) {
    const int32_t bits = static_cast<int32_t>(mode);
    if ((bits & 4) != 0 || (bits & 9) != 0 || (bits & 0xa) != 0) return 1;
    return 0;
}

// 0x101bf98c (56 bytes): a kid leaves; when the staff panels show it, the slide-out panel 1 is closed.
void LocatorByeByeKid(CThing* thing) {
    if (gStaffPanelThing == thing) RemoveSlideoutPanel(1);
}

// 0x10110cd8 (76 bytes): the object's ride record has track type 3. The record is not tested for NULL (the Mac reads through it).
int32_t CObject::IsCoaster() const {
    const RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType});
    return info->Value(kRideSlot_WhichTrackType).mInteger == 3 ? 1 : 0;
}

// 0x1000a0ec (36 bytes): the advisor has nothing of its own to free; the thing's part is.
void CAdvisor::FinalDestroy() { reinterpret_cast<CThing*>(this)->FinalDestroy(); }
