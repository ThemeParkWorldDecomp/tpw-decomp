// Test library for unit v51042 (libtpw_unit_SamsUtilities_UVersion_Dtor_UVersion_2.dylib, tests/test_SamsUtilities_UVersion_Dtor_UVersion_2.py), main app:
//   src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_main.cpp, src/game/ui/ui_parkStatus.cpp   UVersion_Dtor, UIMain_SetObjectStateText,
//   InterfaceRankTextObject_Change, LenderListCallback, LendersItemsCallback, ThoughtMsg, NameEditMsg
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The globals of
// other files (gText, gLenderListMode) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbITable.h"
#include "Text.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_Postcard.h"
#include "ui_main.h"
#include "image_copies.h"

// The tables as loaded, for the image test (unit v5L39).
TPW_IMAGE_COPY(colours, gUIObjectStateColours);
TPW_IMAGE_COPY(strings, gUIObjectStateStrings);

using namespace calllog;

// ---- globals of other files, defined here
int32_t gLenderListMode;                                                       // 0x1039497c
unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");                  // 0x1037ff18 (no constructor runs)

namespace {
int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// ---- the fakes
void FillLenderList(InterfaceListControl2* list) { ev("FillLenderList", {lab(list)}); }
void UpdateListHeadings() { ev("UpdateListHeadings", {}); }
void NewEntryInAddressBook() { ev("NewEntryInAddressBook", {}); }

char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("ConvertMulti", {P(text)});
    return reinterpret_cast<char16_t*>(static_cast<uintptr_t>(ret("ConvertMulti")));
}

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {P(this), static_cast<int64_t>(id)});
    return reinterpret_cast<const unsigned char*>(static_cast<uintptr_t>(ret("GetConstString")));
}

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}

void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    int64_t c[8] = {};
    if (flags == 0x100 && change != nullptr && change->mText != nullptr && change->mText->mText != nullptr)
        for (int i = 0; i < 8 && change->mText->mText[i] != 0; ++i) c[i] = change->mText->mText[i];
    ev("Change", {lab(this), static_cast<int64_t>(flags), lab(change), c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]});
}

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t value = va_arg(ap, int32_t);
    va_end(ap);
    int64_t f[9] = {};
    for (int i = 0; i < 9 && format[i] != 0; ++i) f[i] = format[i];
    ev("swprintf", {static_cast<int64_t>(size), static_cast<int64_t>(static_cast<uint32_t>(value)), f[0], f[1], f[2], f[3], f[4], f[5],
                    f[6], f[7], f[8]});
    uint32_t n = 0;
    while (format[n] != 0 && n + 1 < size) {
        buffer[n] = format[n];
        ++n;
    }
    buffer[n] = 0;
    return 0;
}

SamsUtilities::UString::~UString() { ev("UStringDtor", {lab(this)}); }

namespace v51042_callee {
int32_t WindowGetValue(InterfaceWindow* window) {
    ev("WindowGetValue", {lab(window)});
    return static_cast<int32_t>(ret("WindowGetValue"));
}
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("WindowSetText", {lab(window), static_cast<int64_t>(P(text) & 0xffffffff)}); }
void OperatorDelete(void* block) { ev("OperatorDelete", {lab(block)}); }
}  // namespace v51042_callee

extern "C" {

uint64_t tpw_v51042_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    CText* text = reinterpret_cast<CText*>(gTextStorage);
    const Entry entries[] = {
        {"colours", gUIObjectStateColours}, {"strings", gUIObjectStateStrings}, {"thought", gThoughtState}, {"mode", &gLenderListMode},
        {"tb6", &text->mResourcePointers[6]}, {"tb8", &text->mResourcePointers[8]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51042_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host addresses of a UVersion's two strings.
uint64_t tpw_v51042_uversion_string(uint64_t object, int32_t which) {
    SamsUtilities::UVersion* v = reinterpret_cast<SamsUtilities::UVersion*>(static_cast<uintptr_t>(object));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(which == 0 ? &v->mText : &v->mComment));
}

int32_t tpw_v51042_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gUIObjectStateColours));
    out[n++] = static_cast<int64_t>(sizeof(gThoughtState));
    out[n++] = static_cast<int64_t>(offsetof(DOChange, mF00));
    return n;
}

uint64_t tpw_v51042_uversion_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        UVersion_Dtor(reinterpret_cast<SamsUtilities::UVersion*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag))));
}
void tpw_v51042_state_text(uint64_t window, int32_t state) {
    UIMain_SetObjectStateText(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), state);
}
void tpw_v51042_rank_change(uint64_t self, uint32_t flags, uint64_t change) {
    InterfaceRankTextObject_Change(reinterpret_cast<InterfaceTextObjectSprite*>(static_cast<uintptr_t>(self)), flags,
                                   reinterpret_cast<DOChange*>(static_cast<uintptr_t>(change)));
}
uint32_t tpw_v51042_lender_list(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(LenderListCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}
uint32_t tpw_v51042_lenders_items(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(LendersItemsCallback(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}
uint32_t tpw_v51042_thought(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(ThoughtMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}
uint32_t tpw_v51042_name_edit(uint64_t w, int32_t m, int32_t a, int32_t b) {
    return static_cast<uint32_t>(NameEditMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(w)), m, a, b));
}

}  // extern "C"
