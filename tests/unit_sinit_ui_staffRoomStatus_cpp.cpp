// Test library for the staff room / show status functions (unit v5951; libtpw_unit_sinit_ui_staffRoomStatus_cpp.dylib,
// tests/test_sinit_ui_staffRoomStatus_cpp.py), main app:
//   src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_sinit_ui_staffRoomStatus_cpp.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of other files
// the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
CThing* gStaffRoomPanelThing;
char16_t gStaffEditName[0x20];
alignas(64) ShowStatusPanel gShowStatusPanel;
alignas(64) uint8_t gShowStatusPad[0x80];
StaffRoomStatusPanel gStaffRoomStatusPanel;
StaffStatusPanel gStaffStatusPanel;
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> gStaffRoomId;
InterfaceListControl2* gStaffRoomList;
int16_t gUIRealScreenHeight;
uint32_t gLCFontIndex;

static void* P(uint64_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }

// The objects the fakes answer with, set by the test per case: built window, title child, edit child, font, draw object, title text, name.
enum { kBuilt, kTitle, kEdit, kFont, kDraw, kTitleText, kName, kThing, kCount };
static uint64_t gObj[kCount];

// ---- the fakes
void UIStatusPanel_Construct(UIStatusPanel* self) { ev("UIStatusPanel", {lab(self)}); }

void UIStatusPanel::ZoomTo() { ev("ZoomTo", {lab(this)}); }

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}

extern int32_t OneEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
int32_t OneEditMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<void*>(callback))});
    return static_cast<InterfaceWindow*>(P(gObj[kBuilt]));
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {lab(this), index, lab(value)});
    return 0;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(P(id == 0x5daa ? gObj[kTitle] : id == 0x5dab ? gObj[kEdit] : 0));
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {lab(this), lab(text), capacity});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {lab(this), lab(object)}); }
void InterfaceEditBox::SelectAll() { ev("SelectAll", {lab(this)}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(name);
    auto word = [&](int at) { return static_cast<int64_t>((uint32_t(p[at]) << 24) | (uint32_t(p[at + 1]) << 16) | (uint32_t(p[at + 2]) << 8) | p[at + 3]); };
    ev("FindDrawObject", {word(0), word(4)});
    return static_cast<InterfaceDrawObject*>(P(gObj[kDraw]));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFont", {index});
    return static_cast<InterfaceFont*>(P(gObj[kFont]));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(P(gObj[kTitleText]));
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("TitleColour", {lab(window)}); }
void UIMain_DisableAllKeyTables() { ev("DisableAllKeyTables", {}); }

const char16_t* CStaff::GetName() const {
    ev("GetName", {lab(this)});
    return static_cast<const char16_t*>(P(gObj[kName]));
}
int32_t CStaff::IsRestingInStaffRoom(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> room) const {
    ev("IsResting", {lab(this), room.mIndex});
    return static_cast<int32_t>(ret("IsResting"));
}
int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    // the data word is the thing's address cut to 32 bits (a Mac long): match it against the case's thing
    void* thing = P(gObj[kThing]);
    void* dataThing = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(thing)) == static_cast<uint32_t>(data) ? thing : P(static_cast<uint32_t>(data));
    ev("AddItem", {lab(this), lab(const_cast<char16_t*>(item[0].mText)), item[1].mInt, lab(dataThing), position, update});
    return 0;
}

// Only the weak FontHeight of the src file calls it (the fake below replaces that one): never run.
int32_t InterfaceFont::GetHeight() { return 0; }

// Only the weak NewRowWindow of the src file constructs one (the fake below replaces that one): never run.
asm(".text\n.globl __ZN15InterfaceWindowC1EPS_iissss\n__ZN15InterfaceWindowC1EPS_iissss:\n ret\n");

// The two draw objects' constructors are only named by the weak NewTitleText / NewCursorText of the src file (replaced by the fakes below):
// empty symbols stand in, so that the library links without the draw object classes.
asm(".text\n.globl __ZN24InterfaceTitleTextObjectC1EPv\n__ZN24InterfaceTitleTextObjectC1EPv:\n ret\n"
    ".globl __ZN34InterfaceCursorPosTextObjectSpriteC1EPv\n__ZN34InterfaceCursorPosTextObjectSpriteC1EPv:\n ret\n");

namespace staffroom_callee {
InterfaceWindow* NewRowWindow(int16_t bottom) {
    void* p = tpw::ArenaAllocZeroed(0x138);
    ev("WindowCtor", {newlab(p), 0, 0, 0, 0, 0, 100, bottom});
    return static_cast<InterfaceWindow*>(p);
}
int16_t FontHeight(InterfaceFont* font) {
    ev("FontHeight", {lab(font)});
    return static_cast<int16_t>(ret("FontHeight"));
}
InterfaceDrawObject* NewTitleText() {
    void* p = tpw::ArenaAllocZeroed(0x19c);
    ev("TitleCtor", {newlab(p)});
    return static_cast<InterfaceDrawObject*>(p);
}
InterfaceDrawObject* NewCursorText() {
    void* p = tpw::ArenaAllocZeroed(0x19c);
    ev("CursorCtor", {newlab(p)});
    return static_cast<InterfaceDrawObject*>(p);
}
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("SetText", {lab(window), lab(const_cast<char16_t*>(text))}); }
}  // namespace staffroom_callee

extern "C" {

uint64_t tpw_sr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"room", &gStaffRoomStatusPanel},
        {"show", &gShowStatusPanel},
        {"staffpanel", &gStaffStatusPanel},
        {"roomid", &gStaffRoomId},
        {"list", &gStaffRoomList},
        {"screen", &gUIRealScreenHeight},
        {"font", &gLCFontIndex},
        {"fmt", &gStaffRoomListFormat},
        {"valueA", &gStaffRoomValueA},
        {"valueB", &gStaffRoomValueB},
        {"editname", gStaffEditName},
        {"script", gStaffEditNameScript},
        {"callback", reinterpret_cast<const void*>(&OneEditMsg)},
        {"table_room", &gStaffRoomPanelTable},
        {"table_show", &gShowStatusTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint32_t tpw_sr_off(uint64_t address) { return tpw::ArenaOffset(P(address)); }
void tpw_sr_setobjs(const uint64_t* values) {
    for (int i = 0; i < kCount; ++i) gObj[i] = values[i];
}

int32_t tpw_sr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(StaffRoomPanelFields, mScript));
    return n;
}

int64_t tpw_sr_lcformat() { return lab(StaffRoomLCFormat()); }
int32_t tpw_sr_filler(uint64_t thing) { return StaffRoomListFiller(static_cast<CThing*>(P(thing))); }
int32_t tpw_sr_callback(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return StaffStatusViewWindowCallback(static_cast<InterfaceWindow*>(P(window)), m, a, b);
}
void tpw_sr_show(uint64_t panel) { static_cast<StaffStatusPanel*>(P(panel))->ShowEditNamePanel(); }
void tpw_sr_sinit_room() { __sinit_ui_staffRoomStatus_cpp(); }
void tpw_sr_sinit_show() { __sinit_ui_showStatus_cpp(); }

}  // extern "C"
