// Test library for UIMain_GotoStatusPanelForThing (unit v5071; libtpw_unit_UIMain_GotoStatusPanelForThing.dylib,
// tests/test_UIMain_GotoStatusPanelForThing.py), main app:
//   src/ui_main_kbcapture_UIMain_ShowRidePreview_UIMain_GotoStatusPanelForThing.cpp   the function and the two halfword globals
//
// The callees outside the unit (the panels' Show, CObject::GetDBInfo, GetUpgradeParent, UpgradesWhat) are fakes that record the arguments
// the real ones read; the thing array and the panel objects of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
CThingArrayEntry gThingArray[kThingArraySize];
RideStatusPanel gRideStatusPanel;
UpgradeStatusPanel gUpgradeStatusPanel;
ShopStatusPanel gShopStatusPanel;
ShowStatusPanel gShowStatusPanel;
BogStatusPanel gBogStatusPanel;
FeatureStatusPanel gFeatureStatusPanel;
StaffRoomStatusPanel gStaffRoomStatusPanel;
StaffStatusPanel gStaffStatusPanel;
VisitorStatusPanel gVisitorStatusPanel;

// ---- the fakes
void RideStatusPanel::Show(CThing* t) { ev("RideShow", {lab(this), lab(t)}); }
void UpgradeStatusPanel::Show(CThing* t) { ev("UpgradeShow", {lab(this), lab(t)}); }
void ShopStatusPanel::Show(CThing* t) { ev("ShopShow", {lab(this), lab(t)}); }
void ShowStatusPanel::Show(CThing* t) { ev("ShowShow", {lab(this), lab(t)}); }
void BogStatusPanel::Show(CThing* t) { ev("BogShow", {lab(this), lab(t)}); }
void FeatureStatusPanel::Show(CThing* t) { ev("FeatureShow", {lab(this), lab(t)}); }
void StaffRoomStatusPanel::Show(CThing* t) { ev("StaffRoomShow", {lab(this), lab(t)}); }
void StaffStatusPanel::Show(CThing* t) { ev("StaffShow", {lab(this), lab(t)}); }
void VisitorStatusPanel::Show(CThing* t) { ev("VisitorShow", {lab(this), lab(t)}); }

// A fake on both sides: records the condition and the text.
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    uint32_t sum = 0;   // the text as a checksum (the PowerPC stub computes the same)
    for (uint32_t i = 0; format[i] != 0; ++i) sum += (i + 1) * static_cast<uint8_t>(format[i]);
    ev("assert", {condition, static_cast<int64_t>(sum)});
}

// The thing block holds, at 0x40, the address of the ride record the test built for it.
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    uint64_t word;   // a full native pointer on the host
    std::memcpy(&word, reinterpret_cast<const char*>(this) + 0x40, 8);
    return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(word));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetUpgradeParent() const {
    ev("GetUpgradeParent", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r{};
    r.mIndex = static_cast<uint16_t>(ret("GetUpgradeParent"));
    return r;
}

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    ev("UpgradesWhat", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> r{};
    r.mIndex = static_cast<uint16_t>(ret("UpgradesWhat"));
    return r;
}

extern "C" {

uint64_t tpw_gsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ride", &gRideStatusPanel},       {"upgrade", &gUpgradeStatusPanel}, {"shop", &gShopStatusPanel},
        {"show", &gShowStatusPanel},       {"bog", &gBogStatusPanel},         {"feature", &gFeatureStatusPanel},
        {"staffroom", &gStaffRoomStatusPanel}, {"staff", &gStaffStatusPanel}, {"visitor", &gVisitorStatusPanel},
        {"array", &gThingArray[0]},        {"none1", &gNoUpgradeParent},      {"none2", &gNoUpgradeTarget},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    return n;
}

void tpw_gsp_go(uint64_t thing) { UIMain_GotoStatusPanelForThing(reinterpret_cast<void*>(static_cast<uintptr_t>(thing))); }

}  // extern "C"
