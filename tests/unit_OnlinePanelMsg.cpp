// Test library for the online panel's message function (unit v5079; libtpw_unit_OnlinePanelMsg.dylib, tests/test_OnlinePanelMsg.py), main app:
//   src/game/ui/ui_onlinePanel.cpp   OnlinePanelMsg and the globals nobody else names (gOnline*)
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queues; the globals of
// other files the unit reads, defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_Postcard.h"
#include "ui_main.h"
#include "ui_tooltip.h"

using namespace calllog;

static_assert(sizeof(CObject) <= 0x2000, "the test allocates 0x2000 bytes for each ride");

// ---- globals of other files the unit reads or writes, defined here
CIntrface gIntrface;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
CKeyTable* gKeyTableCamera;
CKeyTable* gKeyTableShortcuts;
// gText as raw zeroed storage under the symbol the header's extern names (its constructor would drag in the font classes).
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");
char16_t gUIMainConvertBuffer[0x4000];
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineWindow;
CThingArrayEntry gThingArray[kThingArraySize];
InterfaceWindow* gUIKeyboardWindow;

namespace {

// The unit of text the fakes record: the character at index i, 0 from the first terminator on.
int64_t WC(const char16_t* p, int i) {
    if (p == nullptr) return 0;
    for (int k = 0; k <= i; ++k)
        if (p[k] == 0) return 0;
    return p[i];
}

struct FakeMode : CMouseMode {
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode"));
    }
};

const char16_t kName[] = {'R', 'i', 'd', 'e', 0};
uintptr_t gKid = 0;

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
void MoveHelpText(int16_t x, int16_t y) { ev("MoveHelpText", {x, y}); }

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}

int32_t InterfaceWindow::CaptureDevice() {
    ev("CaptureDevice", {lab(this)});
    return 0;
}

int32_t InterfaceWindow::UnCaptureDevice() {
    ev("UnCaptureDevice", {lab(this)});
    return 0;
}

void InterfaceWindow::UnCaptureKb(int32_t on) { ev("UnCaptureKb", {lab(this), on}); }

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }

int32_t InterfaceWindow::GetPosition(CoolRect*, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(gKid);
}

uint32_t InterfaceWindow::GetAttr() {
    ev("GetAttr", {lab(this)});
    return static_cast<uint32_t>(ret("GetAttr"));
}

void InterfaceWindow::setCursor(int16_t cursor) { ev("setCursor", {lab(this), cursor}); }

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}

void Will_HandleCameraMessages(int32_t message, int32_t a, int32_t b) { ev("Will_HandleCameraMessages", {message, a, b}); }

void UpdateAvatarPos() { ev("UpdateAvatarPos", {}); }

void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    ev("LLCR_OverRide", {x, y});
    *out = static_cast<uint16_t>(ret("LLCR"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t hash = 0;
    for (const char* p = format; *p != 0; ++p) hash = (hash * 31 + static_cast<unsigned char>(*p)) & 0x7fffffff;
    ev("GAME_ASSERTMSG", {condition & 0xff, hash});
}

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    ev("UpgradesWhat", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(ret("Upgrades"));
    return tag;
}

RideInfo* CObject::GetDBInfo() const {
    static RideInfo* info = nullptr;
    if (info == nullptr) info = static_cast<RideInfo*>(tpw::ArenaAllocZeroed(sizeof(RideInfo)));
    ev("GetDBInfo", {lab(this)});
    info->mUIType = static_cast<int32_t>(ret("UIType"));
    return info;
}

void CObject::GoFirstPerson(int32_t on) const { ev("GoFirstPerson", {lab(this), on}); }

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }

void CMouse::DoubleClick(int32_t x, int32_t y) { ev("DoubleClick", {lab(this), x, y}); }

void CMouse::ProcessIngameMouse(int32_t key, int32_t down, int32_t x, int32_t y, void* data) {
    ev("ProcessIngameMouse", {key, down, x, y, static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

CMMOnline::CMMOnline() { ev("CMMOnline", {newlab(this)}); }
CMMOnline::~CMMOnline() {}
void CMMOnline::OnLeftClick(int32_t, int32_t) {}
void CMMOnline::OnRightClick(int32_t, int32_t) {}
void CMMOnline::OnDoubleClick(int32_t, int32_t) {}
int32_t CMMOnline::GetMode() { return 7; }
void CMMOnline::OnAssign() {}

CMMFirstPersonTarget::CMMFirstPersonTarget() { ev("CMMFirstPersonTarget", {newlab(this)}); }
CMMFirstPersonTarget::~CMMFirstPersonTarget() {}
void CMMFirstPersonTarget::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CMMFirstPersonTarget::OnLeftClick(int32_t, int32_t) {}
void CMMFirstPersonTarget::OnRightClick(int32_t, int32_t) {}
int32_t CMMFirstPersonTarget::GetMode() { return 9; }
void CMMFirstPersonTarget::OnAssign() {}

void SetHelpTextString(char16_t* text) {
    ev("SetHelpTextString", {WC(text, 0), WC(text, 1), WC(text, 2), WC(text, 3), WC(text, 4), WC(text, 5), WC(text, 6), WC(text, 7)});
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    int64_t magic = 0, tagId = 0;
    const char16_t* text = nullptr;
    if (id == 0x244) {
        va_list ap;
        va_start(ap, id);
        TbUniStringVTag* tag = va_arg(ap, TbUniStringVTag*);
        va_end(ap);
        magic = tag->mMagic != 0 ? 1 : 0;
        tagId = tag->mId;
        text = tag->mText;
    }
    ev("FormatString", {lab(this), out == gUIMainConvertBuffer ? 1 : 0, size, id, magic, tagId, WC(text, 0), WC(text, 1), WC(text, 2), WC(text, 3)});
    out[0] = static_cast<char16_t>(0x41 + (id & 15));
    out[1] = static_cast<char16_t>(0x30 + (magic != 0 ? 1 : 0));
    out[2] = 0;
    return 0;
}


int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

char16_t* UIMain_GetObjectName(CObject* object) {
    ev("UIMain_GetObjectName", {lab(object)});
    return const_cast<char16_t*>(kName);
}

void HighlightRide() { ev("HighlightRide", {}); }
void UnhighlightRide() { ev("UnhighlightRide", {}); }

int16_t CheckShifts() {
    ev("CheckShifts", {});
    return static_cast<int16_t>(ret("CheckShifts"));
}

int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    ev("KeyDown", {lab(this), key, modifiers});
    return 0;
}

int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    ev("KeyUp", {lab(this), key, modifiers});
    return 0;
}

int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("Search"));
}

void CreateMainMenu(int32_t a) { ev("CreateMainMenu", {a}); }

void UI_DoPostcard() { ev("UI_DoPostcard", {}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_op_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gOnlinePanel},
        {"window", &gOnlineWindow},
        {"e8c", &gOnlineWindowE8c},
        {"e90", &gOnlineWindowE90},
        {"target", &gOnlineTargetRide},
        {"state", &gOnlineCursorState},
        {"kwin", &gOnlineWindowKeyHeld},
        {"kpan", &gOnlinePanelKeyHeld},
        {"block", &gOnlineBlockToggle},
        {"shown", &gOnlinePanelShown},
        {"noCell", &gOnlineNoCellId},
        {"upId", &gOnlineRideUpgradeId},
        {"noRide", &gOnlineNoRideId},
        {"intr24", &gIntrface.mF24},
        {"mouse", &gMouse},
        {"mode", &gMouseMode},
        {"w1e0", &gMouseWord1E0},
        {"w1e4", &gMouseWord1E4},
        {"cam", &gKeyTableCamera},
        {"sho", &gKeyTableShortcuts},
        {"text7", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[7]},
        {"width", &gUIRealScreenWidth},
        {"height", &gUIRealScreenHeight},
        {"kbwin", &gUIKeyboardWindow},
        {"buf", &gUIMainConvertBuffer[0]},
        {"thing0", &gThingArray[0].mThing},
        {"thing1", &gThingArray[1].mThing},
        {"thing2", &gThingArray[2].mThing},
        {"thing3", &gThingArray[3].mThing},
        {"thing4", &gThingArray[4].mThing},
        {"thing5", &gThingArray[5].mThing},
        {"thing6", &gThingArray[6].mThing},
        {"thing7", &gThingArray[7].mThing},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_op_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A mouse mode whose GetMode answers from the queue "GetMode".
uint64_t tpw_op_mode() {
    void* block = tpw::ArenaAllocZeroed(sizeof(FakeMode));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (block) FakeMode()));
}

// The offsets the module relies on (HOST_LAYOUT).
int32_t tpw_op_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mUIType));
    return n;
}

void tpw_op_setkid(uint64_t kid) { gKid = static_cast<uintptr_t>(kid); }

int32_t tpw_op_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OnlinePanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
