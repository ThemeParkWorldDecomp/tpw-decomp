// The text objects of the main panel's number windows, Change (main app, unit w125): InterfaceNumberTextObject::Change 0x1018db00 (244 bytes)
// and InterfaceCashAndGTickTextObject::Change 0x1018e36c (476 bytes). Both turn the number they are given into text and hand the text to
// InterfaceTextObjectSprite::Change with flag 0x100 (the text flag) through a DOChange and a DOTextStyle that live on the stack (nothing but the
// style's text pointer and the change's style pointer is set; the sprite's Change reads nothing else for flag 0x100). Without the flag they
// tested for, the call goes on to InterfaceTextObjectSprite::Change with the SAME flags and change.
#include <cstdint>
#include <cstring>

#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"

namespace {

// The format strings (UTF-16 at 0x1024dd1a: "%d", then at +6 bytes "%.2f %%"; the Mac reads them through the pointer at 0x102639ac).
const char16_t kIntegerFormat[] = u"%d";
const char16_t kPercentFormat[] = u"%.2f %%";

// The first word of a DOChange: the value both objects show (the Mac reads it as a signed int; DOChange.mF00 holds the word in host byte order here).
int32_t ChangeValue(const DOChange* change) {
    int32_t value;
    std::memcpy(&value, change->mF00, sizeof value);
    return value;
}

// 0xeeeeeee: what the cash window shows in full; beyond it (above or below) the golden ticket text takes over (the listing's 0xeef0000 - 0x1112).
constexpr int32_t kCashLimit = 0xeeeeeee;

// labs of the imported c_c++ library (0x1001c9f0): neg when below 0, so INT_MIN stays INT_MIN.
int32_t Labs(int32_t value) {
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}

}  // namespace

// flag 0x80: the first word as "%d" (a buffer of 0x20 characters). Else flag 8: that word as a float, times 2^-10 and then by 100 (two separate
// single-precision multiplies, in that order: the first rounds to float), shown as a double through "%.2f %%" (a buffer of 0x40 characters).
// Flag 0x80 is tested first, so both flags show the number. Any other flags go to the sprite's Change as they came.
void InterfaceNumberTextObject::Change(uint32_t flags, DOChange* change) {
    if ((flags & 0x80) != 0) {
        char16_t text[0x20];
        swprintf(text, 0x20, kIntegerFormat, ChangeValue(change));
        DOTextStyle style;
        style.mText = text;
        DOChange shown;
        shown.mText = &style;
        InterfaceTextObjectSprite::Change(0x100, &shown);
    } else if ((flags & 8) != 0) {
        char16_t text[0x40];
        const float number = static_cast<float>(ChangeValue(change));
        const float scaled = number * 0.0009765625f;
        const float percent = 100.0f * scaled;
        swprintf(text, 0x40, kPercentFormat, static_cast<double>(percent));
        DOTextStyle style;
        style.mText = text;
        DOChange shown;
        shown.mText = &style;
        InterfaceTextObjectSprite::Change(0x100, &shown);
    } else {
        InterfaceTextObjectSprite::Change(flags, change);
    }
}

// Flag 0x80: the first word `value` is shown by string `id` of the game text's table (resource 6, gText +0x564) as an integer tag (buffer of
// 0x3f characters + the terminator): from -0xeeeeeee to 0xeeeeeee (the absolute value, the first labs, at most the limit) it is
// id 0x1c1 with the value (kind 0xd) when not negative and id 0x1c7 with the negated value (kind 0x17) when negative. Past the limit (the compare is
// signed: INT_MIN counts as within it) a negative value takes id 0x1c2 with labs(value + 0xeeeeeee) (kind 0x13, the second labs call) and a
// positive one id 0x1c1 with value - 0xeeeeeee (kind 0xd). Without the flag the call goes to the sprite's Change as it came.
void InterfaceCashAndGTickTextObject::Change(uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    char16_t text[0x44];
    const int32_t value = ChangeValue(change);
    TbITable* const table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    if (Labs(value) <= kCashLimit) {
        if (value >= 0) {
            TbIntegerVTag tag(0xd, value);
            table->FormatString(text, 0x3f, 0x1c1, static_cast<TbVTag*>(&tag), nullptr);
        } else {
            TbIntegerVTag tag(0x17, static_cast<int32_t>(0u - static_cast<uint32_t>(value)));
            table->FormatString(text, 0x3f, 0x1c7, static_cast<TbVTag*>(&tag), nullptr);
        }
    } else if (value < 0) {
        TbIntegerVTag tag(0x13, Labs(static_cast<int32_t>(static_cast<uint32_t>(value) + static_cast<uint32_t>(kCashLimit))));
        table->FormatString(text, 0x3f, 0x1c2, static_cast<TbVTag*>(&tag), nullptr);
    } else {
        TbIntegerVTag tag(0xd, static_cast<int32_t>(static_cast<uint32_t>(value) - static_cast<uint32_t>(kCashLimit)));
        table->FormatString(text, 0x3f, 0x1c1, static_cast<TbVTag*>(&tag), nullptr);
    }
    DOTextStyle style;
    style.mText = text;
    DOChange shown;
    shown.mText = &style;
    InterfaceTextObjectSprite::Change(0x100, &shown);
}
