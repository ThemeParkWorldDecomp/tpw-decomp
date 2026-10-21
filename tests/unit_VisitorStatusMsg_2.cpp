// Test library for the visitor status panel (unit v5929; libtpw_unit_VisitorStatusMsg_2.dylib, tests/test_VisitorStatusMsg_2.py), main app:
//   src/game/ui/ui_visitorStatus.cpp   VisitorStatusPanel::Show, InitWindow, UpdateWindow,
//   VisitorStatusMsg and __sinit_ui_visitorStatus_cpp, and the globals gVisitorStatusThing, gVisitorStatusScript, gVisitorStatusWordEC3C,
//   gVisitorStatusWordEC40 and gVisitorStatusTable.
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs); the globals of other
// files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CThing* gStaffRoomPanelThing = nullptr;
char16_t gUIMainConvertBuffer[0x4000];
int32_t gSoundBanks[8];
VisitorStatusPanel gVisitorStatusPanel;
// gText is only read through one pointer of its table: raw storage under its symbol (no constructor, nothing else of it is linked).
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");

namespace {

uint64_t gChild = 0, gSprite = 0, gFont = 0;

CText* TheText() { return reinterpret_cast<CText*>(gTextStorage); }

template <int N>
void SlotFn(void* self) {
    ev("slot", {N, lab(self)});
}

}  // namespace

// ---- the fakes
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(gChild);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t h, int32_t v) { ev("SetFontJust", {lab(this), h, v}); }
void InterfaceWindow::SetFontWrap(int32_t h, int32_t v) { ev("SetFontWrap", {lab(this), h, v}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("SetFontColour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(gFont);
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    return reinterpret_cast<void*>(gSprite);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {lab(mem)}); }
}  // namespace lender_callee

void VisitorStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void VisitorStatusPanel::Locate() { ev("Locate", {lab(this)}); }
void VisitorStatusPanel::UpdateStatusWindow() { ev("UpdateStatusWindow", {lab(this)}); }

void UIStatusPanel::GetNextObject() { ev("GetNextObject", {lab(this)}); }
void UIStatusPanel::GetPreviousObject() { ev("GetPreviousObject", {lab(this)}); }
int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {lab(this), lab(thing)});
    return static_cast<int32_t>(ret("FillObjectList"));
}
void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t a, int32_t b, int32_t c) {
    ev("OpenWindow", {lab(this), description == gVisitorStatusScript, callback == VisitorStatusMsg, a, b, c});
}
void UIStatusPanel_Construct(UIStatusPanel* self) { ev("UIStatusPanelCtor", {lab(self)}); }

void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int32_t>(message)}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int32_t>(message)}); }
void UIMain_UpdateSpritePreview() { ev("UpdateSpritePreview", {}); }
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {lab(window), id, lab(text)}); }

void CreateAllKidsPanel(CThing* thing) { ev("CreateAllKidsPanel", {lab(thing)}); }
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultStatusPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultStatusPanelMsg"));
}
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}

// The tag's key function (ltms, untranslated): the tag is only built here, the fake FormatString never calls it.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    int64_t magic = 0, tagId = 0, value = 0, flag = 0;
    if (id == 0x1c8) {
        va_list ap;
        va_start(ap, id);
        const ShowStatusTag* tag = va_arg(ap, const ShowStatusTag*);
        va_end(ap);
        magic = tag->mMagic;
        tagId = tag->mId;
        value = tag->mValue;
        flag = tag->mFlag;
    }
    ev("FormatString", {lab(this), lab(out), size, id, magic, tagId, value, flag});
    return 0;
}

// The tables the static initialisers store (src/game/ui/ui_StatusPanel.cpp, unit v5L31): blocks of the arena made on first use; the test
// compares only where they are stored.
uint32_t* VisitorStatusPanelTable() {
    static uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(0x80));
    return table;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_vsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gVisitorStatusPanel},
        {"thing", &gVisitorStatusThing},
        {"ec3c", &gVisitorStatusWordEC3C},
        {"ec40", &gVisitorStatusWordEC40},
        {"staff", &gStaffRoomPanelThing},
        {"buffer", gUIMainConvertBuffer},
        {"bank1", &gSoundBanks[1]},
        {"text6", &TheText()->mResourcePointers[6]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (arena memory).
uint64_t tpw_vsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of an address (what a Ptr32 field holds).
uint32_t tpw_vsp_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(address)); }

// The arena offset of the table the static initialiser made (0 before).
uint32_t tpw_vsp_tableoff() { return gVisitorStatusTable != nullptr ? tpw::ArenaOffset(gVisitorStatusTable) : 0; }

// The objects the window fakes hand back.
void tpw_vsp_objs(uint64_t child, uint64_t sprite, uint64_t font) {
    gChild = child;
    gSprite = sprite;
    gFont = font;
}

// The function a table entry's block holds on the host, by the Mac's slot offset.
uint64_t tpw_vsp_slotfn(uint32_t slot) {
    void (*fn)(void*) = nullptr;
    switch (slot) {
        case 0x1c: fn = SlotFn<0x1c>; break;
        case 0x20: fn = SlotFn<0x20>; break;
        case 0x34: fn = SlotFn<0x34>; break;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_vsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(UIStatusPanelFields, mVtable));
    out[n++] = static_cast<int64_t>(sizeof(VisitorStatusPanel) >= 0x28 ? 1 : 0);
    return n;
}

// ---- the functions
int32_t tpw_vsp_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return VisitorStatusMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
void tpw_vsp_show(uint64_t thing) { gVisitorStatusPanel.Show(reinterpret_cast<CThing*>(thing)); }
void tpw_vsp_init() { gVisitorStatusPanel.InitWindow(); }
void tpw_vsp_update() { gVisitorStatusPanel.UpdateWindow(); }
void tpw_vsp_sinit() { __sinit_ui_visitorStatus_cpp(); }

}  // extern "C"
