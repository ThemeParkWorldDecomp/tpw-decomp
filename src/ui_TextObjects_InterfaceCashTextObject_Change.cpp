// Change of the cash, null cash, percent, visitor number and password text objects, and the password object's destructor.
#include <cstdint>
#include <cstring>
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

namespace {

// The format at 0x1024dd1a (taken to be "%d", as the rank text object's).
const char16_t kVisitorFormat[] = u"%d";

TbITable* GameTable() { return static_cast<TbITable*>(gText.mResourcePointers[6]); }   // gText +0x564

int32_t FirstWord(const DOChange* change) {
    int32_t value;
    std::memcpy(&value, change->mF00, 4);
    return value;
}

// Draws `text` through the sprite's Change with flag 0x100 (the DOChange on the stack otherwise unset).
void Show(InterfaceTextObjectSprite* self, char16_t* text) {
    DOTextStyle style;
    style.mText = text;
    DOChange shown;
    shown.mText = &style;
    self->InterfaceTextObjectSprite::Change(0x100, &shown);
}

}  // namespace

// 0x1018e02c: flag 0x80: the signed cash value as string 0x1c1 (kind 0xd), or 0x1c2 with labs(value) (kind 0x13) when negative.
void InterfaceCashTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        self->InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    char16_t text[0x44];
    const int32_t value = FirstWord(change);
    if (value < 0) {
        TbIntegerVTag tag(0x13, textobj_callee::Labs(value));
        GameTable()->FormatString(text, 0x3f, 0x1c2, static_cast<TbVTag*>(&tag), nullptr);
    } else {
        TbIntegerVTag tag(0xd, value);
        GameTable()->FormatString(text, 0x3f, 0x1c1, static_cast<TbVTag*>(&tag), nullptr);
    }
    Show(self, text);
}

// 0x1018dd50: flag 0x100: the password's text is copied to the shared buffer, every character becomes '*', and the change (a copy of
// the first six words) is drawn with that buffer as its text.
void InterfacePasswordTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change) {
    if ((flags & 0x100) == 0) {
        self->InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    char16_t* buffer = gUIMainConvertBuffer;
    wcscpy(buffer, change->mText->mText);
    textobj_callee::WcsSet(buffer, u'*');
    DOChange copy = *change;
    DOTextStyle style;
    style.mText = buffer;
    copy.mText = &style;
    self->InterfaceTextObjectSprite::Change(0x100, &copy);
}

// 0x1018dcb0
InterfacePasswordTextObject* InterfacePasswordTextObject_Dtor(InterfacePasswordTextObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        textobj_callee::CursorDtor(self, 0);
        if (deleteFlag > 0) textobj_callee::Delete(self);
    }
    return self;
}

// 0x1018e768: flag 0x80: an empty text for 0, else string 0x1c1 with the value (kind 0xd).
void InterfaceNullCashTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        self->InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    char16_t text[0x44];
    const int32_t value = FirstWord(change);
    if (value == 0) {
        text[0] = 0;
    } else {
        TbIntegerVTag tag(0xd, value);
        GameTable()->FormatString(text, 0x3f, 0x1c1, static_cast<TbVTag*>(&tag), nullptr);
    }
    Show(self, text);
}

// 0x1018ea44: flag 0x80: string 0x1c3 with the value (kind 0x14).
void InterfacePercentTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        self->InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    char16_t text[0x44];
    TbIntegerVTag tag(0x14, FirstWord(change));
    GameTable()->FormatString(text, 0x3f, 0x1c3, static_cast<TbVTag*>(&tag), nullptr);
    Show(self, text);
}

// 0x1018ed10: flag 0x80: string 0x1c9 for 0, else swprintf(text, 0x40, format, value).
void InterfaceVisitorNumTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        self->InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    char16_t text[0x44];
    const int32_t value = FirstWord(change);
    if (value == 0) {
        GameTable()->FormatString(text, 0x3f, 0x1c9);
    } else {
        swprintf(text, 0x40, kVisitorFormat, value);
    }
    Show(self, text);
}
