// Test library for the visitor time and visitor number text objects as the game builds and feeds them (libtpw_unit_VisitorTextCtors.dylib,
// tests/test_VisitorTextCtors.py), main app:
//   src/game/ui/InterfaceTextObjectSprite.cpp (the glue, the classes and their Change), src/game/ui/InterfaceWindow.cpp (SetValue)
//
// The callees outside the unit are recording fakes: the draw object sprite's constructor, the formatter's, the base text sprite's Change (the flags,
// the text's length and a hash) and TbITable::FormatString (the table, the size, the id and the tag; it writes "id:value" or "id"). swprintf is real.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// gText as raw zeroed storage (its constructor needs files this library does not compile; the Mac's bss is zero too).
asm(".globl _gText\n.zerofill __DATA,__bss,_gText,2048,4\n");

// The key function: its definition emits the tag's vtable (the tag is only built and read here).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

namespace {
const void* gObj = nullptr;
const void* gFmt = nullptr;
const void* gTable = nullptr;

int64_t Lab(const void* p) {
    if (p == gObj) return 1;
    if (p == gFmt) return 2;
    return -1;
}

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
}  // namespace

// ---- the draw object chain, empty
InterfaceDrawObject::InterfaceDrawObject(void* data) { mData = data; }
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {
    ev("SpriteCtor", {Lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}

// The base text sprite's Change: what reaches the screen. A plain sprite (the old stand-in) gets flag 0x80 here with no text.
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    int64_t length = -1;
    uint32_t hash = 0;
    if (flags & 0x100) hash = TextHash(change->mText->mText, &length);
    ev("change", {Lab(this), static_cast<int64_t>(flags), length, static_cast<int64_t>(hash)});
}

// ---- the members' fakes
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() {}
CSpriteList::CSpriteList() {}
CMapLinkBase::~CMapLinkBase() {}
int32_t CSpriteList::Prepare() { return 0; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
CSpriteList::~CSpriteList() {}
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRender::TbFontRender() {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() { ev("FormatterCtor", {Lab(this)}); }
TbTextFormatter::~TbTextFormatter() {}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    const bool tagged = id != 0x1c9;
    int64_t magic = 0, tid = 0, value = 0, negative = 0, isvt = 0;
    if (tagged) {
        va_list ap;
        va_start(ap, id);
        TbVTag* tag = va_arg(ap, TbVTag*);
        va_end(ap);
        RideAgeTag* age = static_cast<RideAgeTag*>(tag);
        RideAgeTag reference(0, 0);
        void* tagVtable;
        void* referenceVtable;
        std::memcpy(&tagVtable, static_cast<void*>(tag), sizeof tagVtable);
        std::memcpy(&referenceVtable, static_cast<void*>(&reference), sizeof referenceVtable);
        isvt = tagVtable == referenceVtable;
        magic = age->mMagic;
        tid = age->mId;
        value = age->mValue;
        negative = age->mNegative;
    }
    ev("format", {this == gTable ? 1 : 0, static_cast<int64_t>(size), static_cast<int64_t>(id), tagged ? 1 : 0, magic, tid, value & 0xFFFFFFFFll, negative & 0xFFFFFFFFll, isvt});
    char buffer[40];
    if (tagged) std::snprintf(buffer, sizeof buffer, "%u:%d", id, static_cast<int32_t>(value));
    else std::snprintf(buffer, sizeof buffer, "%u", id);
    size_t i = 0;
    for (; buffer[i] != 0; ++i) out[i] = static_cast<char16_t>(buffer[i]);
    out[i] = 0;
    return 0;
}

extern "C" {

uint64_t tpw_vtc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The block address standing for the game text table (the pointer stored in gText +0x564).
void tpw_vtc_settable(uint64_t table) {
    gTable = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
    gText.mResourcePointers[6] = const_cast<void*>(gTable);
}

// The program's construction: the glue the panels call, on a block of 0x400 bytes (what their allocators hand out for the 0x19c request).
// which: 0 the visitor time object, 1 the visitor number object. out: the style bytes.
void tpw_vtc_make(int32_t which, uint64_t mem, int64_t* out) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(mem));
    std::memset(p, 0xAA, 0x400);
    auto* t = static_cast<InterfaceTextObjectSprite*>(p);
    gObj = t;
    gFmt = &t->mFormatter;
    if (which == 0) vsp_callee::TimeCtor(p);
    else kidlist_callee::VisitorNumCtor(p);
    out[0] = static_cast<uint8_t>(t->mJustify);
    out[1] = static_cast<uint8_t>(t->mVJustify);
    out[2] = t->mWrap;
    out[3] = t->mF0DF;
    out[4] = static_cast<uint8_t>(t->mFontId);
    out[5] = t->mColourMode;
    out[6] = static_cast<uint16_t>(t->mOffsetX);
    out[7] = static_cast<uint16_t>(t->mOffsetY);
    out[8] = t->TextColour()[0];
    out[9] = t->TextColour()[1];
    out[10] = t->TextColour()[2];
    out[11] = t->TextColour()[3];
}

// The value as a window hands it on: the real InterfaceWindow::SetValue on a window holding the object as its draw and/or client draw object.
int32_t tpw_vtc_setvalue(uint64_t drawObject, uint64_t clientObject, int32_t value) {
    auto* window = static_cast<InterfaceWindow*>(tpw::ArenaAllocZeroed(sizeof(InterfaceWindow)));
    window->mDrawObject = reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(drawObject));
    window->mClientDrawObject = reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(clientObject));
    return window->InterfaceWindow::SetValue(value);
}

int32_t tpw_vtc_sizes(int64_t* out) {
    out[0] = sizeof(InterfaceVisitorTimeTextObject);
    out[1] = sizeof(InterfaceVisitorNumTextObject);
    out[2] = sizeof(InterfaceTextObjectSprite);
    return 3;
}

}  // extern "C"
