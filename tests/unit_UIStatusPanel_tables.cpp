// Test library for the status panels' tables (unit v5L31; libtpw_unit_UIStatusPanel_tables.dylib, tests/test_UIStatusPanel_tables.py):
//   src/ui_StatusPanel_tables.cpp   UIStatusPanelTable and the nine XxxStatusPanelTable
//
// Every member a table names is a fake recording its Mac address, its panel and its argument, so the test can call each slot through
// the table (UIStatusPanel_CallSlot, as the base's members do) and compare the member reached with the one the Mac's vtable names.
#include <cstdint>

#include "platform/arena.h"
#include "ui_main.h"

namespace {
uint32_t gReached;      // the Mac address of the member the last call reached (0 none)
const void* gPanel;     // its panel
const void* gArg;       // its argument (Show's thing, ChangeName's name)

void Rec(uint32_t address, const void* panel, const void* arg) {
    gReached = address;
    gPanel = panel;
    gArg = arg;
}

// The panel the calls go through: a block of the arena holding the table's offset at +0x20, made on first use.
UIStatusPanelFields* Panel() {
    static UIStatusPanelFields* panel = static_cast<UIStatusPanelFields*>(tpw::ArenaAllocZeroed(0x40));
    return panel;
}

uint32_t* Table(uint32_t which) {
    switch (which) {
        case 0: return UIStatusPanelTable();
        case 1: return RideStatusPanelTable();
        case 2: return ShopStatusPanelTable();
        case 3: return ShowStatusPanelTable();
        case 4: return StaffStatusPanelTable();
        case 5: return StaffRoomStatusPanelTable();
        case 6: return UpgradeStatusPanelTable();
        case 7: return FeatureStatusPanelTable();
        case 8: return BogStatusPanelTable();
        case 9: return VisitorStatusPanelTable();
    }
    return nullptr;
}
}  // namespace

void UIStatusPanel::InitWindow() { Rec(0x10193398, this, nullptr); }
void UIStatusPanel::UpdateWindow() { Rec(0x101933cc, this, nullptr); }
void UIStatusPanel::DeInitWindow() { Rec(0x10193404, this, nullptr); }
void UIStatusPanel::InitStatusWindow() { Rec(0x1019343c, this, nullptr); }
void UIStatusPanel::InitViewWindow() { Rec(0x10193478, this, nullptr); }
void UIStatusPanel::UpdateStatusWindow() { Rec(0x101934b0, this, nullptr); }
void UIStatusPanel::UpdateViewWindow() { Rec(0x101934ec, this, nullptr); }
void UIStatusPanel::DeInitStatusWindow() { Rec(0x10193528, this, nullptr); }
void UIStatusPanel::DeInitViewWindow() { Rec(0x10193564, this, nullptr); }
void UIStatusPanel::DisplayStatusText() { Rec(0x10193af4, this, nullptr); }
void UIStatusPanel::Show(CThing* thing) { Rec(0x101937dc, this, thing); }
void UIStatusPanel::Remove() { Rec(0x10193814, this, nullptr); }
void UIStatusPanel::ChangeName(char16_t* name) { Rec(0x10193844, this, name); }
char16_t* UIStatusPanel::GetName() { Rec(0x1019387c, this, nullptr); return reinterpret_cast<char16_t*>(static_cast<uintptr_t>(0x1019387c)); }
void UIStatusPanel::ShowEditNamePanel() { Rec(0x10194208, this, nullptr); }
void UIStatusPanel::ApplyChanges() { Rec(0x10194674, this, nullptr); }
void RideStatusPanel::InitWindow() { Rec(0x101ba99c, this, nullptr); }
void RideStatusPanel::UpdateWindow() { Rec(0x101baaac, this, nullptr); }
void RideStatusPanel::DeInitWindow() { Rec(0x101baa74, this, nullptr); }
void RideStatusPanel::InitStatusWindow() { Rec(0x101b9340, this, nullptr); }
void RideStatusPanel::InitViewWindow() { Rec(0x101b90e4, this, nullptr); }
void RideStatusPanel::UpdateStatusWindow() { Rec(0x101b9ac4, this, nullptr); }
void RideStatusPanel::UpdateViewWindow() { Rec(0x101b9264, this, nullptr); }
void RideStatusPanel::DeInitStatusWindow() { Rec(0x101b9a84, this, nullptr); }
void RideStatusPanel::DeInitViewWindow() { Rec(0x101b9208, this, nullptr); }
void RideStatusPanel::Show(CThing* thing) { Rec(0x101bc0bc, this, thing); }
void RideStatusPanel::Remove() { Rec(0x101bc1a0, this, nullptr); }
void RideStatusPanel::ApplyChanges() { Rec(0x101bbacc, this, nullptr); }
void ShopStatusPanel::InitWindow() { Rec(0x101bd28c, this, nullptr); }
void ShopStatusPanel::UpdateWindow() { Rec(0x101bd730, this, nullptr); }
void ShopStatusPanel::DeInitWindow() { Rec(0x101bd6f8, this, nullptr); }
void ShopStatusPanel::InitStatusWindow() { Rec(0x101bc6c4, this, nullptr); }
void ShopStatusPanel::InitViewWindow() { Rec(0x101bc468, this, nullptr); }
void ShopStatusPanel::UpdateStatusWindow() { Rec(0x101bcea0, this, nullptr); }
void ShopStatusPanel::UpdateViewWindow() { Rec(0x101bc5e8, this, nullptr); }
void ShopStatusPanel::DeInitStatusWindow() { Rec(0x101bce60, this, nullptr); }
void ShopStatusPanel::DeInitViewWindow() { Rec(0x101bc58c, this, nullptr); }
void ShopStatusPanel::Show(CThing* thing) { Rec(0x101bdee0, this, thing); }
void ShopStatusPanel::Remove() { Rec(0x101bdf64, this, nullptr); }
void ShopStatusPanel::ApplyChanges() { Rec(0x101bd9bc, this, nullptr); }
void ShowStatusPanel::InitWindow() { Rec(0x101c1c90, this, nullptr); }
void ShowStatusPanel::UpdateWindow() { Rec(0x101c21c4, this, nullptr); }
void ShowStatusPanel::DeInitWindow() { Rec(0x101c218c, this, nullptr); }
void ShowStatusPanel::InitStatusWindow() { Rec(0x101c0f80, this, nullptr); }
void ShowStatusPanel::InitViewWindow() { Rec(0x101c0d24, this, nullptr); }
void ShowStatusPanel::UpdateStatusWindow() { Rec(0x101c1848, this, nullptr); }
void ShowStatusPanel::UpdateViewWindow() { Rec(0x101c0ea4, this, nullptr); }
void ShowStatusPanel::DeInitStatusWindow() { Rec(0x101c1808, this, nullptr); }
void ShowStatusPanel::DeInitViewWindow() { Rec(0x101c0e48, this, nullptr); }
void ShowStatusPanel::Show(CThing* thing) { Rec(0x101c2a38, this, thing); }
void ShowStatusPanel::Remove() { Rec(0x101c2abc, this, nullptr); }
void ShowStatusPanel::ApplyChanges() { Rec(0x101c2508, this, nullptr); }
void StaffStatusPanel::InitWindow() { Rec(0x101c4c34, this, nullptr); }
void StaffStatusPanel::UpdateWindow() { Rec(0x101c4d2c, this, nullptr); }
void StaffStatusPanel::DeInitWindow() { Rec(0x101c4cf0, this, nullptr); }
void StaffStatusPanel::InitStatusWindow() { Rec(0x101c4304, this, nullptr); }
void StaffStatusPanel::InitViewWindow() { Rec(0x101c41a0, this, nullptr); }
void StaffStatusPanel::UpdateStatusWindow() { Rec(0x101c4990, this, nullptr); }
void StaffStatusPanel::UpdateViewWindow() { Rec(0x101c423c, this, nullptr); }
void StaffStatusPanel::DeInitStatusWindow() { Rec(0x101c4950, this, nullptr); }
void StaffStatusPanel::DeInitViewWindow() { Rec(0x101c41dc, this, nullptr); }
void StaffStatusPanel::Show(CThing* thing) { Rec(0x101c54f0, this, thing); }
void StaffStatusPanel::Remove() { Rec(0x101c5668, this, nullptr); }
void StaffStatusPanel::ChangeName(char16_t* name) { Rec(0x101c558c, this, name); }
const char16_t* StaffStatusPanel::GetName() { Rec(0x101c55f8, this, nullptr); return reinterpret_cast<char16_t*>(static_cast<uintptr_t>(0x101c55f8)); }
void StaffStatusPanel::ShowEditNamePanel() { Rec(0x101c5878, this, nullptr); }
void StaffRoomStatusPanel::InitWindow() { Rec(0x101c36f0, this, nullptr); }
void StaffRoomStatusPanel::UpdateWindow() { Rec(0x101c39b8, this, nullptr); }
void StaffRoomStatusPanel::DeInitWindow() { Rec(0x101c3978, this, nullptr); }
void StaffRoomStatusPanel::InitStatusWindow() { Rec(0x101c2e88, this, nullptr); }
void StaffRoomStatusPanel::InitViewWindow() { Rec(0x101c2d60, this, nullptr); }
void StaffRoomStatusPanel::UpdateStatusWindow() { Rec(0x101c3054, this, nullptr); }
void StaffRoomStatusPanel::UpdateViewWindow() { Rec(0x101c2e44, this, nullptr); }
void StaffRoomStatusPanel::DeInitStatusWindow() { Rec(0x101c3010, this, nullptr); }
void StaffRoomStatusPanel::DeInitViewWindow() { Rec(0x101c2db4, this, nullptr); }
void StaffRoomStatusPanel::Show(CThing* thing) { Rec(0x101c4014, this, thing); }
void StaffRoomStatusPanel::Remove() { Rec(0x101c4098, this, nullptr); }
void UpgradeStatusPanel::InitWindow() { Rec(0x101c6208, this, nullptr); }
void UpgradeStatusPanel::UpdateWindow() { Rec(0x101c6304, this, nullptr); }
void UpgradeStatusPanel::DeInitWindow() { Rec(0x101c62c8, this, nullptr); }
void UpgradeStatusPanel::InitStatusWindow() { Rec(0x101c5e00, this, nullptr); }
void UpgradeStatusPanel::InitViewWindow() { Rec(0x101c5c80, this, nullptr); }
void UpgradeStatusPanel::UpdateStatusWindow() { Rec(0x101c60d4, this, nullptr); }
void UpgradeStatusPanel::UpdateViewWindow() { Rec(0x101c5d20, this, nullptr); }
void UpgradeStatusPanel::DeInitStatusWindow() { Rec(0x101c6090, this, nullptr); }
void UpgradeStatusPanel::DeInitViewWindow() { Rec(0x101c5cc0, this, nullptr); }
void UpgradeStatusPanel::Show(CThing* thing) { Rec(0x101c665c, this, thing); }
void UpgradeStatusPanel::Remove() { Rec(0x101c66e0, this, nullptr); }
void FeatureStatusPanel::InitWindow() { Rec(0x1019e85c, this, nullptr); }
void FeatureStatusPanel::UpdateWindow() { Rec(0x1019e958, this, nullptr); }
void FeatureStatusPanel::DeInitWindow() { Rec(0x1019e91c, this, nullptr); }
void FeatureStatusPanel::InitStatusWindow() { Rec(0x1019e454, this, nullptr); }
void FeatureStatusPanel::InitViewWindow() { Rec(0x1019e2d4, this, nullptr); }
void FeatureStatusPanel::UpdateStatusWindow() { Rec(0x1019e728, this, nullptr); }
void FeatureStatusPanel::UpdateViewWindow() { Rec(0x1019e374, this, nullptr); }
void FeatureStatusPanel::DeInitStatusWindow() { Rec(0x1019e6e4, this, nullptr); }
void FeatureStatusPanel::DeInitViewWindow() { Rec(0x1019e314, this, nullptr); }
void FeatureStatusPanel::Show(CThing* thing) { Rec(0x1019edc4, this, thing); }
void FeatureStatusPanel::Remove() { Rec(0x1019ee48, this, nullptr); }
void FeatureStatusPanel::RideIt() { Rec(0x1019ea60, this, nullptr); }
void BogStatusPanel::InitWindow() { Rec(0x1019a7c8, this, nullptr); }
void BogStatusPanel::UpdateWindow() { Rec(0x1019a8cc, this, nullptr); }
void BogStatusPanel::DeInitWindow() { Rec(0x1019a894, this, nullptr); }
void BogStatusPanel::InitStatusWindow() { Rec(0x1019a12c, this, nullptr); }
void BogStatusPanel::InitViewWindow() { Rec(0x10199ed0, this, nullptr); }
void BogStatusPanel::UpdateStatusWindow() { Rec(0x1019a504, this, nullptr); }
void BogStatusPanel::UpdateViewWindow() { Rec(0x1019a050, this, nullptr); }
void BogStatusPanel::DeInitStatusWindow() { Rec(0x1019a4c4, this, nullptr); }
void BogStatusPanel::DeInitViewWindow() { Rec(0x10199ff4, this, nullptr); }
void BogStatusPanel::Show(CThing* thing) { Rec(0x1019ac5c, this, thing); }
void BogStatusPanel::Remove() { Rec(0x1019ad1c, this, nullptr); }
void BogStatusPanel::ApplyChanges() { Rec(0x1019a978, this, nullptr); }
void VisitorStatusPanel::InitWindow() { Rec(0x10142f44, this, nullptr); }
void VisitorStatusPanel::UpdateWindow() { Rec(0x101430c0, this, nullptr); }
void VisitorStatusPanel::DeInitWindow() { Rec(0x10143084, this, nullptr); }
void VisitorStatusPanel::InitStatusWindow() { Rec(0x10142238, this, nullptr); }
void VisitorStatusPanel::InitViewWindow() { Rec(0x101420d0, this, nullptr); }
void VisitorStatusPanel::UpdateStatusWindow() { Rec(0x10142ae4, this, nullptr); }
void VisitorStatusPanel::UpdateViewWindow() { Rec(0x10142170, this, nullptr); }
void VisitorStatusPanel::DeInitStatusWindow() { Rec(0x10142aa0, this, nullptr); }
void VisitorStatusPanel::DeInitViewWindow() { Rec(0x10142110, this, nullptr); }
void VisitorStatusPanel::Show(CThing* thing) { Rec(0x101433f8, this, thing); }
void VisitorStatusPanel::Remove() { Rec(0x1014356c, this, nullptr); }

// Calls the slot at Mac offset `offset` of table `which` (the order of Table above) with argument `arg`; out[0] is the member reached
// (0 when the table has no slot there), out[1] whether it got the panel, out[2] the argument it got (0 for the members that take
// none), out[3] the answer's low word, out[4] whether a second call of the table's maker answered the same table.
extern "C" void tpw_tbl_call(uint32_t which, uint32_t offset, uint32_t arg, uint32_t* out) {
    uint32_t* table = Table(which);
    out[4] = Table(which) == table;
    UIStatusPanelFields* panel = Panel();
    panel->mVtable.set(table);
    Rec(0, nullptr, nullptr);
    if (table[offset / 4] == 0) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    const void* sent = reinterpret_cast<const void*>(static_cast<uintptr_t>(arg));
    const uint64_t answer = UIStatusPanel_CallSlot(panel, offset, sent);
    out[0] = gReached;
    out[1] = gPanel == panel;
    out[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gArg));
    out[3] = static_cast<uint32_t>(answer);
}
