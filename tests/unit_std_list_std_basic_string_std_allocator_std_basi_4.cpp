// Test library for unit v51039 (libtpw_unit_std_list_std_basic_string_std_allocator_std_basi_4.dylib,
// tests/test_std_list_std_basic_string_std_allocator_std_basi_4.py), main app:
//   src/game/online/FS_OnlineList.cpp, src/game/online/OnlineParser.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp   WStringListClear, SwearFilter::Destruct,
//   ParkThemeTree::Destruct, InterfaceFontWrapper::GetCharWidth, CFSOnlineSystemParkList::StoreReceivedList, UIMain_IndentClient,
//   UIMain_GetStaffStateFrame, UIMain_SetStaffStateText.
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs); the globals of
// other files (gText, gUIMainConvertBuffer) are defined here, zero at start like the Mac's bss. gText is raw storage under the symbol
// of the real variable: the CText constructor and destructor need files this library does not compile.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"
#include "FS_OnlineList.h"
#include "InterfaceFontWrapper.h"
#include "Text.h"
#include "ui_main.h"

using namespace calllog;

#define I(x) static_cast<int64_t>(x)

// ---- globals of other files
char16_t gUIMainConvertBuffer[0x4000];
uint8_t gTextRaw[0x800] __asm__("_gText");

namespace {
const unsigned char* gConstString = nullptr;   // the block GetConstString answers with (set by the test)

uint32_t TagAt(const void* p) {   // (a word of the Mac's layout: the tag the test wrote)
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
}  // namespace

// ---- the fakes
namespace masterlist_callee {
void WStringDtor(void* string, int16_t flag) { ev("WStringDtor", {I(TagAt(string)), I(flag)}); }
void OperatorDelete(void* block) {
    int64_t l = lab(block);
    int64_t t = l == -1 ? I(TagAt(static_cast<uint8_t*>(block) + 8)) : 0;
    ev("delete", {l, t});
}
}  // namespace masterlist_callee

void ParkThemeTree::destroy(node* subtree) { ev("TreeDestroy", {lab(this), lab(subtree)}); }

namespace fontwrapper_callee {
uint16_t GetMBValue(void* converter, char16_t character) {
    ev("GetMBValue", {lab(converter), I(character)});
    return static_cast<uint16_t>(ret("GetMBValue"));
}
uint32_t GetCharIndex(TbIFont* font, const unsigned char*& text) {
    uint16_t v;
    std::memcpy(&v, text, 2);
    ev("GetCharIndex", {lab(font), I(v)});
    return static_cast<uint32_t>(ret("GetCharIndex"));
}
uint32_t GetCharWidthOf(TbIFont* font, uint16_t index) {
    ev("GetCharSize", {lab(font), I(index)});
    return static_cast<uint32_t>(ret("width"));
}
}  // namespace fontwrapper_callee

namespace parklist_callee {
void NewPark(const ccParkInfo* info, CFSOnlineSystemParkList* list) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(info);
    ev("new", {I(0xc0)});
    ev("ctor", {I(b[0]), I(b[1]), I(b[2]), I(b[3]), lab(list)});
}
}  // namespace parklist_callee

namespace indent_callee {
void RectCtor(void*) { ev("RectCtor", {I(0)}); }
int32_t GetPosition(InterfaceWindow* window, void*, int16_t subtract) {
    ev("GetPosition", {lab(window), I(subtract)});
    return 0;
}
void Adjust(void*, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("Adjust", {I(left), I(top), I(right), I(bottom)});
}
void SetClientRect(InterfaceWindow* window, void*) { ev("SetClientRect", {lab(window)}); }
void RectDtor(void*, int16_t flag) { ev("RectDtor", {I(flag)}); }
}  // namespace indent_callee

namespace staffstate_callee {
const unsigned char* GetConstString(void* table, uint32_t id) {
    ev("GetConstString", {lab(table), I(id)});
    return ret("GetConstString") != 0 ? gConstString : nullptr;
}
void ConvertString(void* converter, char16_t* out, const unsigned char* source, uint16_t size) {
    ev("ConvertString", {lab(converter), lab(out), lab(source), I(size)});
    uint16_t v = static_cast<uint16_t>(ret("conv"));
    out[0] = v;
    out[1] = static_cast<uint16_t>(v + 1);
    out[2] = 0;
}
}  // namespace staffstate_callee

namespace staffroom_callee {
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("SetText", {lab(window), lab(text)}); }
}  // namespace staffroom_callee

extern "C" {

uint64_t tpw_swf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gText", gTextRaw},
        {"buf", gUIMainConvertBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_swf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

#pragma clang diagnostic ignored "-Winvalid-offsetof"
// The host offsets the module writes by (the Mac's are 4, 4, 0x14, 0x54c): the tree's root, the received list's infos, the wrapper's font,
// gText's first resource pointer.
int32_t tpw_swf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(ccParkList, mInfos));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceFontWrapper, mFont));
    out[n++] = static_cast<int64_t>(offsetof(CText, mResourcePointers));
    out[n++] = static_cast<int64_t>(sizeof(ccParkInfo));
    return n;
}

uint64_t tpw_swf_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

void tpw_swf_setconst(uint64_t block) { gConstString = reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(block)); }

void tpw_swf_clear(uint64_t list) { WStringListClear(reinterpret_cast<MacWStringList*>(static_cast<uintptr_t>(list))); }
uint64_t tpw_swf_swear(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(SwearFilter::Destruct(reinterpret_cast<SwearFilter*>(static_cast<uintptr_t>(self)), flag)));
}
uint64_t tpw_swf_mapdtor(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ParkThemeTree::Destruct(reinterpret_cast<ParkThemeTree*>(static_cast<uintptr_t>(self)), flag)));
}
int32_t tpw_swf_charwidth(uint64_t wrapper, uint32_t character) {
    return reinterpret_cast<InterfaceFontWrapper*>(static_cast<uintptr_t>(wrapper))->InterfaceFontWrapper::GetCharWidth(static_cast<char16_t>(character));
}
void tpw_swf_store(uint64_t self, uint64_t list) {
    reinterpret_cast<CFSOnlineSystemParkList*>(static_cast<uintptr_t>(self))
        ->CFSOnlineSystemParkList::StoreReceivedList(reinterpret_cast<const ccParkList*>(static_cast<uintptr_t>(list)));
}
void tpw_swf_indent(uint64_t window, int16_t a, int16_t b) {
    UIMain_IndentClient(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), a, b);
}
int32_t tpw_swf_frame(int32_t state) { return UIMain_GetStaffStateFrame(state); }
void tpw_swf_settext(uint64_t window, int32_t state) {
    UIMain_SetStaffStateText(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), state);
}

}  // extern "C"
