// Test library for the congratulations and bankrupt windows of the main app (unit v5022; libtpw_unit_UI_Congratulations.dylib,
// tests/test_UI_Congratulations.py), main app:
//   src/ui_main_panel_UI_Congratulations.cpp   UI_Congratulations, UI_Bankrupt, CongratsWindowMsg, __sinit_ui_main_cpp and the
//                                              file's globals (scripts, animations, nodes, the ticks word)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The
// globals of other files the unit reads or writes are defined here, zero at start like the Mac's bss; the ones of complex type
// (the text object, the settings, the light, the cursor table, the strings) are raw storage under the program's symbol names so
// that no static constructor runs.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPSettings.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_main.h"

using namespace calllog;

extern "C" void __register_global_object(void* object, void* destructor, void* node);

// ---- globals of other files, defined here
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gGameWindow;
InterfaceWindow* gFirstPersonWindow;
InterfaceWindow* gUIPanelWindow;
InterfaceWindow* gBankruptWindow;
uint8_t gUIMainKeyTablesDisabled;
CWorld* gGameWorld;
char16_t gUIMainConvertBuffer[0x4000];

alignas(16) uint8_t gTextRaw[sizeof(CText)] asm("_gText");
alignas(16) uint8_t gPlayerSettingsRaw[sizeof(CPlayerSettings)] asm("_gPlayerSettings");
alignas(16) uint8_t gUILightRaw[sizeof(CLight)] asm("_gUILight");
alignas(16) uint8_t gCursorTableRaw[sizeof(CursorTable)] asm("_gCursorTable");
alignas(16) uint8_t gUIFrameNameRaw[sizeof(TbDynamicStringTemplate<char>)] asm("_gUIFrameName");
alignas(16) uint8_t gUISliderNameRaw[sizeof(TbDynamicStringTemplate<char>)] asm("_gUISliderName");
alignas(16) uint8_t gUIPlainFrameNameRaw[sizeof(TbDynamicStringTemplate<char>)] asm("_gUIPlainFrameName");
alignas(16) uint8_t gUIButtonNameRaw[sizeof(TbDynamicStringTemplate<char>)] asm("_gUIButtonName");

// The game type object and its guard: the function-local static of the inline accessor TheGameType() (FS_Online.h), reached through
// its symbol names, so the test sets the Mac's two cells (0x102b3aa0, guard byte 0x102b3aa4).
extern uint8_t gTheGameTypeObject[4] asm("__ZZ11TheGameTypevE9sGameType");
extern uint8_t gTheGameTypeGuard[8] asm("__ZGVZ11TheGameTypevE9sGameType");

// What the fakes of the windows answer with (the test writes the low words).
static InterfaceWindow* gHostCreated;
static InterfaceWindow* gHostBuilt;
static const unsigned char* gHostString;

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

const void* Fn(int32_t (*function)(InterfaceWindow*, int32_t, int32_t, int32_t)) { return reinterpret_cast<const void*>(function); }

}  // namespace

// ---- the string class: the constructor is a fake; the vtable needs every virtual, none is ever run
template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("TbDynamicString", {lab(this)}, {text});
}
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) {
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) {
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) {
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) {
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) {
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) {
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) {
    return *this;
}

// ---- the light and the cursor table
CMapLinkBase::~CMapLinkBase() {}
CLight::CLight() { ev("CLight", {lab(this)}); }
CLight::~CLight() {}
int32_t CLight::Prepare() { return 0; }
int32_t CLight::GetLinkType() { return 1; }
void* CLight::GetData() { return nullptr; }
void CLight::SetColor(float alpha, float red, float green, float blue) {
    ev("SetColor", {lab(this), CanonBits(alpha), CanonBits(red), CanonBits(green), CanonBits(blue)});
}
void CLight::SetNormal(float x, float y, float z) { ev("SetNormal", {lab(this), CanonBits(x), CanonBits(y), CanonBits(z)}); }
CursorTable::CursorTable() { ev("CursorTable", {lab(this)}); }
CursorTable::~CursorTable() {}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

// ---- the rectangle on the stack
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("CoolRect", {lab(this)}); }
CoolRect::~CoolRect() { ev("~CoolRect", {lab(this)}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- the windows
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return gHostCreated;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(Fn(callback))});
    return gHostBuilt;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), lab(Fn(callback))});
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {lab(this), on}); }
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), lab(rect), subtract});
    rect->mRight = static_cast<int16_t>(ret("GetPositionRight"));
    rect->mBottom = static_cast<int16_t>(ret("GetPositionBottom"));
    return static_cast<int32_t>(ret("GetPosition"));
}
void InterfaceWindow::setPivot(int16_t x, int16_t y) { ev("setPivot", {lab(this), x, y}); }
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("startAnim", {lab(this), lab(anim), ticks});
    return static_cast<int32_t>(ret("startAnim"));
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

// The built window's virtual Enable: a fake table in the first word of the block the test hands out.
namespace {
int16_t FakeEnable(void* self, int16_t on) {
    ev("Enable", {lab(self), on});
    return static_cast<int16_t>(ret("Enable"));
}
void FakeUnexpected() { ev("UNEXPECTED VIRTUAL", {}); }
void* gFakeTable[256];
bool gFakeTableBuilt;
size_t EnableSlot() {
    int16_t (InterfaceWindow::*member)(int16_t) = &InterfaceWindow::Enable;
    uintptr_t words[2];
    std::memcpy(words, &member, sizeof(member));
    return words[0] / sizeof(void*);   // arm64: the vtable offset, the virtual flag is in the adjustment's low bit
}
}  // namespace

// The message function of the bankrupt window (0x101868c0, another unit): only its address is used.
int32_t BankruptWindowMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the rest of the callees
void UIMain_GetFullyOutOfFirstPerson() { ev("GetFullyOutOfFirstPerson", {}); }
void CObject::GoFirstPerson(int32_t on) const { ev("GoFirstPerson", {lab(this), on}); }
void DisableAllKeytables() { ev("DisableAllKeytables", {}); }
void EnableAllKeytables() { ev("EnableAllKeytables", {}); }
int32_t GameWindowToggle_Hidden() {
    ev("GameWindowToggle_Hidden", {});
    return static_cast<int32_t>(ret("GameWindowToggle_Hidden"));
}
void ToggleGameWindow() { ev("ToggleGameWindow", {}); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture", {}); }
uint32_t IveGotTicks() {
    ev("IveGotTicks", {});
    return static_cast<uint32_t>(ret("IveGotTicks"));
}
void CWorld::EndWinningSequence() { ev("EndWinningSequence", {lab(this)}); }
CGameType::CGameType() {
    ev("CGameType", {lab(this)});
    mType = static_cast<GameType>(ret("CGameType"));
}
const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    return ret("GetConstString") ? gHostString : nullptr;
}
TbError TbIMBToUnicode::ConvertString(char16_t* out, const unsigned char* source, uint16_t size) {
    ev("ConvertString", {lab(this), lab(out), lab(source), size});
    return TbError(0);
}
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("UI_DialogBox",
       {lab(params.mText), (static_cast<int64_t>(params.mRed) << 16) | (static_cast<int64_t>(params.mGreen) << 8) | params.mBlue,
        params.mValue8, params.mValueC, static_cast<int32_t>(button1), lab(reinterpret_cast<const void*>(callback1)),
        static_cast<int32_t>(button2), lab(reinterpret_cast<const void*>(callback2)), static_cast<int32_t>(button3),
        lab(reinterpret_cast<const void*>(callback3)), flag ? 1 : 0});
    return nullptr;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_u5022_addr(const char* name) {
    struct Entry {
        const char* name;
        uintptr_t address;
    };
    const Entry entries[] = {
        {"kt", reinterpret_cast<uintptr_t>(&gUIMainKeyTablesDisabled)},
        {"bank", reinterpret_cast<uintptr_t>(&gBankruptWindow)},
        {"panel", reinterpret_cast<uintptr_t>(&gUIPanelWindow)},
        {"game", reinterpret_cast<uintptr_t>(&gGameWindow)},
        {"fp", reinterpret_cast<uintptr_t>(&gFirstPersonWindow)},
        {"root", reinterpret_cast<uintptr_t>(&gUIRootWindow)},
        {"world", reinterpret_cast<uintptr_t>(&gGameWorld)},
        {"ticks", reinterpret_cast<uintptr_t>(&gCongratsTicks)},
        {"gtype", reinterpret_cast<uintptr_t>(gTheGameTypeObject)},
        {"guard", reinterpret_cast<uintptr_t>(gTheGameTypeGuard)},
        {"rmb", reinterpret_cast<uintptr_t>(&reinterpret_cast<CPlayerSettings*>(gPlayerSettingsRaw)->mCurrent.mRMBCancelOn)},
        {"res", reinterpret_cast<uintptr_t>(&reinterpret_cast<CText*>(gTextRaw)->mResourcePointers[0])},
        {"w8", reinterpret_cast<uintptr_t>(&gUIMainWordCCF8)},
        {"wc", reinterpret_cast<uintptr_t>(&gUIMainWordCCFC)},
        {"lightflags", reinterpret_cast<uintptr_t>(&reinterpret_cast<CLight*>(gUILightRaw)->mFlags)},
        {"light", reinterpret_cast<uintptr_t>(gUILightRaw)},
        {"cursor", reinterpret_cast<uintptr_t>(gCursorTableRaw)},
        {"frame", reinterpret_cast<uintptr_t>(gUIFrameNameRaw)},
        {"slider", reinterpret_cast<uintptr_t>(gUISliderNameRaw)},
        {"plain", reinterpret_cast<uintptr_t>(gUIPlainFrameNameRaw)},
        {"button", reinterpret_cast<uintptr_t>(gUIButtonNameRaw)},
        {"nlight", reinterpret_cast<uintptr_t>(gUIMainNodeLight)},
        {"ncursor", reinterpret_cast<uintptr_t>(gUIMainNodeCursors)},
        {"nframe", reinterpret_cast<uintptr_t>(gUIMainNodeFrame)},
        {"nslider", reinterpret_cast<uintptr_t>(gUIMainNodeSlider)},
        {"nplain", reinterpret_cast<uintptr_t>(gUIMainNodePlain)},
        {"nbutton", reinterpret_cast<uintptr_t>(gUIMainNodeButton)},
        {"dtlight", reinterpret_cast<uintptr_t>(&UIMain_DestroyLight)},
        {"dtcursor", reinterpret_cast<uintptr_t>(&UIMain_DestroyCursorTable)},
        {"dtstring", reinterpret_cast<uintptr_t>(&UIMain_DestroyString)},
        {"cbcong", reinterpret_cast<uintptr_t>(&CongratsWindowMsg)},
        {"cbbank", reinterpret_cast<uintptr_t>(&BankruptWindowMsg)},
        {"cscript", reinterpret_cast<uintptr_t>(gCongratsScript)},
        {"bscript", reinterpret_cast<uintptr_t>(gBankruptScript)},
        {"canim", reinterpret_cast<uintptr_t>(gCongratsAnim)},
        {"banim", reinterpret_cast<uintptr_t>(gBankruptAnim)},
        {"convbuf", reinterpret_cast<uintptr_t>(gUIMainConvertBuffer)},
        {"hcreated", reinterpret_cast<uintptr_t>(&gHostCreated)},
        {"hbuilt", reinterpret_cast<uintptr_t>(&gHostBuilt)},
        {"hstr", reinterpret_cast<uintptr_t>(&gHostString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(entry.address);
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_u5022_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Makes the block a window whose first word is the fake table (the Enable slot answers, any other slot logs).
void tpw_u5022_initwin(uint64_t address) {
    if (!gFakeTableBuilt) {
        for (void*& slot : gFakeTable) slot = reinterpret_cast<void*>(&FakeUnexpected);
        gFakeTable[EnableSlot()] = reinterpret_cast<void*>(&FakeEnable);
        gFakeTableBuilt = true;
    }
    *reinterpret_cast<void***>(address) = &gFakeTable[0];
}

// The offsets the module relies on (HOST_LAYOUT): the resource pointers of the text object, the settings byte, the rectangle's fields.
int32_t tpw_u5022_layout(int64_t* out) {
    alignas(16) static unsigned char probe[64];
    int32_t n = 0;
    out[n++] = reinterpret_cast<unsigned char*>(&reinterpret_cast<CText*>(gTextRaw)->mResourcePointers[0]) - gTextRaw;
    out[n++] = reinterpret_cast<unsigned char*>(&reinterpret_cast<CPlayerSettings*>(gPlayerSettingsRaw)->mCurrent.mRMBCancelOn) -
               gPlayerSettingsRaw;
    out[n++] = reinterpret_cast<unsigned char*>(&reinterpret_cast<CoolRect*>(probe)->mRight) - probe;
    out[n++] = reinterpret_cast<unsigned char*>(&reinterpret_cast<CoolRect*>(probe)->mBottom) - probe;
    return n;
}

// ---- the functions
void tpw_u5022_cong(uint64_t object) {
    UI_Congratulations(reinterpret_cast<CObject*>(object));
}
void tpw_u5022_bank(uint64_t object) { UI_Bankrupt(reinterpret_cast<CObject*>(object)); }
void tpw_u5022_sinit() { __sinit_ui_main_cpp(); }
int32_t tpw_u5022_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return CongratsWindowMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
