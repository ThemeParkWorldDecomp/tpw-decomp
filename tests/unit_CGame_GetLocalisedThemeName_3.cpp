// Test library for eight small main-app functions (unit v51048; libtpw_unit_CGame_GetLocalisedThemeName_3.dylib,
// tests/test_CGame_GetLocalisedThemeName_3.py), main app:
//   src/Game_level_CGame_GetLocalisedThemeName_3.cpp   CGame::GetLocalisedThemeName (narrow), CGame::TogglePause, __sinit_ActionRecorder_cpp,
//   the pending-delete list's RemoveAll, CParkTime::GetFunnyTimeForGameTurns, InterfaceBar::SetValue and operator-, UI_PutDeferredMessage
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs); the globals of other files the
// unit reads are defined here. The wide string's constructors and destructor are fakes; InterfaceWindow, InterfaceBar and
// InterfaceDrawObject are given empty bodies (their vtables), with WhatAmI and Change recording.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include <cstdlib>
namespace tpw {
// Zeroed blocks from the heap: this unit links its own operator delete (the node frees are logged), so no arena.
inline void* ArenaAllocZeroed(std::size_t size) { return std::calloc(size, 1); }
}  // namespace tpw

#include "WorldThings.h"
#include "ActionRecorder.h"
#include "Advisor.h"
#include "Clock.h"
#include "Game.h"
#include "ParkTime.h"
#include "TPSound.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "WorldThings.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CFixedTime::CFixedTime() {}   // gClock's members (the clock is never run here)
CVariableTime::CVariableTime() {}
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
CoolRegion::~CoolRegion() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CClock gClock;
UIInterface gUIInterface;
CActionRecorder gActionRecorder;
InterfaceTimedMessages* gUITimedMessages;

void UIMain_DestroyWString(void*) {}

// ---- the wide string: constructors, destructor and (empty) virtuals
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() { ev("CtorW", {newlab(this)}); }
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("CtorWText", {lab(this)}, {narrow(text).c_str()});
}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() { ev("DtorW", {lab(this), -1}); }
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

// ---- the fakes
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    ev("StringToUniString", {lab(&out), lab(const_cast<TbDynamicStringTemplate<char>*>(&in))});
    return static_cast<int32_t>(ret("StringToUniString"));
}

void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    ev("GetLocalisedThemeNameW",
       {lab(const_cast<CGame*>(this)), lab(const_cast<TbDynamicStringTemplate<char16_t>*>(&theme)), lab(&out)});
}

void UIInterface::TogglePause() { ev("UIInterface_TogglePause", {lab(this)}); }
void CClock::TogglePause() { ev("Clock_TogglePause", {lab(this)}); }
void SOUND_UnPause() { ev("SOUND_UnPause", {}); }
void SOUND_Pause() { ev("SOUND_Pause", {}); }
void SOUND_SetPauseOn3DSoundsOnly(int32_t on) { ev("SOUND_SetPauseOn3DSoundsOnly", {on}); }
void ADV_UnPause() { ev("ADV_UnPause", {}); }
void ADV_Pause() { ev("ADV_Pause", {}); }

int16_t InterfaceTimedMessages::Add(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    ev("Add", {lab(this), lab(window), message, a, b, static_cast<int64_t>(time)});
    return 0;
}

void CActionRecorder::Construct() { ev("ActionRecorder_ctor", {lab(this)}); }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)});
}

// the node free: operator delete, logging the id the node holds


// ---- the windows: the vtables of InterfaceWindow, InterfaceBar and InterfaceDrawObject
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

InterfaceBar::InterfaceBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                           int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}
void InterfaceBar::DrawFunction() {}
InterfaceBar::~InterfaceBar() {}
int32_t InterfaceBar::WhatAmI() {
    ev("WhatAmI", {lab(this)});
    return static_cast<int32_t>(ret("WhatAmI"));
}
int32_t InterfaceBar::GetValue() { return mValue; }

InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    int32_t word;
    std::memcpy(&word, change->mF00, sizeof(word));
    ev("Change", {lab(this), static_cast<int64_t>(flags), word});
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return nullptr; }

namespace {
using PendingList = TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_v51048_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ui", &gUIInterface},
        {"clock", &gClock},
        {"recorder", &gActionRecorder},
        {"string", gActionRecorderString},
        {"string_node", gActionRecorderStringNode},
        {"recorder_node", gActionRecorderNode},
        {"dtor_w", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"dtor_rec", gActionRecorderDtor},
        {"timed", &gUITimedMessages},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_v51048_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A list node from the heap (RemoveAll frees it with operator delete): id, next, previous.
uint64_t tpw_v51048_mk_node(uint32_t id, uint64_t next, uint64_t prev) {
    PendingList::CNode* node = static_cast<PendingList::CNode*>(::operator new(sizeof(PendingList::CNode)));
    std::memset(static_cast<void*>(node), 0, sizeof(PendingList::CNode));
    std::memcpy(static_cast<void*>(node), &id, 2);
    node->mNext = reinterpret_cast<PendingList::CNode*>(static_cast<uintptr_t>(next));
    node->mPrev = reinterpret_cast<PendingList::CNode*>(static_cast<uintptr_t>(prev));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node));
}

// ---- the objects the host builds by its own offsets
uint64_t tpw_v51048_mk_draw() {
    void* m = tpw::ArenaAllocZeroed(sizeof(InterfaceDrawObject));
    new (m) InterfaceDrawObject(nullptr);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}
uint64_t tpw_v51048_mk_bar(int32_t value, uint64_t d1, uint64_t d2) {
    void* m = tpw::ArenaAllocZeroed(sizeof(InterfaceBar));
    InterfaceBar* bar = new (m) InterfaceBar(nullptr, 0, 0, 0, 0, 0, 0);
    bar->mValue = value;
    bar->mDrawObject = reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(d1));
    bar->mClientDrawObject = reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(d2));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(bar));
}
int32_t tpw_v51048_bar_value(uint64_t bar) { return reinterpret_cast<InterfaceBar*>(static_cast<uintptr_t>(bar))->mValue; }
uint64_t tpw_v51048_mk_park(uint32_t rate) {
    CParkTime* park = reinterpret_cast<CParkTime*>(tpw::ArenaAllocZeroed(sizeof(CParkTime)));
    park->mFunnySecsPerRealSec = rate;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(park));
}
uint64_t tpw_v51048_mk_game(int32_t f1c, uint32_t f20, int32_t f3c) {
    CGame* game = reinterpret_cast<CGame*>(tpw::ArenaAllocZeroed(sizeof(CGame)));
    game->mF1C = f1c;
    game->mF20 = static_cast<uint8_t>(f20);
    game->mF3C = f3c;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(game));
}
void tpw_v51048_game_get(uint64_t game, int32_t* out) {
    const CGame* g = reinterpret_cast<const CGame*>(static_cast<uintptr_t>(game));
    out[0] = g->mF1C;
    out[1] = g->mF20;
    out[2] = g->mF3C;
}

// ---- the functions, one wrapper each with C types only
void tpw_v51048_name(uint64_t game, uint64_t theme, uint64_t out) {
    reinterpret_cast<CGame*>(static_cast<uintptr_t>(game))
        ->GetLocalisedThemeName(*reinterpret_cast<const TbDynamicStringTemplate<char>*>(static_cast<uintptr_t>(theme)),
                                *reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(static_cast<uintptr_t>(out)));
}
void tpw_v51048_toggle(uint64_t game, int32_t allSounds) {
    reinterpret_cast<CGame*>(static_cast<uintptr_t>(game))->TogglePause(allSounds != 0);
}
void tpw_v51048_sinit() { __sinit_ActionRecorder_cpp(); }
// the list: head, tail and count in; head, tail, count out
void tpw_v51048_removeall(uint64_t head, uint64_t tail, int32_t count, uint32_t* out) {
    PendingList list;
    list.mHead = reinterpret_cast<PendingList::CNode*>(static_cast<uintptr_t>(head));
    list.mTail = reinterpret_cast<PendingList::CNode*>(static_cast<uintptr_t>(tail));
    list.mCount = count;
    list.RemoveAll();
    out[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(list.mHead));
    out[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(list.mTail));
    out[2] = static_cast<uint32_t>(list.mCount);
}
int64_t tpw_v51048_funny(uint64_t park, uint32_t turns) {
    return reinterpret_cast<const CParkTime*>(static_cast<uintptr_t>(park))->GetFunnyTimeForGameTurns(turns).mTicks;
}
int32_t tpw_v51048_setvalue(uint64_t bar, int32_t value) {
    return reinterpret_cast<InterfaceBar*>(static_cast<uintptr_t>(bar))->SetValue(value);
}
int32_t tpw_v51048_minus(uint64_t bar, uint64_t other) {
    return reinterpret_cast<InterfaceBar*>(static_cast<uintptr_t>(bar))
        ->operator-(*reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(other)));
}
int32_t tpw_v51048_put(uint64_t window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    return UI_PutDeferredMessage(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b, time);
}

}  // extern "C"
