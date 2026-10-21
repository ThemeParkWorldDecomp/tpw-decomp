// Test library for the cash, null cash, percent, visitor number and password text objects' Change and the password object's destructor
// (unit v5983; libtpw_unit_InterfaceCashTextObject_Change.dylib, tests/test_InterfaceCashTextObject_Change.py), main app:
//   src/game/ui/InterfaceTextObjectSprite.cpp
//
// Fakes: InterfaceTextObjectSprite::Change (logs the flags, optionally the words, the text's length and hash), TbITable::FormatString
// (writes "id:value" or "id"), labs, _wcsset (really sets), the cursor sprite's destructor and operator delete. gText and the shared
// text buffer are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "Text.h"
#include "TbITable.h"
#include "TbVTag.h"

using namespace calllog;

// gText as raw zeroed storage (its constructor needs files this library does not compile; the Mac's bss is zero too).
asm(".globl _gText\n.zerofill __DATA,__bss,_gText,2048,4\n");
char16_t gUIMainConvertBuffer[0x4000];

// The key function: its definition emits the tag's vtable (the tag is only built and read here).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

namespace {
int gLogWords = 0;
const void* gTable = nullptr;

uint32_t TextHash(const char16_t* text, int64_t* length) {
    uint32_t h = 2166136261u;
    int64_t n = 0;
    while (n < 0x400 && text[n] != 0) {
        h = (h ^ static_cast<uint32_t>(text[n])) * 16777619u;
        ++n;
    }
    *length = n;
    return h;
}

void PutText(char16_t* out, uint32_t id, bool tagged, int32_t value) {
    char buffer[40];
    if (tagged) std::snprintf(buffer, sizeof buffer, "%u:%d", id, value);
    else std::snprintf(buffer, sizeof buffer, "%u", id);
    size_t i = 0;
    for (; buffer[i] != 0; ++i) out[i] = static_cast<char16_t>(buffer[i]);
    out[i] = 0;
}
}  // namespace

void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    int64_t length = -1;
    uint32_t hash = 0;
    if (flags & 0x100) hash = TextHash(change->mText->mText, &length);
    int64_t w0 = 0, w1 = 0, w2 = 0, w3 = 0, rect = 0;
    if (gLogWords) {
        uint32_t first;
        std::memcpy(&first, change->mF00, 4);
        w0 = first;
        w1 = (static_cast<uint32_t>(static_cast<uint16_t>(change->mFrame)) << 16) | static_cast<uint16_t>(change->mDepth);
        w2 = (static_cast<uint32_t>(static_cast<uint16_t>(change->mF08)) << 16) | static_cast<uint16_t>(change->mFrameBase);
        w3 = (static_cast<uint32_t>(static_cast<uint16_t>(change->mF0C)) << 16) | static_cast<uint16_t>(change->mF0E);
        rect = lab(change->mRect);
    }
    ev("change", {lab(this), static_cast<int64_t>(flags), w0, w1, w2, w3, rect, length, static_cast<int64_t>(hash)});
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    bool tagged = id != 0x1c9;
    int64_t magic = 0, tid = 0, val = 0, isvt = 0;
    if (tagged) {
        va_list ap;
        va_start(ap, id);
        TbVTag* tag = va_arg(ap, TbVTag*);
        va_end(ap);
        magic = tag->mMagic;
        tid = tag->mId;
        TbIntegerVTag reference(0, 0);
        void* tagVtable;
        void* referenceVtable;
        std::memcpy(&tagVtable, static_cast<void*>(tag), sizeof tagVtable);
        std::memcpy(&referenceVtable, static_cast<void*>(&reference), sizeof referenceVtable);
        isvt = tagVtable == referenceVtable;
        val = static_cast<TbIntegerVTag*>(tag)->mValue;
    }
    ev("format", {this == gTable ? 1 : 0, static_cast<int64_t>(size), static_cast<int64_t>(id), tagged ? 1 : 0, magic, tid, val & 0xFFFFFFFFll, isvt});
    PutText(out, id, tagged, static_cast<int32_t>(val));
    return 0;
}

namespace textobj_callee {
int32_t Labs(int32_t value) {
    ev("labs", {static_cast<int64_t>(static_cast<uint32_t>(value))});
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}
void WcsSet(char16_t* text, char16_t c) {
    ev("wcsset", {static_cast<int64_t>(c)});
    for (; *text != 0; ++text) *text = c;
}
void CursorDtor(void*, int16_t deleteFlag) { ev("cursordtor", {static_cast<int64_t>(deleteFlag)}); }
void Delete(void*) { ev("delete", {}); }
}  // namespace textobj_callee

extern "C" {

uint64_t tpw_tx_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"buffer", gUIMainConvertBuffer},
        {"table", &gText.mResourcePointers[6]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_tx_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The block address standing for the game text table (the pointer stored in gText +0x564).
void tpw_tx_settable(uint64_t table) {
    gTable = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
    gText.mResourcePointers[6] = const_cast<void*>(gTable);
}

int32_t tpw_tx_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(DOChange, mText));
    return n;
}

// fn: 0 cash, 1 password, 2 null cash, 3 percent, 4 visitor. The change's words as the Mac has them; `text` is the host address of
// the style's text, `rect` that of the rectangle.
void tpw_tx_change(int32_t fn, uint64_t self, uint32_t flags, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3, uint64_t text, uint64_t rect) {
    DOChange change;
    std::memcpy(change.mF00, &w0, 4);
    change.mFrame = static_cast<int16_t>(w1 >> 16);
    change.mDepth = static_cast<int16_t>(w1);
    change.mF08 = static_cast<int16_t>(w2 >> 16);
    change.mFrameBase = static_cast<int16_t>(w2);
    change.mF0C = static_cast<int16_t>(w3 >> 16);
    change.mF0E = static_cast<int16_t>(w3);
    DOTextStyle style;
    style.mText = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text));
    change.mText = &style;
    change.mRect = reinterpret_cast<CoolRect*>(static_cast<uintptr_t>(rect));
    auto* object = reinterpret_cast<InterfaceTextObjectSprite*>(static_cast<uintptr_t>(self));
    gLogWords = fn == 1;
    switch (fn) {
    case 0: InterfaceCashTextObject_Change(object, flags, &change); break;
    case 1: InterfacePasswordTextObject_Change(object, flags, &change); break;
    case 2: InterfaceNullCashTextObject_Change(object, flags, &change); break;
    case 3: InterfacePercentTextObject_Change(object, flags, &change); break;
    default: InterfaceVisitorNumTextObject_Change(object, flags, &change); break;
    }
    gLogWords = 0;
}

uint64_t tpw_tx_dtor(uint64_t self, int32_t flag) {
    auto* object = reinterpret_cast<InterfacePasswordTextObject*>(static_cast<uintptr_t>(self));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(InterfacePasswordTextObject_Dtor(object, static_cast<int16_t>(flag))));
}

}  // extern "C"
