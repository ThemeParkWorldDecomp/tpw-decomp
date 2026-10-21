// Test library for the bog status panel's window functions (unit v5301; libtpw_unit_BogStatusPanel_InitViewWindow.dylib,
// tests/test_BogStatusPanel_InitViewWindow.py), main app:
//   src/game/ui/ui_bogStatus.cpp   InitViewWindow, UpdateViewWindow, InitWindow, UpdateWindow, Show
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). The virtual calls of
// the Mac (tables of TVector pointers) are the Ptr32 tables of UIStatusPanelFields with blocks holding the fake's address, and the
// window's SetText is a hand made vtable (slot found through the pointer to member).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CThing* gStaffRoomPanelThing = nullptr;   // 0x103947a4

namespace {

uint64_t gObj[16];   // the objects the fakes answer with, by label (set by the test)

int64_t L(const void* p) { return static_cast<int64_t>(lab(const_cast<void*>(p))); }

uint64_t Slot3c(void* self, uint64_t) {
    ev("Slot", {0x3c, L(self)});
    return static_cast<uint64_t>(ret("slot3c"));
}
uint64_t Slot20(void* self, uint64_t) {
    ev("Slot", {0x20, L(self)});
    return 0;
}
uint64_t Slot1c(void* self, uint64_t) {
    ev("Slot", {0x1c, L(self)});
    return 0;
}
int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {L(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
    return 0;
}

void* gWindowVtable[512];

}  // namespace

// ---- the fakes
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {L(this), z}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {L(this), on});
    return 1;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {L(this), L(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {L(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {L(this), L(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {L(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    ev("SetClientDrawText", {L(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(text))});
}
int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {L(this)});
    rect->mLeft = static_cast<int16_t>(ret("rect", 0, 0));
    rect->mTop = static_cast<int16_t>(ret("rect", 1, 0));
    rect->mRight = static_cast<int16_t>(ret("rect", 2, 0));
    rect->mBottom = static_cast<int16_t>(ret("rect", 3, 0));
    return 1;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {L(this), id});
    return reinterpret_cast<InterfaceWindow*>(gObj[ret("GetWindow", id, 0)]);
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(gObj[6]);
}
void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) {
    ev("ShowRidePreview", {rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, id, z});
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int64_t>(message)}); }
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {L(this)});
    return reinterpret_cast<RideInfo*>(gObj[10]);
}

CoolRect::CoolRect() { ev("CoolRect", {0}); }
CoolRect::~CoolRect() { ev("~CoolRect", {0}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {L(this), L(thing)});
    return static_cast<int32_t>(ret("Fill"));
}
void UIStatusPanel::InitEditBox(int32_t id) { ev("InitEditBox", {L(this), id}); }
void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t a, int32_t b, int32_t c) {
    ev("OpenWindow", {L(this), L(description), L(reinterpret_cast<const void*>(callback)), a, b, c});
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("New", {size});
    if (ret("newnull") != 0) return nullptr;
    return reinterpret_cast<void*>(gObj[7]);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {L(mem)}); }
}  // namespace lender_callee

int32_t BogStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

extern "C" {

uint64_t tpw_bog_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gBogStatusThing", &gBogStatusThing},
        {"gPanelThing", &gStaffRoomPanelThing},
        {"script", gBogStatusScript},
        {"callback", reinterpret_cast<const void*>(&BogStatusMsg)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bog_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of an address (the Ptr32 words of the Mac layout).
uint32_t tpw_bog_offset(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address)));
}

void tpw_bog_set_obj(uint32_t index, uint64_t address) { gObj[index] = address; }

// The address of the fake behind a table slot (0x3c, 0x20, 0x1c).
uint64_t tpw_bog_slotfn(uint32_t slot) {
    using Slot = uint64_t (*)(void*, uint64_t);
    Slot f = slot == 0x3c ? Slot3c : slot == 0x20 ? Slot20 : Slot1c;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(f));
}

// The vtable of a fake window: SetText goes to FakeSetText. The window's first 8 bytes hold the returned address.
uint64_t tpw_bog_window_vptr() {
    auto member = &InterfaceWindow::SetText;
    uintptr_t words[2];
    std::memcpy(words, &member, sizeof(words));
    // The ARM variant of the Itanium ABI: the byte offset in the vtable is the first word, the virtual flag is bit 0 of the second.
    size_t slot = words[0] / sizeof(void*);
    gWindowVtable[slot] = reinterpret_cast<void*>(&FakeSetText);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWindowVtable));
}

int32_t tpw_bog_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF14));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF18));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    alignas(RideInfo) static unsigned char ride[sizeof(RideInfo)];   // the ride record's type number (the Mac's +0x78c)
    out[n++] = reinterpret_cast<const unsigned char*>(&reinterpret_cast<const RideInfo*>(ride)->mRideTypeNumber) - ride;
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    return n;
}

void tpw_bog_init_view_window(uint64_t panel) { reinterpret_cast<BogStatusPanel*>(panel)->InitViewWindow(); }
void tpw_bog_update_view_window(uint64_t panel) { reinterpret_cast<BogStatusPanel*>(panel)->UpdateViewWindow(); }
void tpw_bog_init_window(uint64_t panel) { reinterpret_cast<BogStatusPanel*>(panel)->InitWindow(); }
void tpw_bog_update_window(uint64_t panel) { reinterpret_cast<BogStatusPanel*>(panel)->UpdateWindow(); }
void tpw_bog_show(uint64_t panel, uint64_t thing) {
    reinterpret_cast<BogStatusPanel*>(panel)->Show(reinterpret_cast<CThing*>(thing));
}

}  // extern "C"
