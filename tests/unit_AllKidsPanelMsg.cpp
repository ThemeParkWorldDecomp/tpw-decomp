// Test library for the all-kids panel (unit v5078; libtpw_unit_AllKidsPanelMsg.dylib, tests/test_AllKidsPanelMsg.py), main app:
//   src/game/ui/ui_allKidsPanel.cpp   FillAllKidsListCallback, AllKids::UpdateListEntries, AllKidsPanelMsg, LCFormat
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "Ptr32.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- the unit's own entry points and globals (src/game/ui/ui_allKidsPanel.cpp)
void RemoveAllKidsPanel();
int32_t FillAllKidsListCallback(CThing* thing);
int32_t AllKidsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
InterfaceWindow* LCFormat();
namespace AllKids {
void UpdateListEntries();
}
extern InterfaceWindow* gAllKidsMsgWindow;
extern InterfaceListControl2* gAllKidsListControl;
extern int32_t gAllKidsListFormat;
extern int32_t gAllKidsFontIndex;
extern int32_t gAllKidsSelectedIndex;

// ---- globals of other files, defined here
CWorld* gGameWorld;
VisitorStatusPanel gVisitorStatusPanel;
int16_t gUIRealScreenHeight;

namespace {
alignas(16) uint8_t gFontBuf[64];
void* gListObj;
void* gPersons[8];

int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// ---- the fakes
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetString"))));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {lab(window), id, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(text)))});
}
void RemoveAllKidsPanel() { ev("RemoveAllKidsPanel", {}); }
void CreateParkStatusPanel() { ev("CreateParkStatusPanel", {}); }
void CreateAllItemsPanel(UIObjType type, CThing* thing) { ev("CreateAllItemsPanel", {static_cast<int32_t>(type), lab(thing)}); }
void CreateAllStaffPanel(StaffType type, CThing* thing) { ev("CreateAllStaffPanel", {static_cast<int32_t>(type), lab(thing)}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisor", {static_cast<int32_t>(message)}); }
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultPanelMsg"));
}
void VisitorStatusPanel::Show(CThing* thing) { ev("Show", {lab(this), lab(thing)}); }
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) { ev("Zoom", {id.mIndex}); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return tag;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") != 0 ? static_cast<InterfaceWindow*>(gListObj) : nullptr;
}
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    if (data) *data = static_cast<int32_t>(tpw::ArenaOffset(gPersons[position & 7]));
    return 1;
}
uint32_t CPerson::GetTimeInPark() const {
    ev("TimeInPark", {lab(this)});
    return static_cast<uint32_t>(ret("TimeInPark"));
}
TbTimeDiff CParkTime::GetFunnyTimeForGameTurns(uint32_t turns) const {
    ev("Funny", {lab(this), turns});
    TbTimeDiff d;
    d.mTicks = static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(ret("FunnyHi"))) << 32) | static_cast<uint32_t>(ret("FunnyLo")));
    return d;
}
int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), item[0].mInt, item[1].mInt, item[2].mInt, item[3].mInt, item[4].mInt, item[5].mInt,
                   lab(Ptr32<CThing>{static_cast<uint32_t>(data)}.get()), position, update});
    return 0;
}
void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    ev("ChangeItem", {lab(this), position, item[0].mInt, item[1].mInt, item[2].mInt, item[3].mInt, item[4].mInt, item[5].mInt, columns, update, unused});
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFont", {index});
    return reinterpret_cast<InterfaceFont*>(gFontBuf);
}
int32_t InterfaceFont::GetHeight() {
    ev("FontCall", {lab(this)});
    return static_cast<int32_t>(ret("FontCall"));
}
// The window constructor fake is a plain function under the constructor's mangled name (a C++ constructor here would need the
// window's whole vtable and its members' constructors).
extern "C" void* FakeWindowCtor(void* self, void* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                int16_t bottom) asm("__ZN15InterfaceWindowC1EPS_iissss");
extern "C" void* FakeWindowCtor(void* self, void* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                int16_t bottom) {
    ev("InterfaceWindow", {newlab(self), lab(parent), attributes, id, left, top, right, bottom});
    return self;   // Apple arm64 constructors return this
}
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}

extern "C" {

uint64_t tpw_akp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},          {"win", &gAllKidsMsgWindow},     {"lst", &gAllKidsListControl}, {"fmt", &gAllKidsListFormat},
        {"fontidx", &gAllKidsFontIndex}, {"sel", &gAllKidsSelectedIndex}, {"scr", &gUIRealScreenHeight}, {"vsp", &gVisitorStatusPanel},
        {"font", gFontBuf},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_akp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The list object the GetWindow fake answers, its item count (written where the host's InterfaceListControl2 keeps it) and the persons GetData answers.
void tpw_akp_setup(uint64_t list, int32_t count, const uint64_t* persons) {
    gListObj = reinterpret_cast<void*>(static_cast<uintptr_t>(list));
    static_cast<InterfaceListControl2*>(static_cast<InterfaceWindow*>(gListObj))->mItemCount = count;
    for (int i = 0; i < 8; ++i) gPersons[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(persons[i]));
}

int32_t tpw_akp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbTimeDiff));
    return n;
}

int32_t tpw_akp_fill(uint64_t thing) { return FillAllKidsListCallback(reinterpret_cast<CThing*>(static_cast<uintptr_t>(thing))); }
void tpw_akp_update() { AllKids::UpdateListEntries(); }
int32_t tpw_akp_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return AllKidsPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
int64_t tpw_akp_lcformat() { return lab(LCFormat()); }

}  // extern "C"
