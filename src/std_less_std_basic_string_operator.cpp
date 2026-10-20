// Unit v51044: small functions of the online, lobby and 2D map code:
//   0x1021eb7c std::less<std::wstring>::operator()          0x1021ecb8 std::allocator<nocase_string>::destroy
//   0x1021e4dc std::basic_streambuf<wchar_t>::uflow         0x10222c78 std::allocator<pair<const nocase_string, BuddyInfo>>::destroy
//   0x102238d4 std::allocator<std::wstring>::destroy        0x10227840 std::pair<const nocase_string, BuddyInfo>::pair (copy)
//   0x1022b7e8 ShowWorldLobbySats  0x1022b85c HideWorldLobbySats  0x1022ba30 WorldLobbySetZoomButton
//   0x10230d64 CUI2DMapSprite::CUI2DMapSprite
#include <cstdint>
#include <cstring>

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

// 0x102318f0: the sprite list's AddTail (translated elsewhere; the test library fakes it).
template <>
void TbTList<CUI2DMapSprite*, CUI2DMapSprite*>::AddTail(CUI2DMapSprite* value);

namespace {

// The three allocators' destroy functions are the same code: the string is one word, a pointer to a rep with the reference count at 0x08
// and the data pointer at 0x0c. The count is first made at least 1 (a count of 0 is taken as 1), then lowered; the string keeps its rep
// only when the count reached 0, and then the data and the rep are freed.
void ReleaseString(void* slot) {
    if (slot == nullptr) return;
    NocaseRep** word = reinterpret_cast<NocaseRep**>(slot);
    NocaseRep* rep = *word;
    if (rep->mRefs == 0) {
        uint32_t refs = rep->mRefs;
        if (refs == 0) refs = 1;
        rep->mRefs = refs;
    }
    uint32_t left = rep->mRefs - 1;
    rep->mRefs = left;
    if (left != 0) rep = nullptr;
    *word = rep;
    if (*word != nullptr) {
        buddy_tree_callee::Delete((*word)->mData);
        buddy_tree_callee::Delete(*word);
    }
}

}  // namespace

// 0x1021eb7c
bool WideStringLess(void* functor, const MslWideString* a, const MslWideString* b) {
    (void)functor;
    return wstring_less_callee::Lt(a, b);
}

namespace nocase_callee {
// 0x1021ecb8
void AllocDestroy(void* allocator, nocase_string* value) {
    (void)allocator;
    ReleaseString(value);
}
}  // namespace nocase_callee

namespace buddy_tree_callee {
// 0x10222c78
void AllocDestroy(void* allocator, void* pair) {
    (void)allocator;
    ReleaseString(pair);
}

// 0x10227840: the key is copied whole, then the two flag bytes (a halfword on the Mac).
void* PairCtor(void* self, const void* src) {
    online_callee::NocaseWStringCopy(self, src, 0, 0xffffffffu);
    BuddyEntry* to = static_cast<BuddyEntry*>(self);
    const BuddyEntry* from = static_cast<const BuddyEntry*>(src);
    to->mDirect = from->mDirect;
    to->mFlag = from->mFlag;
    return self;
}
}  // namespace buddy_tree_callee

namespace wstring_alloc {
// 0x102238d4
void Destroy(void* allocator, void* string) {
    (void)allocator;
    ReleaseString(string);
}
}  // namespace wstring_alloc

// 0x1021e4dc
uint16_t WStreambufUflow(MslWStreambuf* self) {
    uint16_t result = 0xffff;
    if (wstreambuf_callee::Underflow(self) != 0xffff) {
        char16_t* at = self->mGNext;
        self->mGNext = at + 1;
        result = *at;
    }
    return result;
}

// 0x1022b7e8
void ShowWorldLobbySats() {
    UI_SendMessage(gUISatelliteParent->GetWindow(0x16007), 6, 1, 0);
}

// 0x1022b85c
void HideWorldLobbySats() {
    UI_SendMessage(gUISatelliteParent->GetWindow(0x16007), 6, 0, 0);
}

// 0x1022ba30
void WorldLobbySetZoomButton(int32_t zoom) {
    InterfaceWindow* parent = gUISatelliteParent;
    if (parent == nullptr) return;
    int8_t state = (zoom == 1) ? 1 : 0;
    static_cast<InterfaceButton*>(parent->GetWindow(0x16006))->SetState(state, 1);
}

// 0x10230d64
CUI2DMapSprite::CUI2DMapSprite(void* image, EUI2DMapSpriteID id, TUI2DMapIndividualShowStatus status, float scale) {
    const uint32_t vtable = 0x102b2958u;   // the Mac's vtable address (the host class has no vtable)
    std::memcpy(mF00, &vtable, 4);
    mSprite = gei_EZSprite.CreateSpriteInstance(image);
    mKind = id;
    mVisible = 1;
    mMode = status.mA;
    mMatch = status.mB;
    SetVisibility(0);
    std::memcpy(&mF20[4], &scale, 4);   // Mac 0x24
    uint32_t* details = static_cast<uint32_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
    details[4] |= 0x10000u;
    details[4] |= 0x20000u;
    TbTList<CUI2DMapSprite*, CUI2DMapSprite*>* list =
        *reinterpret_cast<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>**>(gUI2DMap.mBytes + 0x20080);
    list->AddTail(this);
}
