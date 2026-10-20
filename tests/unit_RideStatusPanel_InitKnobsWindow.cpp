// Test library for the ride status panel's window functions (unit v51011; libtpw_unit_RideStatusPanel_InitKnobsWindow.dylib,
// tests/test_RideStatusPanel_InitKnobsWindow.py), main app:
//   src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade_RideStatusPanel_InitKnobsWindow.cpp
//   InitWindow, UpdateWindow, InitKnobsWindow, UpdateKnobsTitles and gRideStatusMeshNames.
//
// The callees outside the unit are fakes that record what the real ones read, the same event names and arguments as the Python stubs.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
// gText as raw zeroed storage under the symbol of the program's gText (its constructor would drag in the font classes); the unit reads
// mResourcePointers[6] only.
alignas(16) char gTextStorage[sizeof(CText)] asm("_gText");
char16_t gUIMainConvertBuffer[0x4000];
CThing* gStaffRoomPanelThing;
CThing* gRideStatusShowThing;

namespace {

void* gInfo;
void* gNameText;
void* gFont;
void* gWin[14];

class FakeOps : public UIStatusPanelOps {
public:
    void Slot5() override { ev("OpsSlot5", {0}); }
    void Slot6() override { ev("OpsSlot6", {0}); }
};
FakeOps gOps;

std::string Text(const char16_t* text) { return text ? narrow(text) : std::string("(null)"); }

}  // namespace

// ---- the fakes
// The tag's key function (the vtable the tags built here point at); never called by the unit.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(id)});
    int32_t index = id - 0x3e26;
    return index >= 0 && index < 14 ? static_cast<InterfaceWindow*>(gWin[index]) : nullptr;
}

void InterfaceScrollBar::SetMinMax(int32_t min, int32_t max) {
    ev("SetMinMax", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(min), static_cast<int64_t>(max)});
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) {
    ev("SetClientDrawObject", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(object))});
}

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) {
    ev("SetFontJust", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(horizontal), static_cast<int64_t>(vertical)});
}

void InterfaceWindow::SetFont(InterfaceFont* font) {
    ev("SetFont", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(font))});
}

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(red), static_cast<int64_t>(green),
                         static_cast<int64_t>(blue), static_cast<int64_t>(alpha)});
}

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    ev("SetClientDrawText", {static_cast<int64_t>(lab(this))}, {Text(text).c_str()});
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(on)});
    return 0;
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return static_cast<InterfaceFont*>(gFont);
}

char16_t* UIMain_GetObjectName(CObject* object) {
    ev("GetObjectName", {static_cast<int64_t>(lab(object))});
    return static_cast<char16_t*>(gNameText);
}

int32_t UIStatusPanel::FillObjectList(CThing* thing) {
    ev("FillObjectList", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(thing))});
    return static_cast<int32_t>(ret("FillObjectList"));
}

void UIStatusPanel::InitEditBox(int32_t id) {
    ev("InitEditBox", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(id)});
}

void RideStatusPanel::UpdateKnobsWindow() { ev("UpdateKnobsWindow", {static_cast<int64_t>(lab(this))}); }
void RideStatusPanel::UpdateWhichButtons() { ev("UpdateWhichButtons", {static_cast<int64_t>(lab(this))}); }

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {static_cast<int64_t>(lab(this))});
    return static_cast<RideInfo*>(gInfo);
}

// The tag is read as UpdateKnobsTitles builds it; the text written is "F<string id>_<tag id>_<value>_<negative>".
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list arguments;
    va_start(arguments, id);
    RideKnobTag* tag = va_arg(arguments, RideKnobTag*);
    va_end(arguments);
    ev("FormatString", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(out)), static_cast<int64_t>(size),
                        static_cast<int64_t>(id), static_cast<int64_t>(tag->mMagic), static_cast<int64_t>(tag->mId),
                        static_cast<int64_t>(tag->mValue), static_cast<int64_t>(tag->mNegative)});
    char buffer[96];
    std::snprintf(buffer, sizeof buffer, "F%u_%u_%d_%d", id, tag->mId, tag->mValue, tag->mNegative);
    size_t n = std::strlen(buffer);
    for (size_t i = 0; i <= n; ++i) out[i] = static_cast<char16_t>(static_cast<unsigned char>(buffer[i]));
    return 0;
}

namespace parkstatus_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {static_cast<int64_t>(newlab(mem))}); }
void DrawSpriteCtor(void*) {}
void NumberCtor(void*) {}
void GradientCtor(void* mem, const char* meshName) { ev("GradientCtor", {static_cast<int64_t>(newlab(mem))}, {meshName}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) {
    ev("WindowSetValue", {static_cast<int64_t>(lab(window)), static_cast<int64_t>(value)});
}
void WindowSetText(InterfaceWindow* window, const char16_t* text) {
    ev("WindowSetText", {static_cast<int64_t>(lab(window))}, {Text(text).c_str()});
}
}  // namespace parkstatus_callee

namespace kidlist_callee {
void VisitorNumCtor(void*) {}
void RectCtor(void*) { ev("RectCtor", {0}); }
void RectDtor(void*) { ev("RectDtor", {0}); }
}  // namespace kidlist_callee

extern "C" {

uint64_t tpw_rsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sroom", &gStaffRoomPanelThing},
        {"showthing", &gRideStatusShowThing},
        {"text6", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6]},
        {"convbuf", gUIMainConvertBuffer},
        {"ops", &gOps},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The objects the fakes hand out: 0 the ride record, 1 the name text, 2 the font, 10 + k the window of id 0x3e26 + k.
void tpw_rsp_set(int32_t index, uint64_t value) {
    void* pointer = reinterpret_cast<void*>(static_cast<uintptr_t>(value));
    if (index == 0) gInfo = pointer;
    else if (index == 1) gNameText = pointer;
    else if (index == 2) gFont = pointer;
    else if (index >= 10 && index < 24) gWin[index - 10] = pointer;
}

int32_t tpw_rsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mNameWindow));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mOps));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mSpeed));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mCapacity));
    out[n++] = static_cast<int64_t>(offsetof(RideStatusPanel, mDuration));
    out[n++] = static_cast<int64_t>(sizeof(RideStatusPanel));
    RideInfo* info = reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(0x1000));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&info->mBalance.mValues[25]) - 0x1000);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&info->mBalance.mValues[36]) - 0x1000);
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    return n;
}

void tpw_rsp_initknobs(uint64_t panel) { reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->InitKnobsWindow(); }
void tpw_rsp_updatewindow(uint64_t panel) { reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateWindow(); }
void tpw_rsp_initwindow(uint64_t panel) { reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->InitWindow(); }
void tpw_rsp_titles(uint64_t panel, int32_t which) {
    reinterpret_cast<RideStatusPanel*>(static_cast<uintptr_t>(panel))->UpdateKnobsTitles(which);
}

}  // extern "C"
