// Test library for the main panel date change, the last panel memory, the loans panel refresh and the slide-out panel's message function
// (unit v5995; libtpw_unit_UI_MainPanelDateChange_2.dylib, tests/test_UI_MainPanelDateChange_2.py), main app:
//   src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_mainpanel2.cpp   UI_MainPanelDateChange, SetLastPanel, UI_LoansPanelDateChange, RailsCallback
//
// Every callee is a fake that records the arguments the real one reads and answers from the test's queues; the globals of other files
// the unit reads are defined here (zero at start like the Mac's bss).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "LoansPanelMsg.h"
#include "TPSound.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gMainPanel;
InterfaceWindow* gGameWindow;
InterfaceWindow* gSlideoutTab;
InterfaceWindow* gLoansPanel;
int32_t gLenderListMode;
int32_t gLastPanelLists;
int32_t gLastPanelPark;
int32_t gLastPanelFinance;
int32_t gSoundBanks[8];
// gText's own constructor needs the font code; the test only reads one word of it, so its storage is defined by hand under the same symbol.
alignas(16) uint8_t gTextStorage[sizeof(CText)] __asm__("_gText");
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;

namespace {

int64_t I(uint32_t v) { return static_cast<int64_t>(v); }
int64_t I(int32_t v) { return static_cast<int64_t>(v); }
int64_t I(int16_t v) { return static_cast<int64_t>(v); }
int64_t P(const void* p) { return static_cast<int64_t>(lab(p)); }   // an object of the case, as its label
int64_t R(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }   // a plain address word

void* gWinTable[16];

void* WinByLabel(int64_t label) {
    if (label < 0 || label >= 16) return nullptr;
    return gWinTable[label];
}

int64_t RetKey(const char* base, int32_t arg) {
    char key[64];
    std::snprintf(key, sizeof key, "%s:%d", base, arg);
    return ret(key);
}

char16_t gConvBuffer[8];

}  // namespace

// The vtable of the integer tag (key function), as the program's own file would emit it.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {P(this), I(id)});
    return static_cast<InterfaceWindow*>(WinByLabel(RetKey("GetWindow", id)));
}

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    ev("SetClientDrawText", {P(this), text == nullptr ? -1 : I(static_cast<uint32_t>(text[0]))});
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {P(this), I(frame)}); }

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {P(this), I(index), index == 1 ? P(value) : I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)))});
    return 0;
}

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {P(this), I(index)});
    int64_t value = RetKey("getExtraParam", index);
    if (index == 1) return WinByLabel(value);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(value)));
}

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {P(this), I(id), I(ms)});
    return static_cast<int16_t>(ret("setAnimTimer"));
}

int32_t InterfaceWindow::linkChecked(InterfaceWindow* child, char a, char b) {
    ev("link", {P(this), P(child), I(static_cast<int32_t>(a)), I(static_cast<int32_t>(b))});
    return static_cast<int32_t>(ret("link"));
}

void InterfaceWindow::setZ(int16_t z) { ev("setZ", {P(this), I(z)}); }

static void FillRect(CoolRect* rect) {
    rect->mLeft = static_cast<int16_t>(ret("pos"));
    rect->mTop = static_cast<int16_t>(ret("pos"));
    rect->mRight = static_cast<int16_t>(ret("pos"));
    rect->mBottom = static_cast<int16_t>(ret("pos"));
}

int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {P(this), I(subtract)});
    FillRect(rect);
    return 0;
}

int32_t InterfaceWindow::SetPosition(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    ev("SetPosition4", {P(this), I(left), I(top), I(right), I(bottom)});
    return 0;
}

int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    ev("SetPositionR", {P(this), I(rect->mLeft), I(rect->mTop), I(rect->mRight), I(rect->mBottom)});
    return 0;
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {P(this), I(on)});
    return 0;
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {P(this), I(which)});
    return 0;
}

int32_t InterfaceWindow::stopAnimTimer(int32_t id) { ev("stopAnimTimer", {P(this), I(id)}); return 0; }  // int32_t since v51111

int32_t InterfaceWindow::GetOriginalPosition(CoolRect* rect) {
    ev("GetOriginalPosition", {P(this)});
    FillRect(rect);
    return 0;
}

int32_t InterfaceWindow::GetPreviousPosition(CoolRect* rect) {
    ev("GetPreviousPosition", {P(this)});
    FillRect(rect);
    return 0;
}

void InterfaceWindow::unlink() { ev("unlink", {P(this)}); }

InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {P(this)});
    return static_cast<InterfaceWindow*>(WinByLabel(ret("GetParent")));
}

int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    ev("ShiftPosition", {P(this), I(dx), I(dy)});
    return 0;
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    int64_t shown = I(a);
    for (int i = 1; i < 16; i++)
        if (gWinTable[i] && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gWinTable[i])) == static_cast<uint32_t>(a)) shown = -1000 - i;
    ev("CallDefaultWindowHandler", {P(this), I(message), shown, I(b)});
    return static_cast<int32_t>(ret("default"));
}

void CoolRect::GetDims(int16_t* width, int16_t* height) {
    ev("GetDims", {I(mLeft), I(mTop), I(mRight), I(mBottom)});
    *width = static_cast<int16_t>(ret("dims"));
    *height = static_cast<int16_t>(ret("dims"));
}

void HideIfUnder(InterfaceWindow* window, int16_t limit) { ev("HideIfUnder", {P(window), I(limit)}); }

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {I(entity), I(bank), I(event), I(a), I(b), I(c)});
    return 0;
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {P(window), I(message), I(a), I(b)});
    return 0;
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {P(window), I(message), I(a), I(b)});
    return 0;
}

void UIMain_UISound(int32_t code) { ev("UIMain_UISound", {I(code)}); }

void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {I(which)}); }

void FillLenderList(InterfaceListControl2* list) { ev("FillLenderList", {P(list)}); }

char16_t* UIMain_ConvertMulti(unsigned char* text) {   // 0x10180334, the overload the Mac calls (the PPC side's fake)
    const char16_t* chars = reinterpret_cast<const char16_t*>(text);
    ev("ConvertMulti", {I(static_cast<uint32_t>(chars[0])), I(static_cast<uint32_t>(chars[1]))});
    gConvBuffer[0] = static_cast<char16_t>(ret("conv"));
    gConvBuffer[1] = 0;
    return gConvBuffer;
}

// The unsigned char overload, the one UI_MainPanelDateChange calls (as the Mac's glue 0x10239f70 does); the two values are written as
// two halfwords, as the PPC side's fake writes them and the ConvertMulti fake above reads them.
int32_t TbITable::FormatString(unsigned char* buffer, uint32_t size, uint32_t id, ...) const {
    char16_t* out = reinterpret_cast<char16_t*>(buffer);
    va_list args;
    va_start(args, id);
    ShowStatusTag* tags[3];
    for (int i = 0; i < 3; i++) tags[i] = va_arg(args, ShowStatusTag*);
    va_end(args);
    // an event holds 12 values: the magic is folded into the flag (0x100 when it is not the expected one)
    auto flag = [](ShowStatusTag* t) { return I(static_cast<int32_t>(static_cast<uint32_t>(t->mFlag) ^ (static_cast<uint32_t>(t->mMagic) != 0x5654414Du ? 0x100u : 0u))); };
    ev("FormatString", {R(this), I(size), I(id), I(tags[0]->mId), I(tags[0]->mValue), flag(tags[0]), I(tags[1]->mId), I(tags[1]->mValue),
                        flag(tags[1]), I(tags[2]->mId), I(tags[2]->mValue), flag(tags[2])});
    out[0] = static_cast<char16_t>(ret("fmt"));
    out[1] = static_cast<char16_t>(ret("fmt"));
    return 0;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {R(this)});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x30000000));
}

template <>
bool CHistory<int, 144>::GetValue(int32_t& out, int32_t back) const {
    ev("GetValue", {R(this), I(back)});
    out = static_cast<int32_t>(ret("hist"));
    return true;
}

// CoolRect is only named by the program's weak RectCtor / RectDtor, which the fakes below replace; its code is not in this library, so
// empty bodies give the weak versions something to link against.
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() : mLeft(0), mTop(0), mRight(0), mBottom(0) {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

namespace rails_callee {
void RectCtor(void* rect) {
    ev("RectCtor", {});
    std::memset(rect, 0, 16);
}
void RectDtor(void*) { ev("RectDtor", {-1}); }
InterfaceWindow* WindowFromWord(int32_t word) {
    for (int i = 1; i < 16; i++)
        if (gWinTable[i] && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gWinTable[i])) == static_cast<uint32_t>(word))
            return static_cast<InterfaceWindow*>(gWinTable[i]);
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(word)));
}
void WindowEnable(InterfaceWindow* window, int16_t on) { ev("Enable", {P(window), I(on)}); }
}  // namespace rails_callee

extern "C" {

uint64_t tpw_v5995_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMainPanel", &gMainPanel},
        {"gGameWindow", &gGameWindow},
        {"gSlideoutTab", &gSlideoutTab},
        {"gLoansPanel", &gLoansPanel},
        {"gLenderListMode", &gLenderListMode},
        {"gLastPanelLists", &gLastPanelLists},
        {"gLastPanelPark", &gLastPanelPark},
        {"gLastPanelFinance", &gLastPanelFinance},
        {"bank3", &gSoundBanks[3]},
        {"table", &gText.mResourcePointers[6]},
        {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5995_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_v5995_setwin(int32_t label, uint64_t address) { gWinTable[label & 15] = reinterpret_cast<void*>(static_cast<uintptr_t>(address)); }

int32_t tpw_v5995_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mF130));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mF134));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow) <= 0x200);
    return n;
}

void tpw_v5995_date(int32_t day, int32_t month, int32_t year) { UI_MainPanelDateChange(day, month, year); }
void tpw_v5995_last(int32_t type, int32_t panel) { SetLastPanel(type, panel); }
void tpw_v5995_loans(int32_t month) { UI_LoansPanelDateChange(month); }
int32_t tpw_v5995_rails(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return RailsCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
