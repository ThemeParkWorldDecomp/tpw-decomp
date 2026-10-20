// Test library for the all-items panel builder, its entry handler and the staff list functions (unit v5066;
// libtpw_unit_CreateAllItemsPanel.dylib, tests/test_CreateAllItemsPanel.py), main app:
//   src/MainWindowCallback_CreateAllItemsPanel.cpp   CreateAllItemsPanel, AllItemsLCEntryHandler, FillAllStaffListCallback,
//                                                    FillAllStaffList, UpdateListEntries, and the globals gAllItemsScript,
//                                                    gAllItemsNoParent, gStaffListControl
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. Pointers are
// objects of the case (eight blocks the test allocates, labels 1 to 8), which the fakes hand back by their number in the queue
// (tpw_cap_setobjs gives this library the blocks). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "AdvisorScoring.h"
#include "Debug.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_iterators.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gAllItemsPanel = nullptr;
int32_t gAllItemsListType = 0;
InterfaceWindow* gGameWindow = nullptr;
InterfaceWindow* gStaffPanel = nullptr;
int32_t gStaffType = 0;
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont*) {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}   // gText's constructor needs it; the program's one is in another file
CText gText;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

uint64_t gObjs[8];

void* Obj(int64_t k) { return (k >= 1 && k <= 8) ? reinterpret_cast<void*>(static_cast<uintptr_t>(gObjs[k - 1])) : nullptr; }

int64_t Lab(const void* p) { return lab(const_cast<void*>(p)); }

// A list data word (an offset from the arena base) as a label.
int64_t DataLab(int32_t data) { return lab(data != 0 ? reinterpret_cast<void*>(tpw::gArenaBase + static_cast<uint32_t>(data)) : nullptr); }

// The raw word where it names no object of the case (otherwise 0), so a plain number is compared too.
int64_t DataRaw(int32_t data) { return DataLab(data) == -1 ? data : 0; }

}  // namespace

// ---- the callbacks the unit passes on (only their addresses matter)
int32_t MainWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t ListTemplateCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
bool UIMain_CheckPanel() {
    ev("CheckPanel", {0});
    return false;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {Lab(this), description == gAllItemsScript, callback == MainWindowCallback});
    return static_cast<InterfaceWindow*>(Obj(ret("Build")));
}
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {Lab(window)}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {Lab(this), id});
    return static_cast<InterfaceWindow*>(Obj(ret("GetWindow")));
}
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {Lab(this), id});
    return 0;
}
void CreateAllShowsPanel(CThing* thing) { ev("CreateAllShows", {Lab(thing)}); }
void CreateAllShopsPanel(CThing* thing) { ev("CreateAllShops", {Lab(thing)}); }
void CreateAllRidesPanel(CThing* thing) { ev("CreateAllRides", {Lab(thing)}); }
void CreateAllFeaturesPanel(CThing* thing) { ev("CreateAllFeatures", {Lab(thing)}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(Obj(ret("GetString")));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {Lab(window), id, Lab(text)}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {Lab(this), callback == ListTemplateCallback});
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int32_t>(message)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetUpgradeParent() const {
    ev("GetUpgradeParent", {Lab(this)});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(ret("GetUpgradeParent"))};
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("Assert", {condition, std::strcmp(format, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n") == 0});
}
int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    ev("GetObjectState", {Lab(object), flag});
    return static_cast<int32_t>(ret("GetObjectState"));
}
uint32_t UIMain_GetObjectStateTextColour(int32_t state) {
    ev("TextColour", {state});
    return static_cast<uint32_t>(ret("TextColour"));
}
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {Lab(this), red, green, blue, alpha});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {Lab(this), message, a, DataLab(b), DataRaw(b)});
    return static_cast<int32_t>(ret("CallDefault"));
}

int32_t CStaff::GetSkill() const {
    ev("GetSkill", {Lab(this)});
    return static_cast<int32_t>(ret("GetSkill"));
}
int32_t CStaff::GetGeneralStaffState() const {
    ev("GetState", {Lab(this)});
    return static_cast<int32_t>(ret("GetState"));
}
const char16_t* CStaff::GetName() const {
    ev("GetName", {Lab(this)});
    return static_cast<const char16_t*>(Obj(ret("GetName")));
}
int32_t CStaff::HasPatrolArea() const {
    ev("HasPatrolArea", {Lab(this)});
    return static_cast<int32_t>(ret("HasPatrolArea"));
}
uint32_t CStaff::GetSalary() {
    ev("GetSalary", {Lab(this)});
    return static_cast<uint32_t>(ret("GetSalary"));
}
const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {Lab(this), static_cast<int64_t>(id)});
    return static_cast<const unsigned char*>(Obj(ret("GetConstString")));
}
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("ConvertMulti", {Lab(text)});
    return static_cast<char16_t*>(Obj(ret("ConvertMulti")));
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {Lab(this), Lab(item[0].mText), Lab(item[1].mText), item[2].mInt, item[3].mInt, item[4].mInt, DataLab(data), position, update});
    return 0;
}
void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {Lab(this), position, item[0].mInt, Lab(item[1].mText), item[2].mInt, item[3].mInt, item[4].mInt, columns, update, unused});
}
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {Lab(this), position});
    void* object = Obj(ret("GetData"));
    *data = object != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(object)) : 0;
    return 0;
}
void InterfaceListControl2::RemoveAllItems() { ev("RemoveAll", {Lab(this)}); }
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {Lab(this)}); }
int32_t UI_IterateThingList(UIIteratorTypes mask, int (*fn)(CThing*)) {
    ev("Iterate", {static_cast<int32_t>(mask), fn == FillAllStaffListCallback});
    return 0;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cap_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gAllItemsListType", &gAllItemsListType},
        {"gAllItemsPanel", &gAllItemsPanel},
        {"gGameWindow", &gGameWindow},
        {"gStaffPanel", &gStaffPanel},
        {"gStaffType", &gStaffType},
        {"gStaffListControl", &gStaffListControl},
        {"gAllItemsNoParent", &gAllItemsNoParent},
        {"gText10", &gText.mResourcePointers[10]},
        {"gThingArray", &gThingArray[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cap_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A block's list data word: its offset from the arena base.
uint32_t tpw_cap_offset(uint64_t block) { return block != 0 ? static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(block)))) : 0; }

// The case's eight objects (label k is block k - 1).
void tpw_cap_setobjs(const uint64_t* blocks) {
    for (int i = 0; i < 8; ++i) gObjs[i] = blocks[i];
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cap_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<InterfaceListControl2*>(0x1000)->mItemCount) - 0x1000);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<CStaff*>(0x1000)->mHappiness) - 0x1000);
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(UI_LCAdd));
    return n;
}

// ---- the functions
void tpw_cap_create(int32_t type, uint64_t thing) {
    CreateAllItemsPanel(static_cast<UIObjType>(type), reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing)));
}
int32_t tpw_cap_handler(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return AllItemsLCEntryHandler(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
int32_t tpw_cap_callback(uint64_t thing) { return FillAllStaffListCallback(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }
void tpw_cap_fill(int32_t type, int32_t flag) { FillAllStaffList(static_cast<StaffType>(type), flag); }
void tpw_cap_update() { UpdateListEntries(); }

}  // extern "C"
