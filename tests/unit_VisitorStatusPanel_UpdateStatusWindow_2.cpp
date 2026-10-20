// Test library for VisitorStatusPanel::UpdateStatusWindow and UpdateViewWindow (unit v51022;
// libtpw_unit_VisitorStatusPanel_UpdateStatusWindow_2.dylib, tests/test_VisitorStatusPanel_UpdateStatusWindow_2.py), main app:
//   src/ActionRecorder_CActionRecorder_CActionRecorder_VisitorStatusMsg_2_VisitorStatusPanel_UpdateStatusWindow_2.cpp
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files, defined here
CThing* gStaffRoomPanelThing;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
char16_t gUIMainConvertBuffer[0x4000];
// gText: the real type needs constructors this library does not compile; the symbol is all the unit's code uses.
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");

static void* gLabelAddr[64];
static int32_t gAnswers[18];

namespace {

void MkText(uint32_t v, char16_t* out) {
    out[0] = static_cast<char16_t>(0x41 + v % 26);
    out[1] = static_cast<char16_t>(0x61 + (v >> 5) % 26);
    out[2] = static_cast<char16_t>(0x30 + (v >> 10) % 10);
    out[3] = 0;
}

void Slot34Fake(void* panel) { ev("Slot34", {lab(panel)}); }
void (*gSlot34)(void*) = &Slot34Fake;

}  // namespace

namespace visitor_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
void WindowSetText(InterfaceWindow* window, const char16_t* text) {
    ev("WindowSetText", {lab(window), lab(const_cast<char16_t*>(text)), text[0], text[1], text[2], text[3]});
}
void RectCtor(void* rect) { ev("RectCtor", {lab(rect)}); }
void RectDtor(void* rect) { ev("RectDtor", {lab(rect), -1}); }
}  // namespace visitor_callee

// ---- the tags' vtables are emitted here (their formatters are never called by the unit)
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// ---- CoolRect: only the weak default of RectCtor / RectDtor in the source refers to it (the test overrides both); empty bodies keep the link whole
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    int32_t k = id - 0x3724;
    if (k < 0 || k >= 18 || gAnswers[k] == 0) return nullptr;
    return static_cast<InterfaceWindow*>(gLabelAddr[gAnswers[k]]);
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {lab(this), frame}); }

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this), lab(rect)});
    return 0;
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    if (id == 0x1c8) {
        va_list args;
        va_start(args, id);
        const ShowStatusTag* tag = va_arg(args, const ShowStatusTag*);
        va_end(args);
        ev("FormatString", {lab(const_cast<TbITable*>(this)), lab(out), size, id, tag->mMagic, tag->mId, tag->mValue, tag->mFlag});
    } else if (id == 0x10) {
        va_list args;
        va_start(args, id);
        const TbUniStringVTag* tag = va_arg(args, const TbUniStringVTag*);
        va_end(args);
        ev("FormatString", {lab(const_cast<TbITable*>(this)), lab(out), size, id, tag->mMagic, tag->mId, tag->mText[0], tag->mText[1],
                            tag->mText[2], tag->mText[3]});
    } else {
        ev("FormatString", {lab(const_cast<TbITable*>(this)), lab(out), size, id});
    }
    MkText(static_cast<uint32_t>(ret("FormatString")), out);
    return 0;
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(const_cast<TbITable*>(this)), id});
    return static_cast<const unsigned char*>(gLabelAddr[10]);
}

void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {lab(window), id, lab(text), text[0], text[1], text[2], text[3]});
}

char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("ConvertMulti", {lab(const_cast<unsigned char*>(text)), text[0], text[1], text[2], text[3]});
    return static_cast<char16_t*>(gLabelAddr[11]);
}

void UIMain_ShowSpritePreview(CoolRect& rect, CPeep* peep, int32_t z, float scale) {
    ev("ShowSpritePreview", {lab(&rect), lab(peep), z, fbits(scale)});
}

uint32_t CPerson::GetTimeInPark() const {
    ev("GetTimeInPark", {lab(const_cast<CPerson*>(this))});
    return static_cast<uint32_t>(ret("TimeInPark"));
}

TbTimeDiff CParkTime::GetFunnyTimeForGameTurns(uint32_t turns) const {
    ev("GetFunnyTime", {lab(const_cast<CParkTime*>(this)), turns});
    TbTimeDiff diff;
    diff.mTicks = static_cast<int64_t>(ret("Funny"));
    return diff;
}

void CObject::GetUIName(char16_t* out, uint32_t size) {
    ev("GetUIName", {lab(this), size});
    MkText(static_cast<uint32_t>(ret("UIName")), out);
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    uint32_t sum = 0;
    for (uint32_t i = 0; format[i] != 0; ++i) sum += (i + 1) * static_cast<uint8_t>(format[i]);
    ev("GAME_ASSERTMSG", {condition, sum});
}

extern "C" {

uint64_t tpw_vsp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing", &gStaffRoomPanelThing},
        {"world", &gGameWorld},
        {"tarr", &gThingArray[0]},
        {"buf", &gUIMainConvertBuffer[0]},
        {"res6", &gText.mResourcePointers[6]},
        {"res8", &gText.mResourcePointers[8]},
        {"res9", &gText.mResourcePointers[9]},
        {"labaddr", &gLabelAddr[0]},
        {"answers", &gAnswers[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_vsp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_vsp_off(uint64_t address) {
    return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
}

uint64_t tpw_vsp_fn34(){ return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gSlot34)); }

int32_t tpw_vsp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(kThingArraySize);
    return n;
}

void tpw_vsp_status(uint64_t panel) { reinterpret_cast<VisitorStatusPanel*>(panel)->UpdateStatusWindow(); }
void tpw_vsp_view(uint64_t panel) { reinterpret_cast<VisitorStatusPanel*>(panel)->UpdateViewWindow(); }

}  // extern "C"
