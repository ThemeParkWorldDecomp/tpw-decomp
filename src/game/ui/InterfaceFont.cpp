// InterfaceFont.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceFont code, named after it.

#define TPW_V51039_STRONG
#include "analyser.h"
#include "CBalanceTable.h"
#include "Clock.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "helpers/InterfaceFontHelpers.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "keytable_actions.h"
#include "LbDraw.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Main.h"
#include "MainBalance.h"
#include "MsgCheatMode.h"
#include "MslSubscriptionTree.h"
#include "OptionsBalance.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "SkyObjects.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "TbColour.h"
#include "TbDynamicStringTemplate.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPOptions.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "v51029_decls.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <ios>
#include <new>

// InterfaceFont and InterfaceFontWrapper (main app of SimTheme Park 2.04b3,
// 0x10168ae0..0x10168ff8; the inventory files the range under FS_MasterSave.cpp).
// PARTIAL: Gen8x8DebugFont and the width functions (0x1016906c..0x101692d4) are still to do.

InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}

InterfaceFontWrapper::~InterfaceFontWrapper() {}

// Unit v51118: small stand-ins of the main app (CTimeSig's three members are inline in include/TimeSig.h and are tested through it;
// CSkyObjects, CSaveLoad, InterfaceFontWrapper, the ride database and balance destructors, two messages, and the std containers'
// destructors, constructors and allocator functions of the master save).

// The words the constructors of std::list<std::wstring> (0x1016d63c) and of the park theme tree (0x1016e37c) read from their globals.
uint32_t gWStringListCtorWord = 0;   // 0x102a723c
uint32_t gParkTreeCtorWord = 0;      // 0x102a7234

namespace v51118 {

// Runtime glue the destructors call; weak defaults here, recording fakes in the test library.
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }   // 0x10238218
__attribute__((weak)) void KeyStringDtor(void* key, int16_t flag) {                    // 0x102382f0
    static_cast<TbDynamicStringTemplate<char>*>(key)->~TbDynamicStringTemplate();
    (void)flag;
}

namespace {
uint32_t WordOf_2(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x101696f8: the tree is destroyed from its root when there is one.
MacU16Set* U16SetDtor(MacU16Set* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mRoot != 0) SetTreeDestroy(self, self->mRoot);
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x1016985c
MacWStringList* WStringListDtor(MacWStringList* self, int16_t deleteFlag) {
    if (self != nullptr) {
        WStringListClear(self);
        if (deleteFlag > 0) OperatorDelete(self);
    }
    return self;
}

// 0x1016d63c: the count from a global, the sentinel (at +4) linked to itself. The two words the Mac copies from its stack are overwritten.
void WStringListCtor(MacWStringList* list) {
    uint32_t sentinel = WordOf_2(list) + 4;
    list->mCount = gWStringListCtorWord;
    list->mSentinelPrev = sentinel;
    list->mFirst = sentinel;
}

// 0x1016e37c: the count from a global, no root, the first byte of the compare object kept in the third word, begin = end().
void ParkThemeTreeCtor(ParkThemeTree* tree, const uint8_t* compare) {
    tree->mCount = gParkTreeCtorWord;
    tree->mRoot = nullptr;
    *reinterpret_cast<uint8_t*>(&tree->mSlot8) = *compare;
    tree->mBegin = nullptr;
    tree->mBegin = tree->end();
}

}  // namespace v51118

namespace ParkThemeTreeCallee {
// 0x1016dfd0: destroys the key of the pair (the flag is -1) unless the pointer is NULL.
void PairDestroy(ParkThemeTree* tree, ParkThemeTree::Pair* value) {
    (void)tree;
    if (value != nullptr) v51118::KeyStringDtor(reinterpret_cast<void*>(value), -1);
}

// 0x1016dc80: frees the node (the count is not used).
void NodeDeallocate(ParkThemeTree* tree, ParkThemeTree::node* node, unsigned long count) {
    (void)tree;
    (void)count;
    v51118::OperatorDelete(node);
}

// 0x1016b168
void PairDtor(ParkThemeTree::Pair* pair, int16_t flag) {
    if (pair != nullptr) {
        v51118::KeyStringDtor(reinterpret_cast<void*>(pair), -1);
        if (flag > 0) v51118::OperatorDelete(pair);
    }
}
}  // namespace ParkThemeTreeCallee

// 0x10168c04: 0 without a font, else the font's virtual at +0x28.
__attribute__((weak)) uint8_t fontwrapper_callee::GetVerticalSpacing(TbIFont* font) { return font->GetVerticalSpacing(); }
int32_t InterfaceFontWrapper::GetHeight() {
    if (mFont == nullptr) return 0;
    return fontwrapper_callee::GetVerticalSpacing(mFont);
}

// Unit v51029: seven small functions of the main app.
//   CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010,
//   CSubscription::Save 0x10152b08, __sinit_keyboard_cpp 0x101512fc,
//   InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38.

// ---------------------------------------------------------------------------------------------------------------- InterfaceFontWrapper

// 0x10168c7c (132 bytes). Loops while the index is below last - first (signed 32-bit) and the character is not NUL; each adds the
// virtual width (a short: the call is slot +0x14 of the vptr at +0x10, which is InterfaceFont::GetCharWidth here).
int32_t InterfaceFontWrapper::GetWidth(const char16_t* text, int16_t first, int16_t last) {
    int32_t total = 0;
    int32_t count = static_cast<int32_t>(last) - static_cast<int32_t>(first);
    const char16_t* p = text + first;
    for (int32_t i = 0; i < count && *p != 0; ++i, ++p) total += static_cast<int16_t>(GetCharWidth(*p));
    return static_cast<int16_t>(total);
}

// 0x10168d38 (144 bytes). While the 16-bit remaining room is above zero and the character is not NUL: room -= width (the width is
// the full register), index + 1. Answers the index - 1.
int32_t InterfaceFontWrapper::LastCharIn(const char16_t* text, int16_t first, int16_t last) {
    int32_t index = first;
    int32_t room = last;
    const char16_t* p = text + first;
    while (static_cast<int16_t>(room) > 0 && *p != 0) {
        room = room - GetCharWidth(*p);
        ++index;
        ++p;
    }
    return index - 1;
}

// Unit v51039 (main app): std::list<std::wstring>::clear and the destructors of the swear filter and of the theme map, the font
// wrapper's GetCharWidth, the park list's StoreReceivedList and four small UI helpers. The Mac's strings, heap and virtual calls are
// the callees of the headers' *_callee namespaces (weak definitions in the _callees file; the test library fakes them).

// ---- the weak definitions of the callees the host cannot call as the Mac does (the Mac's strings and heap, new plus constructor,
// virtual calls through __ptr_glue); the unit's test library defines them again as recording fakes.
#define TPW_WEAK __attribute__((weak))
static_assert(sizeof(CoolRect) <= 64, "UIMain_IndentClient keeps a CoolRect in 64 bytes");

namespace masterlist_callee {
// The Mac's list nodes hold Mac strings; the host never has such a list (no-op).
TPW_WEAK void WStringDtor(void*, int16_t) {}
TPW_WEAK void OperatorDelete(void* block) { ::operator delete(block); }
}  // namespace masterlist_callee

namespace fontwrapper_callee {
TPW_WEAK uint32_t GetCharIndex(TbIFont* font, const unsigned char*& text) { return font->GetCharIndex(text); }
TPW_WEAK uint32_t GetCharWidthOf(TbIFont* font, uint16_t index) { return font->GetCharSize(index).mWidth; }
}  // namespace fontwrapper_callee

// 0x1016d4e8 (140): nothing for a count of 0; else every node from the first to the sentinel: the string's destructor, the next link
// read, the node freed; then the count 0 and the sentinel linked to itself both ways.
void WStringListClear(MacWStringList* list) {
    if (list->mCount != 0) {
        uint32_t sentinel = WordOf(list) + 4;
        uint32_t node = list->mFirst;
        while (node != sentinel) {
            masterlist_callee::WStringDtor(NodeAt(node) + 8, -1);
            uint32_t next = *reinterpret_cast<uint32_t*>(NodeAt(node) + 4);
            masterlist_callee::OperatorDelete(NodeAt(node));
            node = next;
        }
        list->mCount = 0;
        list->mFirst = sentinel;
        list->mSentinelPrev = sentinel;
    }
}

// 0x101695a8 (116)
ParkThemeTree* ParkThemeTree::Destruct(ParkThemeTree* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mRoot != nullptr) self->destroy(self->mRoot);
        if (deleteFlag > 0) masterlist_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10168e04 (152)
int32_t InterfaceFontWrapper::GetCharWidth(char16_t character) {
    if (mFont == nullptr) return 0;
    uint16_t value = fontwrapper_callee::GetMBValue(gText.mResourcePointers[1], character);
    const unsigned char* text = reinterpret_cast<const unsigned char*>(&value);
    uint32_t index = fontwrapper_callee::GetCharIndex(mFont, text);
    uint32_t width = fontwrapper_callee::GetCharWidthOf(mFont, static_cast<uint16_t>(index));
    return static_cast<int16_t>(width);
}

InterfaceFont::InterfaceFont() {
    mBitmap = nullptr;
    mF00 = 0;
    mF02 = 2;
    mF04 = 2;
}

InterfaceFont::~InterfaceFont() {
    if (mBitmap != nullptr) delete[] mBitmap;
}

// GetHeight, GetWidth, GetCharWidth and LastCharIn are virtual on the Mac (the vptr at +0x10) and InterfaceFontWrapper
// overrides them; on the host they are not, so the four bodies pass a wrapper (every font CText holds) on to its own
// version, the call the Mac makes. They are not virtual here because many unit tests fake them on addresses that are
// not objects.
int32_t InterfaceFont::GetHeight() {
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->GetHeight();
    return mHeight;
}

void InterfaceFont::Release() {
    if (mBitmap != nullptr) {
        delete[] mBitmap;
        mBitmap = nullptr;
    }
}

// The small leaves the lobby's start reaches (main app, SimTheme Park 2.04b3): the theme texture folders, the authorised
// user text, the interface's built-in debug font and the interface string heap. The inventory files the first two under
// ui_ErrorDisplay.cpp, Gen8x8DebugFont under FS_MasterSave.cpp and Allocate under ui_cityLobbyController.cpp: the ranges
// do not match their names.

// ---- the debug font

// 0x1016906c: the built-in 8x8 font: `a` entries of 8 bytes, each starting with the byte `b` and then the byte `e` and
// ending in a zero word; `c` and `d` become the font's two halfwords at +2 and +6. Nothing but those two (and the
// array) is set when the array cannot be made. (The loop counts in a short against the count as an unsigned halfword.)
void InterfaceFont::Gen8x8DebugFont(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e) {
    mBitmap = new (std::nothrow) uint8_t[static_cast<uint32_t>(static_cast<int32_t>(a)) << 3];
    if (mBitmap != nullptr) {
        mF00 = static_cast<uint16_t>(a);
        for (int16_t i = 0; static_cast<int32_t>(i) < static_cast<int32_t>(mF00); ++i) {
            uint8_t* entry = mBitmap + static_cast<int32_t>(i) * 8;
            entry[0] = static_cast<uint8_t>(b);
            entry[1] = static_cast<uint8_t>(e);
            std::memset(entry + 4, 0, 4);
        }
        mHeight = d;
    }
    mF02 = static_cast<uint16_t>(c);
}

// The interface's text drawing (main app of SimTheme Park 2.04b3): UI_DrawText 0x1018b578, MakeRenderArea 0x1018b360 and
// AdjustJustificationForHebrew 0x1018b2c8 (ui_utils.cpp), UIMain_ConvertString 0x10181a7c (ui_main.cpp) and the three
// InterfaceFont width members 0x1016915c, 0x10169264, 0x101692d4 (the inventory files them under FS_MasterSave.cpp).
// UI_DrawText hands the text to the ltms text formatter (unit w35a), which draws through the Bullfrog library's current
// area, viewport and clip rectangle (src/bfl_lbdraw_leaves.cpp). The declarations are in include/UI_Base.h and
// include/InterfaceFont.h.

// ---- InterfaceFont's widths

// 0x1016915c
int32_t InterfaceFont::GetWidth(const char16_t* text, int16_t first, int16_t last) {
    // (the Mac's call is virtual: a wrapper answers with its own; see src/InterfaceFont.cpp)
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->GetWidth(text, first, last);
    int32_t width = 0;
    if (text == nullptr || mBitmap == nullptr) return 0;
    const int32_t length = static_cast<int32_t>(wcslen(text));
    if (last < first) last = static_cast<int16_t>(length - 1);
    if (length == 0 || first < 0 || first > length) return 0;
    if (last <= first || last > length) return 0;
    for (int32_t i = first; i <= last; ++i) {
        width += static_cast<int8_t>(mBitmap[static_cast<uint32_t>(text[i]) * 8]);
        width += mF02;
    }
    return width;
}

// 0x10169264
int32_t InterfaceFont::GetCharWidth(char16_t character) {
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->GetCharWidth(character);
    if (mBitmap != nullptr && character < mF00) return mF02 + static_cast<int8_t>(mBitmap[static_cast<uint32_t>(character) * 8]);
    return 0;
}

// 0x101692d4
int32_t InterfaceFont::LastCharIn(const char16_t* text, int16_t first, int16_t last) {
    if (auto* const wrapper = dynamic_cast<InterfaceFontWrapper*>(this)) return wrapper->LastCharIn(text, first, last);
    return 0;
}
