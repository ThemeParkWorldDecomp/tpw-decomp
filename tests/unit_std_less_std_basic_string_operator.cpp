// Test library for the small online / lobby / 2D map functions of unit v51044 (libtpw_unit_std_less_std_basic_string_operator.dylib,
// tests/test_std_less_std_basic_string_operator.py), main app:
//   src/game/online/online.cpp, src/game/ui/ui_2dmap.cpp, src/game/ui/ui_worldLobbyController.cpp   WideStringLess, the three allocator destroys, PairCtor, WStreambufUflow,
//   Show/HideWorldLobbySats, WorldLobbySetZoomButton, CUI2DMapSprite::CUI2DMapSprite
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs); the globals of other
// files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "MslWStringTree.h"
#include "TbTList.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "tb_ezsprite.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files
InterfaceWindow* gUISatelliteParent;   // 0x10396fec
CUI2DMap gUI2DMap;                     // 0x1039742c
CEZSprite gei_EZSprite;                // engine data
CEZSprite::CEZSprite() {}
CEZSprite::~CEZSprite() {}

namespace {
uint64_t gDetails;   // what GetSpriteDetails answers (a host address)

int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// ---- the fakes
namespace wstring_less_callee {
bool Lt(const MslWideString* a, const MslWideString* b) {
    ev("Lt", {lab(a), lab(b)});
    return (ret("Lt") & 0xff) != 0;
}
}  // namespace wstring_less_callee

namespace buddy_tree_callee {
void Delete(void* p) { ev("Delete", {lab(p)}); }
}  // namespace buddy_tree_callee

namespace online_callee {
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("Copy", {lab(self), lab(other), static_cast<int32_t>(pos), static_cast<int32_t>(count)});
}
}  // namespace online_callee

namespace wstreambuf_callee {
uint16_t Underflow(MslWStreambuf* self) {
    ev("Underflow", {lab(self)});
    return static_cast<uint16_t>(ret("Underflow"));
}
}  // namespace wstreambuf_callee

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(ret("GetWindow")));
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return 0;
}

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}

SSpriteGameData* CEZSprite::CreateSpriteInstance(void* sprite) {
    ev("CreateSpriteInstance", {lab(this), Raw(sprite)});
    return reinterpret_cast<SSpriteGameData*>(static_cast<uintptr_t>(ret("CreateSpriteInstance")));
}

void* CEZSprite::GetSpriteDetails(void* instance) {
    ev("GetSpriteDetails", {lab(this), Raw(instance)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(gDetails));
}

void CUI2DMapSprite::SetVisibility(int32_t visible) {
    uint32_t sprite32 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mSprite));
    ev("SetVisibility", {lab(this), visible, mKind, mMode, mMatch, mVisible, static_cast<int64_t>(sprite32)});
}

template <>
void TbTList<CUI2DMapSprite*, CUI2DMapSprite*>::AddTail(CUI2DMapSprite* value) {
    ev("AddTail", {lab(this), lab(value)});
}

extern "C" {

uint64_t tpw_v51044_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gparent", &gUISatelliteParent},
        {"gmap", &gUI2DMap},
        {"gei", &gei_EZSprite},
        {"details", &gDetails},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51044_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v51044_layout(int64_t* out) {
    CUI2DMapSprite* sprite = nullptr;
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mF00));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mKind));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mMode));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mMatch));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mF20) + 4);
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mSprite));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mVisible));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMap, mBytes));
    out[n++] = static_cast<int64_t>(offsetof(NocaseRep, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(NocaseRep, mData));
    out[n++] = static_cast<int64_t>(offsetof(BuddyEntry, mDirect));
    out[n++] = static_cast<int64_t>(offsetof(BuddyEntry, mFlag));
    MslWStreambuf probe;
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&probe.mGNext) - reinterpret_cast<char*>(&probe));
    (void)sprite;
    return n;
}

// ---- the functions
uint32_t tpw_v51044_WideStringLess(uint64_t functor, uint64_t a, uint64_t b) {
    return WideStringLess(reinterpret_cast<void*>(functor), reinterpret_cast<const MslWideString*>(a),
                          reinterpret_cast<const MslWideString*>(b));
}

void tpw_v51044_AllocDestroyNocase(uint64_t allocator, uint64_t p) {
    nocase_callee::AllocDestroy(reinterpret_cast<void*>(allocator), reinterpret_cast<nocase_string*>(p));
}

void tpw_v51044_AllocDestroyPair(uint64_t allocator, uint64_t p) {
    buddy_tree_callee::AllocDestroy(reinterpret_cast<void*>(allocator), reinterpret_cast<void*>(p));
}

void tpw_v51044_AllocDestroyWString(uint64_t allocator, uint64_t p) {
    wstring_alloc::Destroy(reinterpret_cast<void*>(allocator), reinterpret_cast<void*>(p));
}

uint64_t tpw_v51044_PairCtor(uint64_t self, uint64_t src) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        buddy_tree_callee::PairCtor(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(src))));
}

// The stream object is built in the arena block; its get pointer is set first. Answers the character; the pointer after goes to *after.
uint32_t tpw_v51044_Uflow(uint64_t object, uint64_t gnext, uint64_t* after) {
    MslWStreambuf* sb = new (reinterpret_cast<void*>(object)) MslWStreambuf();
    sb->mGNext = reinterpret_cast<char16_t*>(gnext);
    uint32_t answer = WStreambufUflow(sb);
    *after = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sb->mGNext));
    return answer;
}

void tpw_v51044_ShowWorldLobbySats() { ShowWorldLobbySats(); }
void tpw_v51044_HideWorldLobbySats() { HideWorldLobbySats(); }
void tpw_v51044_WorldLobbySetZoomButton(int32_t zoom) { WorldLobbySetZoomButton(zoom); }

void tpw_v51044_SpriteCtor(uint64_t object, uint64_t image, int32_t id, int32_t a, int32_t b, float scale) {
    TUI2DMapIndividualShowStatus status{a, b};
    new (reinterpret_cast<void*>(object)) CUI2DMapSprite(reinterpret_cast<void*>(image), static_cast<EUI2DMapSpriteID>(id), status, scale);
}

}  // extern "C"
