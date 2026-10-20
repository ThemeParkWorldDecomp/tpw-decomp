// Unit v51039 (main app): std::list<std::wstring>::clear and the destructors of the swear filter and of the theme map, the font
// wrapper's GetCharWidth, the park list's StoreReceivedList and four small UI helpers. The Mac's strings, heap and virtual calls are
// the callees of the headers' *_callee namespaces (weak definitions in the _callees file; the test library fakes them).
#define TPW_V51039_STRONG
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "analyser.h"
#include "FS_MasterSave.h"
#include "FS_OnlineList.h"
#include "InterfaceFontWrapper.h"
#include "Text.h"
#include "ui_main.h"

#include <dlfcn.h>

#include <new>

#include "platform/arena.h"

#include "TbITable.h"
#include "TbResource.h"
#include "UI_Base.h"

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

namespace parklist_callee {
// CFSOnlinePark's constructor (0x1017c0d8) is found by its symbol at run time: the test library does not compile its file.
TPW_WEAK void NewPark(const ccParkInfo* info, CFSOnlineSystemParkList* list) {
    typedef void (*Ctor)(void*, const ccParkInfo*, CFSOnlineSystemParkList*);
    static Ctor ctor = reinterpret_cast<Ctor>(dlsym(RTLD_DEFAULT, "_ZN13CFSOnlineParkC1EPK10ccParkInfoP23CFSOnlineSystemParkList"));
    void* park = ::operator new(0xc0);
    if (ctor != nullptr) ctor(park, info, list);
}
}  // namespace parklist_callee

namespace indent_callee {
// (virtual calls only: no symbol of another file is needed)
TPW_WEAK void Adjust(void* rect, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    static_cast<CoolRect*>(rect)->Adjust(left, top, right, bottom);
}
TPW_WEAK void RectDtor(void* rect, int16_t) { static_cast<CoolRect*>(rect)->~CoolRect(); }
}  // namespace indent_callee
// NOT DEFINED HERE (they need symbols of files the test library does not compile; a callees file of the launcher defines them):
// fontwrapper_callee::GetMBValue (TbIUnicodeToMB::GetMBValue), indent_callee::RectCtor / GetPosition / SetClientRect (CoolRect::CoolRect,
// InterfaceWindow::GetPosition, InterfaceWindow::SetClientRect; platform/indent_dispatch.cpp), staffstate_callee::GetConstString / ConvertString (TbITable, TbIMBToUnicode).

namespace {
// The Mac's links are 32-bit words: offsets from the arena base on the host (Ptr32.h).
inline uint8_t* NodeAt(uint32_t word) { return tpw::gArenaBase + word; }
inline uint32_t WordOf(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

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

// 0x101697bc (120). The original also tests `this + 0xc` against 0 (this = 0xfffffff4); not reproducible on the host.
SwearFilter* SwearFilter::Destruct(SwearFilter* self, int16_t deleteFlag) {
    if (self != nullptr) {
        uint8_t* base = reinterpret_cast<uint8_t*>(self);
        WStringListClear(reinterpret_cast<MacWStringList*>(base + 0xc));
        WStringListClear(reinterpret_cast<MacWStringList*>(base));
        if (deleteFlag > 0) masterlist_callee::OperatorDelete(self);
    }
    return self;
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

// 0x10178bb0 (116): the signed count is compared with cmpw; the stride is the info's 0x66 bytes.
void CFSOnlineSystemParkList::StoreReceivedList(const ccParkList* list) {
    const ccParkInfo* info = list->mInfos;
    for (int32_t i = 0; i < list->mCount; ++i) {
        parklist_callee::NewPark(info, this);
        ++info;
    }
}

// 0x10180940 (156)
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) {
    alignas(16) unsigned char rect[64];
    indent_callee::RectCtor(rect);
    indent_callee::GetPosition(window, rect, 1);
    indent_callee::Adjust(rect, a, b, static_cast<int16_t>(-a), static_cast<int16_t>(-b));
    indent_callee::SetClientRect(window, rect);
    indent_callee::RectDtor(rect, -1);
}

// 0x10180a1c (84): the frame of a staff state, 9 for a state outside 0..4.
int32_t UIMain_GetStaffStateFrame(int32_t state) {
    if (state == 3) return 2;
    if (state < 3) {
        if (state < 2) return state >= 0 ? 0 : 9;
        return 1;
    }
    return state < 5 ? 3 : 9;
}

// 0x10180aa0 (160). QUIRK: the converter used is resource 0 (+0x54c) but the test is on resource 1 (+0x550); an unconverted buffer is
// passed on when that one is missing.
void UIMain_SetStaffStateText(InterfaceWindow* window, int32_t state) {
    char16_t* text = gUIMainConvertBuffer;
    const unsigned char* string = staffstate_callee::GetConstString(gText.mResourcePointers[10], static_cast<uint32_t>(state));
    if (string == nullptr) {
        text = nullptr;
    } else if (gText.mResourcePointers[1] != nullptr) {
        staffstate_callee::ConvertString(gText.mResourcePointers[0], text, string, 0x200);
    }
    staffroom_callee::WindowSetText(window, text);
}
