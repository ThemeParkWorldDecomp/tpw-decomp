// Test library for the feature status panel's message function, its static initialiser and the finance graph's two loops (unit v5904;
// libtpw_unit_FeatureStatusMsg_2.dylib, tests/test_FeatureStatusMsg_2.py), main app:
//   src/FeatureStatusPanel_UpdateStatusWindow_FeatureStatusMsg_2.cpp   FeatureStatusMsg, __sinit_ui_FeatureStatus_cpp, GetMaxYValue, KissMyLines
//
// The callees outside the unit are recording fakes (the same events as the module's PowerPC stubs). The windows are FakeWin blocks:
// a vtable whose SetValue slot records, an id, and the two children GetWindow answers. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
CThing* gStaffRoomPanelThing;
uint32_t gFinanceGraphFlags;
int32_t gFinanceGraphMode;
uint8_t gFinancePanelColours[8][5];
// The panel has the 0x30-byte UIStatusPanelFields layout on the Mac: give the symbol room for it.
struct alignas(16) PanelStorage {
    FeatureStatusPanel panel;
    uint8_t pad[0x50];
};
PanelStorage gPanelStorage __asm__("_gFeatureStatusPanel");

namespace {

const int64_t kAnalyser = 0x30000000;

struct FakeWin {
    void** vptr;
    uint32_t id;
    FakeWin* child1586;
    FakeWin* child1584;
};

int32_t FakeSetValue(FakeWin* window, int32_t value) {
    ev("SetValue", {window->id, value});
    return 0;
}

uint64_t FakeSlot34(void* panel) {
    ev("Slot34", {lab(panel)});
    return 0;
}

void* gFakeVtable[64];
uint64_t (*gSlotBlock)(void*) = FakeSlot34;

uint16_t ObjectId(const void* object) { return *reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(object) + 0xe); }

uint32_t WinId(const void* window) { return static_cast<const FakeWin*>(window)->id; }

int64_t Addr(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

UIStatusPanelFields* Fields(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

}  // namespace

// ---- the fakes
void UIMain_TickRidePreview() { ev("TickRidePreview", {}); }

void FeatureStatusPanel::Remove() { ev("Remove", {lab(this)}); }

void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisor", {static_cast<int32_t>(message)}); }

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {WinId(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {WinId(this), which});
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    const FakeWin* window = reinterpret_cast<const FakeWin*>(this);
    ev("GetWindow", {window->id, id});
    if (id == 0x1586) return reinterpret_cast<InterfaceWindow*>(window->child1586);
    if (id == 0x1584) return reinterpret_cast<InterfaceWindow*>(window->child1584);
    return nullptr;
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {ObjectId(this)});
    return nullptr;
}

uint32_t CObject::GetCurrentResidualValue() const {
    ev("Residual", {ObjectId(this)});
    return static_cast<uint32_t>(ret("Residual"));
}

void CObject::GoFirstPerson(int32_t on) const { ev("GoFirstPerson", {ObjectId(this), on}); }

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {Addr(this)});
    return reinterpret_cast<CParkAnalyser*>(kAnalyser);
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    ev("NumberOfObjectInPark", {id.mIndex});
    return static_cast<int32_t>(ret("NumberOfObjectInPark"));
}

void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), Addr(thing)}); }

int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {WinId(window), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

void UIStatusPanel::GetNextObject() { ev("GetNext", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPrevious", {lab(this)}); }
void UIStatusPanel::EraseObject() { ev("Erase", {lab(this)}); }
void UIStatusPanel::Move() { ev("Move", {lab(this)}); }

// The constructor leaves marker words so that the order of the initialiser's stores shows.
void UIStatusPanel_Construct(UIStatusPanel* self) {
    ev("Construct", {lab(self)});
    Fields(self)->mMode = 0x77;
    Fields(self)->mVtable.mOffset = 0x88;
}

void MaxAnalyserValue(CHistory<int32_t, 144>* history, int32_t& value, int32_t range) {
    ev("Max", {Addr(history), value, range});
    value = static_cast<int32_t>(ret("Max"));
}

void GraphLine(CUIGrapher& grapher, CHistory<int32_t, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    int64_t index = (reinterpret_cast<uint8_t*>(&colour) - &gFinancePanelColours[0][0]) / 5;
    ev("GraphLine", {Addr(&grapher), Addr(history), index, step, mode});
}

extern "C" {

uint64_t tpw_fs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gPanelStorage},
        {"thing", &gStaffRoomPanelThing},
        {"world", &gGameWorld},
        {"flags", &gFinanceGraphFlags},
        {"mode", &gFinanceGraphMode},
        {"colours", gFinancePanelColours},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_fs_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// A fake window: the host's vtable (SetValue is the Itanium slot 8: two destructor slots) and the answers of GetWindow.
uint64_t tpw_fs_mkwin(uint32_t id, uint64_t child1586, uint64_t child1584) {
    gFakeVtable[8] = reinterpret_cast<void*>(FakeSetValue);
    FakeWin* window = static_cast<FakeWin*>(tpw::ArenaAllocZeroed(sizeof(FakeWin)));
    window->vptr = gFakeVtable;
    window->id = id;
    window->child1586 = reinterpret_cast<FakeWin*>(static_cast<uintptr_t>(child1586));
    window->child1584 = reinterpret_cast<FakeWin*>(static_cast<uintptr_t>(child1584));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window));
}

// The panel's table: slot 0x34 holds the arena offset of a block whose first 8 bytes are FakeSlot34. Answers the table's address.
uint64_t tpw_fs_mktable() {
    uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(0x80));
    void* block = tpw::ArenaAllocZeroed(16);
    std::memcpy(block, &gSlotBlock, sizeof(gSlotBlock));
    table[0x34 / 4] = tpw::ArenaOffset(block);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(table));
}

int32_t tpw_fs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF10));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mMode));
    out[n++] = static_cast<int64_t>(sizeof(TbColour));
    return n;
}

int32_t tpw_fs_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return FeatureStatusMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

void tpw_fs_sinit() { __sinit_ui_FeatureStatus_cpp(); }

int32_t tpw_fs_maxy() { return GetMaxYValue(); }

void tpw_fs_kiss(uint64_t grapher) { KissMyLines(*reinterpret_cast<CUIGrapher*>(static_cast<uintptr_t>(grapher))); }

}  // extern "C"
