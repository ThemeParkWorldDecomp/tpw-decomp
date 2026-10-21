// Test library for the staff costs panel's cost words and the two panel message functions (unit v5152; libtpw_unit_UpdateStaffCost.dylib,
// tests/test_UpdateStaffCost.py), main app:
//   src/CreateStaffCostsPanel_UpdateStaffCost.cpp   UpdateStaffCost, StaffCostsPanelMsg, ShopStatusMsg
//
// The callees outside the unit are recording fakes; the globals of other files the unit reads are defined here (zero at start like the
// Mac's bss). The windows are fake objects (a vtable pointer first, every slot answering SetValue's fake).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "fake_status_panel_table.h"

using namespace calllog;

// ---- globals of other files
InterfaceWindow* gStaffCostsPanel;
int32_t gStaffCostsDeposits;
int32_t gStaffCostsWord14;
CThing* gStaffRoomPanelThing;
ShopStatusPanel gShopStatusPanel;
CWorld* gGameWorld;

namespace {

struct Obj {
    void* vptr;
    uint8_t pad[0x400];
};
// The objects are a block of the arena: the shop panel keeps its window in a Ptr32 (unit v5L31).
Obj (&Objs())[16] {
    static auto* objs = static_cast<Obj(*)[16]>(tpw::ArenaAllocZeroed(sizeof(Obj[16])));
    return *objs;
}
// 1 panel, 2 summary, 3 w2df, 4 w2e4, 5 w0xc07f, 6 shop window, 7 thing, 8 callback window, 9 world
alignas(16) uint8_t gHq[sizeof(CStaffHQ) + 64];   // 10
void* gVtable[64];

int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}

// The shop panel's table: the slot at +0x1c is logged as the Mac's stub logs it.
uint64_t Slot(void*, uint32_t offset, const void*) {
    if (offset == 0x1c) ev("Slot5", {});
    return 0;
}

CTrainingCentre& Block() { return reinterpret_cast<CStaffHQ*>(gHq)->mTraining; }

InterfaceWindow* ObjPtr(int idx) { return idx == 0 ? nullptr : reinterpret_cast<InterfaceWindow*>(&Objs()[idx]); }

int IdxOf(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 1; i < 10; ++i)
        if (p == &Objs()[i]) return i;
    return -1;
}

}  // namespace

// ---- the fakes
CStaffHQ* CWorld::GetStaffHQ() {
    ev("GetStaffHQ", {lab(this)});
    return reinterpret_cast<CStaffHQ*>(gHq);
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    switch (id) {
        case 0x2de: return ret("GetWindow", 0x2de, 0) != 0 ? ObjPtr(2) : nullptr;
        case 0x2df: return ObjPtr(3);
        case 0x2e4: return ObjPtr(4);
        case 0xc07f: return ObjPtr(5);
        default: return nullptr;
    }
}

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}

void RemoveStaffCostsPanel() { ev("RemoveStaffCostsPanel", {}); }
void CreateFinancePanel() { ev("CreateFinancePanel", {}); }
void CreateEntryPricePanel() { ev("CreateEntryPricePanel", {}); }
void CreateLoansPanel() { ev("CreateLoansPanel", {}); }
void UIMain_TickRidePreview() { ev("TickRidePreview", {}); }

int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UIMain_DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UIMain_DefaultPanelMsg"));
}

int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultStatusPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultStatusPanelMsg"));
}

void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int64_t>(message)}); }

void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int64_t>(type), lab(thing)}); }

int32_t _applytoall(CThing*) { return 0; }

int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("UI_IterateThingList", {static_cast<int64_t>(mask), fn == reinterpret_cast<int (*)(CThing*)>(&_applytoall) ? 0x10267e20 : 0});
    return 0;
}

void ShopStatusPanel::Remove() { ev("ShopRemove", {lab(this)}); }
void ShopStatusPanel::UpdateStatusWindow() { ev("UpdateStatusWindow", {lab(this)}); }
void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
void UIStatusPanel::EraseObject() { ev("EraseObject", {lab(this)}); }
void UIStatusPanel::Move() { ev("Move", {lab(this)}); }
void UIStatusPanel::CloseObject(int32_t flag) { ev("CloseObject", {lab(this), flag}); }

extern "C" {

uint64_t tpw_sm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gStaffCostsDeposits", &gStaffCostsDeposits},
        {"gStaffCostsWord14", &gStaffCostsWord14},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTrainingCentre, mF14));
    out[n++] = static_cast<int64_t>(sizeof(CTrainingCentre));
    return n;
}

// The host address of the object with that label (1..10, 11 the shop panel); the module's fixed labels.
uint64_t tpw_sm_obj(int32_t idx) {
    if (idx == 10) return reinterpret_cast<uintptr_t>(gHq);
    if (idx == 11) return reinterpret_cast<uintptr_t>(&gShopStatusPanel);
    return reinterpret_cast<uintptr_t>(&Objs()[idx]);
}

// Resets the fakes' world: the vtables, the world pointer, the pointer globals (idx labels; 0 is NULL), the headquarters block and the panel.
void tpw_sm_reset(int32_t panel, int32_t shopWindow, int32_t thing) {
    for (void*& slot : gVtable) slot = reinterpret_cast<void*>(&FakeSetValue);
    for (Obj& o : Objs()) {
        std::memset(o.pad, 0, sizeof(o.pad));
        o.vptr = gVtable + 0;
    }
    std::memset(gHq, 0, sizeof(gHq));
    gGameWorld = reinterpret_cast<CWorld*>(&Objs()[9]);
    gStaffCostsPanel = ObjPtr(panel);
    gStaffRoomPanelThing = reinterpret_cast<CThing*>(ObjPtr(thing));
    std::memset(static_cast<void*>(&gShopStatusPanel), 0, sizeof(gShopStatusPanel));
    gShopStatusPanel.mWindow.set(ObjPtr(shopWindow));
    gShopStatusPanel.mVtable.set(FakeStatusPanelTable<Slot>());
}

void tpw_sm_hq_set(int32_t i, uint32_t v) {
    if (i == 5) Block().mF14 = static_cast<uint8_t>(v);
    else Block().mF00[i] = v;
}
uint32_t tpw_sm_hq_get(int32_t i) { return i == 5 ? Block().mF14 : Block().mF00[i]; }

// Mac offsets 0x24, 0x28, 0x2c of the shop panel.
void tpw_sm_panel_set(int32_t off, int32_t v) {
    if (off == 0x24) gShopStatusPanel.mQuality = v;
    else if (off == 0x28) gShopStatusPanel.mIngredient = v;
    else gShopStatusPanel.mF2c = v;
}
int32_t tpw_sm_panel_get(int32_t off) {
    return off == 0x24 ? gShopStatusPanel.mQuality : off == 0x28 ? gShopStatusPanel.mIngredient : gShopStatusPanel.mF2c;
}
int32_t tpw_sm_get_panel() { return IdxOf(gStaffCostsPanel); }

void tpw_sm_UpdateStaffCost(int32_t id, int32_t value) { UpdateStaffCost(id, value); }
int32_t tpw_sm_StaffCostsPanelMsg(int32_t w, int32_t m, int32_t a, int32_t b) { return StaffCostsPanelMsg(ObjPtr(w), m, a, b); }
int32_t tpw_sm_ShopStatusMsg(int32_t w, int32_t m, int32_t a, int32_t b) { return ShopStatusMsg(ObjPtr(w), m, a, b); }

}  // extern "C"
