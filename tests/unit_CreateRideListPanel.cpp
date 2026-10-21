// Test library for the ride list panel (unit v5010; libtpw_unit_CreateRideListPanel.dylib, tests/test_CreateRideListPanel.py), main app:
//   src/game/ui/ui_ridepanel.cpp   CreateRideListPanel and the globals nothing else names
//
// Every callee is a fake that records what the real one reads and answers from the test's queues or tables. The windows are blocks with a
// vtable whose SetText slot records; GetWindow answers from a link table the test fills; objects the fakes return are named pointers.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

#include <cstdarg>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gGameWindow;
CWorld* gGameWorld;
char16_t gUIMainConvertBuffer[0x4000];
// CText's constructor lives in Text.cpp: raw zeroed storage under gText's name (an asm label on a definition is not honoured here).
static_assert(sizeof(CText) <= 0x10000, "gText storage");
__asm__(".globl _gText\n.zerofill __DATA,__bss,_gText,65536,4\n.text\n");

namespace {

struct Named {
    char name[24];
    void* pointer;
};
Named gNamed[48];
int gNamedCount = 0;

struct Link {
    void* parent;
    int32_t id;
    void* child;
};
Link gLinks[64];
int gLinkCount = 0;

void* NamedPointer(const char* name) {
    for (int i = 0; i < gNamedCount; i++)
        if (std::strcmp(gNamed[i].name, name) == 0) return gNamed[i].pointer;
    return nullptr;
}

// The slot of the window's vtable that the original calls through (+0x2c on the Mac: the tenth virtual, SetText).
struct FakeWindow {
    virtual void DrawFunction() {}
    virtual void CalcClientArea() {}
    virtual ~FakeWindow() {}
    virtual int32_t WhatAmI() { return 0; }
    virtual void ClientDraw() {}
    virtual int16_t Enable(int16_t) { return 0; }
    virtual int32_t Minus(void*) { return 0; }
    virtual int32_t SetValue(int32_t) { return 0; }
    virtual int32_t GetValue() { return 0; }
    virtual int32_t SetText(char16_t* text) {
        ev("SetText", {lab(this)}, {narrow(text).c_str()});
        return 1;
    }
    virtual char16_t* GetText() { return nullptr; }
    virtual int32_t GetHelpText() { return 0; }
};

std::string Narrow8(const char* text) { return text == nullptr ? std::string() : std::string(text); }

}  // namespace

// ---- the constructors: no vtable is written (the classes' own files are not in this library), the call is only recorded
extern "C" {
void tpw_crl_fake_title(void* self, void* data) {
    ev("InterfaceTitleTextObject", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}
void tpw_crl_fake_sprite(void* self, void* data) {
    ev("InterfaceDrawObjectSprite", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}
void tpw_crl_fake_textsprite(void* self, void* data) {
    ev("InterfaceTextObjectSprite", {newlab(self), static_cast<int64_t>(reinterpret_cast<intptr_t>(data))});
}
}
// The Itanium names of the three constructors (complete and base object), branching to the fakes above.
#define TPW_CTOR_ALIAS(MANGLED, TARGET) \
    __asm__(".text\n.globl " MANGLED "\n.p2align 2\n" MANGLED ":\n\tb _" TARGET "\n")
// The tag's vtable (the Mac's import __vt__13TbIntegerVTag; its Format is never called here): offset, type info, Format.
__asm__(".section __DATA,__data\n.globl __ZTV13TbIntegerVTag\n.p2align 3\n__ZTV13TbIntegerVTag:\n\t.quad 0\n\t.quad 0\n\t.quad 0\n.text\n");
TPW_CTOR_ALIAS("__ZN24InterfaceTitleTextObjectC1EPv", "tpw_crl_fake_title");
TPW_CTOR_ALIAS("__ZN24InterfaceTitleTextObjectC2EPv", "tpw_crl_fake_title");
TPW_CTOR_ALIAS("__ZN25InterfaceDrawObjectSpriteC1EPv", "tpw_crl_fake_sprite");
TPW_CTOR_ALIAS("__ZN25InterfaceDrawObjectSpriteC2EPv", "tpw_crl_fake_sprite");
TPW_CTOR_ALIAS("__ZN25InterfaceTextObjectSpriteC1EPv", "tpw_crl_fake_textsprite");
TPW_CTOR_ALIAS("__ZN25InterfaceTextObjectSpriteC2EPv", "tpw_crl_fake_textsprite");

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void FillRideList(UIObjType type, int32_t refresh) { ev("FillRideList", {static_cast<int32_t>(type), refresh}); }
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
bool UIMain_CheckPanel() {
    ev("UIMain_CheckPanel", {});
    return ret("UIMain_CheckPanel") != 0;
}
void UIMain_SetPanel(InterfaceWindow* window) { ev("UIMain_SetPanel", {lab(window)}); }
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("UIMain_SetTitleText", {lab(window), id}, {narrow(text).c_str()});
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(message)}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    char name[16];
    std::snprintf(name, sizeof name, "font%u", index);
    return static_cast<InterfaceFont*>(NamedPointer(name));
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    char name[16];
    std::snprintf(name, sizeof name, "str%x", id);
    return static_cast<const char16_t*>(NamedPointer(name));
}
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), a, b, c, d, e}, {narrow(text).c_str()});
}
int32_t RidePanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t RideListLCHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceWindow* RideListLCFormat() { return nullptr; }
void RideListLCPostFormat(InterfaceWindow*) {}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (int i = 0; i < gLinkCount; i++)
        if (gLinks[i].parent == this && gLinks[i].id == id) return static_cast<InterfaceWindow*>(gLinks[i].child);
    return nullptr;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    int64_t script = description == gRideListScript ? 5 : 0;
    int64_t handler = callback == RidePanelMsg ? 1 : 0;
    ev("Build", {lab(this), script, handler});
    return static_cast<InterfaceWindow*>(NamedPointer("buildresult"));
}
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) { ev("SetFontWrap", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    ev("SetClientDrawText", {lab(this), static_cast<int64_t>(reinterpret_cast<intptr_t>(text))});
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == RideListLCHandler ? 2 : 0});
}

int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("CheckButtons_SetCurrent", {lab(this), id});
    return 0;
}

int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), data});
    return static_cast<int32_t>(ret("FindData"));
}
void InterfaceListControl2::SetCurrent(int32_t position) { ev("ListControl_SetCurrent", {lab(this), position}); }
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), makeRow == RideListLCFormat ? 3 : 0, attach == RideListLCPostFormat ? 4 : 0});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), column, descending ? 1 : 0}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {Narrow8(name).c_str()});
    return static_cast<InterfaceDrawObject*>(NamedPointer("selection"));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return static_cast<CBankAccount*>(NamedPointer("bank"));
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}

// The variadic argument is a pointer to the tag (the Mac passes the address of the inline temporary in r7).
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list arguments;
    va_start(arguments, id);
    TbIntegerVTag* tag = va_arg(arguments, TbIntegerVTag*);
    va_end(arguments);
    ev("FormatString", {lab(this), out == gUIMainConvertBuffer ? 1 : 0, static_cast<int64_t>(size), static_cast<int64_t>(id),
                        static_cast<int64_t>(tag->mMagic), static_cast<int64_t>(tag->mId), static_cast<int64_t>(tag->mValue), 1});
    std::string text = "F" + std::to_string(id) + ":" + std::to_string(tag->mValue);
    for (size_t i = 0; i <= text.size() && i < size; i++) out[i] = static_cast<char16_t>(i < text.size() ? text[i] : 0);
    return 0;
}

extern "C" {

uint64_t tpw_crl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"buffer", gUIMainConvertBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_crl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A window: a zeroed block of the host's size with the fake vtable.
uint64_t tpw_crl_newwin() {
    static FakeWindow prototype;
    void* block = tpw::ArenaAllocZeroed(sizeof(InterfaceWindow) + 16);
    std::memcpy(block, static_cast<void*>(&prototype), sizeof(void*));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

void tpw_crl_reset() {
    gNamedCount = 0;
    gLinkCount = 0;
}

void tpw_crl_setnamed(const char* name, uint64_t pointer) {
    for (int i = 0; i < gNamedCount; i++)
        if (std::strcmp(gNamed[i].name, name) == 0) {
            gNamed[i].pointer = reinterpret_cast<void*>(pointer);
            return;
        }
    std::snprintf(gNamed[gNamedCount].name, sizeof gNamed[gNamedCount].name, "%s", name);
    gNamed[gNamedCount].pointer = reinterpret_cast<void*>(pointer);
    gNamedCount++;
}

void tpw_crl_link(uint64_t parent, int32_t id, uint64_t child) {
    gLinks[gLinkCount++] = {reinterpret_cast<void*>(parent), id, reinterpret_cast<void*>(child)};
}

// The pointer globals, by name.
void tpw_crl_setptr(const char* name, uint64_t pointer) {
    if (std::strcmp(name, "gGameWindow") == 0) gGameWindow = reinterpret_cast<InterfaceWindow*>(pointer);
    else if (std::strcmp(name, "gRideListPanel") == 0) gRideListPanel = reinterpret_cast<InterfaceWindow*>(pointer);
    else if (std::strcmp(name, "gGameWorld") == 0) gGameWorld = reinterpret_cast<CWorld*>(pointer);
    else if (std::strcmp(name, "table") == 0)
        gText.mResourcePointers[6] = (std::remove_reference_t<decltype(gText.mResourcePointers[6])>)pointer;
}

uint64_t tpw_crl_getptr(const char* name) {
    if (std::strcmp(name, "gRideListPanel") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRideListPanel));
    return 0;
}

// The integer globals, by name.
void tpw_crl_setint(const char* name, int64_t value) {
    if (std::strcmp(name, "gRideListType") == 0) gRideListType = static_cast<int32_t>(value);
    else if (std::strcmp(name, "gRideListNoneA") == 0) gRideListNoneA = static_cast<uint16_t>(value);
    else if (std::strcmp(name, "gRideListNoneB") == 0) gRideListNoneB = static_cast<uint16_t>(value);
    else if (std::strcmp(name, "gRideListButtonFont") == 0) gRideListButtonFont = static_cast<int32_t>(value);
    else if (std::strcmp(name, "gRideListSortColumn") == 0) gRideListSortColumn = static_cast<int32_t>(value);
    else if (std::strcmp(name, "gRideListF90") == 0) gRideListF90 = static_cast<int32_t>(value);
    else if (std::strcmp(name, "gRideListF8C") == 0) gRideListF8C = static_cast<int16_t>(value);
    else if (std::strcmp(name, "gRideListF8A") == 0) gRideListF8A = static_cast<int16_t>(value);
    else if (std::strcmp(name, "gRideListF88") == 0) gRideListF88 = static_cast<int16_t>(value);
}

int64_t tpw_crl_getint(const char* name) {
    if (std::strcmp(name, "gRideListF90") == 0) return static_cast<uint32_t>(gRideListF90);
    if (std::strcmp(name, "gRideListF8C") == 0) return static_cast<uint16_t>(gRideListF8C);
    if (std::strcmp(name, "gRideListF8A") == 0) return static_cast<uint16_t>(gRideListF8A);
    if (std::strcmp(name, "gRideListF88") == 0) return static_cast<uint16_t>(gRideListF88);
    return 0;
}

int64_t tpw_crl_lab(uint64_t pointer) { return lab(reinterpret_cast<void*>(pointer)); }

int32_t tpw_crl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    return n;
}

void tpw_crl_create(int32_t type, uint32_t selected) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(selected);
    CreateRideListPanel(static_cast<UIObjType>(type), tag);
}

}  // extern "C"
