// Unit v5108: the eight "max staff" advisor scorers (CAdvisorScoring::_ADVISOR_MAX_*, 0x10016e68 .. 0x10017460).
// Each compares a staff-limit slot of gMainBalance (Mac +0x2a8 .. +0x2b8) with a slot of the advisor's own balance table, then asks the park
// analyser how many staff of one type there are. The Mac offsets in the comments are of the 4-byte slots; the host's slots are wider.
#include <cstdint>

#include "AdvisorScoring.h"
#include "MainBalance.h"
#include "World.h"
#include "analyser.h"

namespace {

// The advisor table's value at the Mac offset `offset` of the scoring object (the table is at 0x0c, its values 8 on).
int32_t AdvisorSlot(CAdvisorScoring* self, int32_t offset) { return self->mBalance.mValues[(offset - 0x14) / 4].mInteger; }

// gMainBalance's value at the Mac offset `offset` (values start at 8).
int32_t MainSlot(int32_t offset) { return gMainBalance.mValues[(offset - 8) / 4].mInteger; }

int32_t StaffCount(int32_t model) {
    return gGameWorld->GetParkAnalyser()->NumberOfStaffType(static_cast<CThing::ThingModel>(model));
}

}  // namespace

// 0x10016e68
int32_t CAdvisorScoring::_ADVISOR_MAX_HANDYMEN_V2() {
    if (MainSlot(0x2a8) == AdvisorSlot(this, 0x38c)) return 0;
    if (StaffCount(5) == MainSlot(0x2a8)) return AdvisorSlot(this, 0x394);
    return 0;
}

// 0x10016f2c
int32_t CAdvisorScoring::_ADVISOR_MAX_ENTERTAINERS() {
    if (MainSlot(0x2b0) != AdvisorSlot(this, 0x398)) return 0;
    if (StaffCount(6) < AdvisorSlot(this, 0x398)) return 0;
    return AdvisorSlot(this, 0x39c);
}

// 0x10016fe8
int32_t CAdvisorScoring::_ADVISOR_MAX_ENTERTAINERS_V2() {
    if (MainSlot(0x2b0) == AdvisorSlot(this, 0x398)) return 0;
    if (StaffCount(6) == MainSlot(0x2b0)) return AdvisorSlot(this, 0x3a0);
    return 0;
}

// 0x10017168
int32_t CAdvisorScoring::_ADVISOR_MAX_MECHANICS_V2() {
    if (MainSlot(0x2ac) == AdvisorSlot(this, 0x3a4)) return 0;
    if (StaffCount(4) == MainSlot(0x2ac)) return AdvisorSlot(this, 0x3ac);
    return 0;
}

// 0x10017230
int32_t CAdvisorScoring::_ADVISOR_MAX_GUARDS() {
    if (MainSlot(0x2b4) != AdvisorSlot(this, 0x3b0)) return 0;
    if (StaffCount(7) < AdvisorSlot(this, 0x3b0)) return 0;
    return AdvisorSlot(this, 0x3b4);
}

// 0x100172e4
int32_t CAdvisorScoring::_ADVISOR_MAX_GUARDS_V2() {
    if (MainSlot(0x2b4) == AdvisorSlot(this, 0x3b0)) return 0;
    if (StaffCount(7) == MainSlot(0x2b4)) return AdvisorSlot(this, 0x3b8);
    return 0;
}

// 0x100173a8 (the guard on the balance slot reads 0x3b0, the threshold 0x3bc: as in the listing)
int32_t CAdvisorScoring::_ADVISOR_MAX_RESEARCHERS() {
    if (MainSlot(0x2b4) != AdvisorSlot(this, 0x3b0)) return 0;
    if (StaffCount(8) < AdvisorSlot(this, 0x3bc)) return 0;
    return AdvisorSlot(this, 0x3c0);
}

// 0x10017460
int32_t CAdvisorScoring::_ADVISOR_MAX_RESEARCHERS_V2() {
    if (MainSlot(0x2b8) == AdvisorSlot(this, 0x3bc)) return 0;
    if (StaffCount(8) == MainSlot(0x2b8)) return AdvisorSlot(this, 0x3c4);
    return 0;
}
