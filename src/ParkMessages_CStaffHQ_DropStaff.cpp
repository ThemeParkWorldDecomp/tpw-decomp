// CStaffHQ::DropStaff 0x1012c168 and CStaffHQ::ForceStrike 0x1012c04c (unit v5037).
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"

namespace {
// 0x10245ff2
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
}  // namespace

void CStaffHQ::DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint16_t row = mF60;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    static_cast<CStaff*>(gThingArray[row].mThing)->DropMe(cell);
    mF60 = 0;
}

void CStaffHQ::ForceStrike(StaffType type) {
    if (mF64 == 0 || static_cast<int32_t>(mStrikes[static_cast<int32_t>(type)].mF00) <= 1) {
        CTRACE(2, "Forcing strike\n");   // 0x1024634a
        mF64 = 1;
    } else {
        CTRACE(2, "Turning off force strike\n");   // 0x10246330
        mF64 = 0;
    }
}
