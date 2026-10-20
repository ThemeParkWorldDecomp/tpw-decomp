// Unit v51042: UVersion's destructor, the object-state text, the rank text object's Change, the lender list callbacks, the thought and
// name edit handlers (main app, the neighbours of ui_main_panel_UI_Congratulations.cpp).
#include <cstdint>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbITable.h"
#include "Text.h"
#include "UI_Base.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_Postcard.h"
#include "ui_main.h"

// ---- data (contents other than the first colour are not known here; the game's tables are in the image)
uint32_t gUIObjectStateColours[27] = {0xffffffffu};   // 0x102a7780
uint32_t gUIObjectStateStrings[27] = {};              // 0x102a77ec
uint32_t gThoughtState[2] = {};                       // 0x10394a20

namespace {
// 0x1024dd1a: the rank text's formats ("%d"); the second is the string 0x16 bytes on (shown for the value 0x7fffffff).
const char16_t kRankFormat[] = u"%d";
const char16_t kRankFormatNone[] = u"-";
}  // namespace

// ---- weak callees (the virtuals and the runtime's delete; the test library fakes them)
namespace v51042_callee {
__attribute__((weak)) int32_t WindowGetValue(InterfaceWindow* window) { return window->GetValue(); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }
}  // namespace v51042_callee

// 0x10187710 (112 bytes)
SamsUtilities::UVersion* UVersion_Dtor(SamsUtilities::UVersion* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mComment.~UString();   // +0x18
        self->mText.~UString();      // +0x0c
        if (deleteFlag > 0) v51042_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10181018 (148 bytes)
void UIMain_SetObjectStateText(InterfaceWindow* window, int32_t state) {
    uint32_t colour = gUIObjectStateColours[state];
    window->SetFontColour(static_cast<int32_t>(colour >> 24), static_cast<int32_t>((colour >> 16) & 0xff),
                          static_cast<int32_t>((colour >> 8) & 0xff), 0xff);
    const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[6]);   // 0x564
    const unsigned char* text = table->GetConstString(gUIObjectStateStrings[state]);
    v51042_callee::WindowSetText(window, UIMain_ConvertMulti(text));
}

// 0x1018fb08 (156 bytes)
void InterfaceRankTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change) {
    if ((flags & 0x80) != 0) {
        char16_t text[8];
        int32_t value;
        std::memcpy(&value, change->mF00, 4);
        // The original passes the word to both calls.
        if (value == 0x7fffffff)
            swprintf(text, 8, kRankFormatNone, value);
        else
            swprintf(text, 8, kRankFormat, value);
        DOTextStyle style;
        style.mText = text;
        DOChange shown;
        shown.mText = &style;
        self->InterfaceTextObjectSprite::Change(0x100, &shown);
    } else {
        self->InterfaceTextObjectSprite::Change(flags, change);
    }
}

// 0x101a4bec (184 bytes)
int32_t LenderListCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        gLenderListMode = (a == 0x12c4bf) ? 1 : 0;
        FillLenderList(reinterpret_cast<InterfaceListControl2*>(window));
        UpdateListHeadings();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a4ce4 (192 bytes)
int32_t LendersItemsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        if (b < 0)
            window->SetFontColour(0x8c, 0, 0x3c, 0xff);
        else
            window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101ab968 (116 bytes). Message 0xc answers the address of the state (the Mac's pointer, cut to 32 bits here).
int32_t ThoughtMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0xc) {
        int32_t value = v51042_callee::WindowGetValue(window);
        const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[8]);   // 0x56c
        const unsigned char* text = table->GetConstString(static_cast<uint32_t>(value + 1));
        gThoughtState[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(UIMain_ConvertMulti(text)));
        return static_cast<int32_t>(reinterpret_cast<uintptr_t>(gThoughtState));
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101afea4 (132 bytes)
int32_t NameEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1000b && a == 0xd) NewEntryInAddressBook();
    return window->CallDefaultWindowHandler(message, a, b);
}
