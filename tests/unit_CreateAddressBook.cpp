// Test library for the address book panel (unit v5012; libtpw_unit_CreateAddressBook.dylib, tests/test_CreateAddressBook.py), main app:
//   src/game/ui/ui_Postcard.cpp   CreateAddressBook, RemoveAddressBookEntry, NewEntryInAddressBook and the globals they own.
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of other
// files (gPostcardPanel, gUserInfo, gOnlineSystem, gText) are defined here. The constructors of classes whose bases are not linked are
// defined under their mangled names (asm labels), so `new` in the translation reaches them.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "email.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gPostcardPanel;
CFSUserInfo* gUserInfo;
CFSOnlineSystem* gOnlineSystem;
// CText's constructor drags in unlinked font code: the object is raw zeroed storage under gText's symbol.
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");
#define gTextRef (*reinterpret_cast<CText*>(gTextStorage))

namespace {

int64_t Hash16(const char16_t* text) {
    uint32_t h = 2166136261u;
    for (; *text != 0; ++text) h = (h ^ static_cast<uint32_t>(*text)) * 16777619u;
    return h;
}

int64_t HashN(const char* text, size_t length) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < length; ++i) h = (h ^ static_cast<uint8_t>(text[i])) * 16777619u;
    return h;
}

int64_t P(const void* pointer) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu); }

void* gFakeVtable[256];

int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {lab(self)}, {narrow(text).c_str()});
    return static_cast<int32_t>(ret("SetText"));
}

void FakeTrap() {
    std::fprintf(stderr, "FakeTrap hit\n");
    std::abort();
}

void* VtableAddress() {
    static bool ready = false;
    if (!ready) {
        auto member = &InterfaceWindow::SetText;
        uintptr_t words[2];
        std::memcpy(words, &member, sizeof words);
        for (auto& slot : gFakeVtable) slot = reinterpret_cast<void*>(&FakeTrap);
        gFakeVtable[64 + ((words[1] & 1) != 0 ? words[0] : words[0] - 1) / sizeof(void*)] = reinterpret_cast<void*>(&FakeSetText);
        ready = true;
    }
    return &gFakeVtable[64];
}

}  // namespace

// ---- untranslated neighbours the unit points at (only their addresses matter)
int32_t AddressBookMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t NameEditMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t DeleteEntryOK(void*) { return 0; }
InterfaceWindow* PostcardLCFormat() { return nullptr; }
void PostcardLCPostFormat(InterfaceWindow*) {}

void FillAddressList(int32_t flag) { ev("FillAddressList", {flag}); }
void AddToAddressField(char16_t* text) { ev("AddToAddressField", {lab(text), Hash16(text)}); }

// ---- constructors under their mangled names
void FakeUserInfoCtor(CFSUserInfo* self) __asm__("__ZN11CFSUserInfoC1Ev");
void FakeUserInfoCtor(CFSUserInfo* self) { ev("CFSUserInfo", {newlab(self)}); }
void FakeOnlineCtor(CFSOnlineSystem* self) __asm__("__ZN15CFSOnlineSystemC1Ev");
void FakeOnlineCtor(CFSOnlineSystem* self) { ev("CFSOnlineSystem", {newlab(self)}); }
void FakeTitleCtor(InterfaceTitleTextObject* self, void* data) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
void FakeTitleCtor(InterfaceTitleTextObject* self, void* data) { ev("InterfaceTitleTextObject", {newlab(self), P(data)}); }
void FakeCursorCtor(InterfaceCursorPosTextObjectSprite* self, void* data) __asm__("__ZN34InterfaceCursorPosTextObjectSpriteC1EPv");
void FakeCursorCtor(InterfaceCursorPosTextObjectSprite* self, void* data) {
    ev("InterfaceCursorPosTextObjectSprite", {newlab(self), P(data)});
}

// ---- interface fakes
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), lab(description), lab(reinterpret_cast<void*>(callback))});
    return reinterpret_cast<InterfaceWindow*>(ret("Build"));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(ret("GetWindow", id, 0));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(ret("Font", static_cast<int64_t>(index), 0));
}

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), P(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(ret("GetString", id, 0));
}

void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {lab(window)}); }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }

void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), lab(reinterpret_cast<void*>(makeRow)), lab(reinterpret_cast<void*>(attach))});
}

void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {static_cast<const char*>(name)});
    return reinterpret_cast<InterfaceDrawObject*>(ret("Find"));
}

void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), lab(reinterpret_cast<void*>(callback))});
}

int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {lab(this), lab(text), capacity});
    return static_cast<int32_t>(ret("SetStaticTextArea"));
}

void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {lab(this), lab(object)}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("UIMain_AdvisorMessage", {static_cast<int32_t>(message)}); }

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetDataValue"));
    return static_cast<int32_t>(ret("GetData"));
}

void InterfaceListControl2::SetCurrent(int32_t position) { ev("SetCurrent", {lab(this), position}); }

// ---- the address book
AddressBookNode** AddressBook::First() {
    char* owner = reinterpret_cast<char*>(this) - offsetof(CFSUserInfo, mAddressBook);
    ev("First", {lab(owner)});
    return reinterpret_cast<AddressBookNode**>(ret("First"));
}

AddressBookNode** AddressBook::Next() {
    char* owner = reinterpret_cast<char*>(this) - offsetof(CFSUserInfo, mAddressBook);
    ev("Next", {lab(owner)});
    return reinterpret_cast<AddressBookNode**>(ret("Next"));
}

void AddressBook::Add(const std::string& address) {
    char* owner = reinterpret_cast<char*>(this) - offsetof(CFSUserInfo, mAddressBook);
    ev("Add", {lab(owner), HashN(address.data(), address.size()), static_cast<int64_t>(address.size())});
}

CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() {
    ev("GetFTPDownloadParkInfo", {lab(this)});
    return reinterpret_cast<CFSFTPParkInfo*>(ret("GetFTPDownloadParkInfo"));
}

// ---- text
int swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list args;
    va_start(args, format);
    const char* text = va_arg(args, const char*);
    va_end(args);
    size_t length = std::strlen(text);
    ev("swprintf", {lab(buffer), static_cast<int64_t>(size), Hash16(format), HashN(text, length), static_cast<int64_t>(length)});
    size_t out = length < size - 1 ? length : size - 1;
    for (size_t i = 0; i < out; ++i) buffer[i] = static_cast<uint8_t>(text[i]);
    buffer[out] = 0;
    return static_cast<int>(out);
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    const TbUniStringVTag* tag = va_arg(args, const TbUniStringVTag*);
    va_end(args);
    size_t length = 0;
    while (tag->mText[length] != 0) ++length;
    ev("FormatString", {lab(const_cast<TbITable*>(this)), static_cast<int64_t>(size), static_cast<int64_t>(id), tag->mMagic, tag->mId,
                        Hash16(tag->mText), static_cast<int64_t>(length)});
    std::u16string text = u"F";
    for (char c : std::to_string(id)) text.push_back(static_cast<char16_t>(c));
    text.push_back(u':');
    text.append(tag->mText, length);
    size_t count = size == 0 ? 0 : size - 1;
    if (text.size() > count) text.resize(count);
    std::memcpy(out, text.data(), text.size() * 2);
    out[text.size()] = 0;
    return static_cast<int32_t>(text.size());
}

int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    int64_t rgb = (static_cast<int64_t>(params.mRed) << 16) | (params.mGreen << 8) | params.mBlue;
    int64_t valueC = static_cast<int32_t>(button1) == 2 ? params.mValueC : 0;
    ev("UI_DialogBox", {Hash16(params.mText), rgb, params.mValue8, valueC, static_cast<int32_t>(button1),
                        lab(reinterpret_cast<void*>(callback1)), static_cast<int32_t>(button2), lab(reinterpret_cast<void*>(callback2)),
                        static_cast<int32_t>(button3), lab(reinterpret_cast<void*>(callback3)), flag ? 1 : 0});
    return nullptr;
}

extern "C" {

uint64_t tpw_ab_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"desc", gAddressBookDescription},
        {"msgAB", reinterpret_cast<const void*>(&AddressBookMsg)},
        {"msgName", reinterpret_cast<const void*>(&NameEditMsg)},
        {"lcf", reinterpret_cast<const void*>(&PostcardLCFormat)},
        {"lcp", reinterpret_cast<const void*>(&PostcardLCPostFormat)},
        {"delok", reinterpret_cast<const void*>(&DeleteEntryOK)},
        {"editbuf", gAddressEditText},
        {"rembuf", gAddressRemoveName},
        {"ppanel", &gPostcardPanel},
        {"bpanel", &gAddressBookPanel},
        {"userinfo", &gUserInfo},
        {"online", &gOnlineSystem},
        {"tslot", &gTextRef.mResourcePointers[6]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ab_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ab_vptr() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(VtableAddress())); }

int32_t tpw_ab_layout(int64_t* out) {
    alignas(16) static char listBuffer[0x800];
    alignas(16) static char windowBuffer[0x800];
    alignas(16) static char userBuffer[0x800];
    int32_t n = 0;
    auto* list = reinterpret_cast<InterfaceListControl2*>(listBuffer);
    auto* window = reinterpret_cast<InterfaceWindow*>(windowBuffer);
    auto* user = reinterpret_cast<CFSUserInfo*>(userBuffer);
    out[n++] = reinterpret_cast<char*>(&list->mCurrentEntry) - listBuffer;
    out[n++] = reinterpret_cast<char*>(&window->mZ) - windowBuffer;
    out[n++] = reinterpret_cast<char*>(&user->mAddressBook) - userBuffer;
    out[n++] = static_cast<int64_t>(sizeof(AddressBookNode));
    AddressBookNode node;
    out[n++] = reinterpret_cast<char*>(&node.mText) - reinterpret_cast<char*>(&node);
    return n;
}

void tpw_ab_create() { CreateAddressBook(); }
void tpw_ab_remove() { RemoveAddressBookEntry(); }
void tpw_ab_new() { NewEntryInAddressBook(); }

}  // extern "C"
