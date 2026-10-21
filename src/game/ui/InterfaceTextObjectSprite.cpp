// InterfaceTextObjectSprite.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceTextObjectSprite code, named after it.

#define TPW_UIMENU_CTOR_EXTERNAL
#include "analyser.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "FS_UserInfo.h"
#include "helpers/InterfaceTextObjectSpriteHelpers.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "LbPalette.h"
#include "LLCellSystem.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "Ptr32.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "std_basic_filebuf.h"
#include "TbColour.h"
#include "TbDynamicStringTemplate.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbIRLE4bitFont.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_Postcard.h"
#include "WorldSavvy.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// InterfaceTextObjectSprite constructor, main 0x1018d11c (136 bytes); existing body separated for unit w43.

InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {
    mJustify = 0;
    mVJustify = 1;
    mWrap = 0;
    mF0DF = 0;
    mOffsetX = 0;
    mOffsetY = 0;
    mFontId = -1;
    mColourMode = 0;
    TextColour()[0] = 0xff;
    TextColour()[1] = 0xff;
    TextColour()[2] = 0xff;
}

// The text draw objects (main app of SimTheme Park 2.04b3, 0x1018d11c..0x101902a8; the inventory files the range under
// ui_textmanager.cpp): InterfaceTextObjectSprite (constructor, destructor, UpdateTextures, Change), InterfaceCursorPosTextObjectSprite
// (constructor, destructor, Query) and InterfaceTitleTextObject (constructor, destructor, UpdateTextures). See include/ui_DrawObject.h for
// the classes. The other text objects of the game (cash, number, rank, percent...) are further subclasses made elsewhere.

CoolRect gUIQueryRect;

// ---- InterfaceTextObjectSprite

// Left justified, vertically centred, no wrap, no font, the one colour, no offsets, the text colour white.

// (The formatter, the list and the surface are destroyed by the members' destructors, in that order, before the sprite object's base.)
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}

// Draws `text` in white on a cleared surface (the sprite's colour word tints it); nothing without a text. The font is the object's; the
// colour is the one white of the palette (found each time) when the colour mode is 0, else whatever the font parameters have.
void InterfaceTextObjectSprite::UpdateTextures(char16_t* text) {
    TbRenderArea area;
    TbRect bounds = {0, 0, 0, 0};
    if (text == nullptr) return;
    const TextBox box = Justify(*this);
    gText.mRenderParams.SetFont(FontOf(*this));
    if (mColourMode == 0) {
        const uint8_t index = LbPalette_FindColour(&_lbGlobalPalette, 0xff, 0xff, 0xff);
        const TbColour white = MakeColour(0xff, 0xff, 0xff, 0xff, index);
        gText.mRenderParams.UseReColour(white);
    }
    PrepareSurface(*this, area);
    const TbRect rect = {box.mLeft, box.mTop, box.mRight, box.mBottom};
    mFormatter.Draw(rect, UIMain_ConvertString(text), gText.mRenderParams, &bounds);
    FinishSurface(*this, area, bounds);
}

void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    if (change == nullptr || flags == 0) return;
    InterfaceDrawObjectSprite::Change(flags, change);
    if ((flags & 0x7fd00) == 0) return;
    const DOTextStyle* style = change->mText;
    if (style == nullptr) return;
    if ((flags & 0x400) != 0) mJustify = static_cast<int8_t>(style->mF04);
    if ((flags & 0x800) != 0) mVJustify = static_cast<int8_t>(style->mF05);
    if ((flags & 0x1000) != 0) mWrap = style->mF06[0];
    if ((flags & 0x2000) != 0) mF0DF = style->mF06[1];
    if ((flags & 0x4000) != 0) std::memcpy(&mOffsetX, &style->mF0A[0], sizeof mOffsetX);
    if ((flags & 0x8000) != 0) std::memcpy(&mOffsetY, &style->mF0A[2], sizeof mOffsetY);
    if ((flags & 0x10000) != 0) mFontId = static_cast<int8_t>(style->mF06[2]);
    if ((flags & 0x20000) != 0) mColourMode = style->mF09;
    if ((flags & 0x40000) != 0) {
        uint8_t* colour = TextColour();
        colour[0] = style->mColour[0];
        colour[1] = style->mColour[1];
        colour[2] = style->mColour[2];
        colour[3] = style->mF13;
        mSprite.mF24 = static_cast<int32_t>((static_cast<uint32_t>(colour[0]) << 24) | (static_cast<uint32_t>(colour[1]) << 16) |
                                            (static_cast<uint32_t>(colour[2]) << 8) | colour[3]);
    }
    if ((flags & 0x100) != 0 && style->mText != nullptr) UpdateTextures(style->mText);
}

// The text objects of the main panel's number windows (Mac file ui_textmanager.cpp; unit w118): InterfaceNumberTextObject constructor
// 0x1018d930 (164 bytes) and destructor 0x1018da0c (188), InterfaceCashAndGTickTextObject constructor 0x1018e190 (164) and destructor
// 0x1018e274 (188). Their Change (0x1018db00 and 0x1018e36c) is in src/ui_TextObjects.cpp.
//
// The Mac constructors are InterfaceTextObjectSprite's (0x1018d11c, src/ui_DrawObjectText_ctor.cpp) with the base constructor inlined, in
// the same order, and the class's own vtable stored last; the host chains to that constructor. The destructors are the sprite chain
// inlined (the formatter, the sprite list, the engine data, the draw object), which the language's member destruction repeats.

InterfaceNumberTextObject::InterfaceNumberTextObject(void* data) : InterfaceTextObjectSprite(data) {}

InterfaceNumberTextObject::~InterfaceNumberTextObject() {}

// The text objects of the main panel's number windows, Change (main app, unit w125): InterfaceNumberTextObject::Change 0x1018db00 (244 bytes)
// and InterfaceCashAndGTickTextObject::Change 0x1018e36c (476 bytes). Both turn the number they are given into text and hand the text to
// InterfaceTextObjectSprite::Change with flag 0x100 (the text flag) through a DOChange and a DOTextStyle that live on the stack (nothing but the
// style's text pointer and the change's style pointer is set; the sprite's Change reads nothing else for flag 0x100). Without the flag they
// tested for, the call goes on to InterfaceTextObjectSprite::Change with the SAME flags and change.

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

// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.

// 0x1018dc38: the cursor position text object's constructor, then its own vtable (0x102a81ac).
InterfacePasswordTextObject::InterfacePasswordTextObject(void* data) : InterfaceCursorPosTextObjectSprite(data) {}

// 0x1018de60 (unit v5L16), 0x1018e594, 0x1018e874, 0x1018eb38, 0x1018edf4, 0x1018f93c: the text sprite's constructor (sprite
// constructor, formatter, justification 0, 1, wrap 0, offsets 0, font -1, colour mode 0, colour 0xff 0xff 0xff), then each class's
// vtable.
InterfaceCashTextObject::InterfaceCashTextObject(void* data) : InterfaceTextObjectSprite(data) {}
// 0x1018df3c, 0x1018e674, 0x1018e950, 0x1018ec18, 0x1018eed4: the vtables stepped down, the formatter, the sprite list, the engine data and
// the draw object destroyed in that order (the host's members and bases are destroyed in the same order); the delete flag is the caller's.
InterfaceCashTextObject::~InterfaceCashTextObject() {}

// Change of the cash, null cash, percent, visitor number and password text objects, and the password object's destructor.

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

// labs (glue 0x10239928): neg when below 0, so INT_MIN stays INT_MIN. Weak: the test library fakes it. (Unit v5L33: it had no definition,
// so the launcher's generated stand-in answered 0 and a negative balance showed as "-$ 0".)
__attribute__((weak)) int32_t textobj_callee::Labs(int32_t value) {
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}

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

// The classes' own Change (each Mac vtable's Change slot holds the function above): InterfaceWindow::SetValue reaches the text through these.
void InterfaceCashTextObject::Change(uint32_t flags, DOChange* change) { InterfaceCashTextObject_Change(this, flags, change); }

InterfaceCashAndGTickTextObject::InterfaceCashAndGTickTextObject(void* data) : InterfaceTextObjectSprite(data) {}

InterfaceCashAndGTickTextObject::~InterfaceCashAndGTickTextObject() {}

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

InterfaceNullCashTextObject::InterfaceNullCashTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceNullCashTextObject::~InterfaceNullCashTextObject() {}

void InterfaceNullCashTextObject::Change(uint32_t flags, DOChange* change) { InterfaceNullCashTextObject_Change(this, flags, change); }

InterfacePercentTextObject::InterfacePercentTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfacePercentTextObject::~InterfacePercentTextObject() {}

void InterfacePercentTextObject::Change(uint32_t flags, DOChange* change) { InterfacePercentTextObject_Change(this, flags, change); }

InterfaceVisitorNumTextObject::InterfaceVisitorNumTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceVisitorNumTextObject::~InterfaceVisitorNumTextObject() {}

void InterfaceVisitorNumTextObject::Change(uint32_t flags, DOChange* change) { InterfaceVisitorNumTextObject_Change(this, flags, change); }

InterfaceVisitorTimeTextObject::InterfaceVisitorTimeTextObject(void* data) : InterfaceTextObjectSprite(data) {}
InterfaceVisitorTimeTextObject::~InterfaceVisitorTimeTextObject() {}

// Unit v51012: the rest of the interface's ui_utils.cpp helpers (main app of SimTheme Park 2.04b3):
//   UI_DrawPostcardText 0x1018bbc4, __sinit_ui_utils_cpp 0x1018bda8, MakeRenderAreaFromRAW 0x1018b434,
//   InterfaceDrawEngineHGradient::CalcUVs 0x10189d70, InterfaceVisitorTimeTextObject::Change 0x1018efcc.

// 0x1018efcc: with flag 0x80 the text is made from the time (in hours, as the first word of the change): string 0x1b9 for up to
// 24 (the number itself, negative ones as their magnitude with the flag set), 0x1ba up to 48 and 0x1bb above, those two with the
// number divided by 24 (the days). Then the text object draws it (flag 0x100). Any other flags go straight to the base.
void InterfaceVisitorTimeTextObject::Change(uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    const int32_t time = static_cast<int32_t>((static_cast<uint32_t>(change->mF00[0]) << 24) |
                                              (static_cast<uint32_t>(change->mF00[1]) << 16) |
                                              (static_cast<uint32_t>(change->mF00[2]) << 8) | change->mF00[3]);
    char16_t text[0x44];
    TbITable* const table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    if (time > 0x18) {
        const uint32_t days = static_cast<uint32_t>((static_cast<uint64_t>(0xaaaaaaabu) * static_cast<uint32_t>(time)) >> 32) >> 4;
        RideAgeTag tag(static_cast<int32_t>(days), 0);
        tag.mId = 8;
        table->FormatString(text, 0x3f, time > 0x30 ? 0x1bb : 0x1ba, static_cast<TbVTag*>(&tag));
    } else {
        RideAgeTag tag(time, 0);
        if (time < 0) {
            tag.mValue = static_cast<int32_t>(0u - static_cast<uint32_t>(time));
            tag.mNegative = 1;
        }
        tag.mId = 0xb;
        table->FormatString(text, 0x3f, 0x1b9, static_cast<TbVTag*>(&tag));
    }
    DOTextStyle style;
    style.mText = text;
    DOChange shown;
    shown.mText = &style;
    InterfaceTextObjectSprite::Change(0x100, &shown);
}

// ---- InterfaceCursorPosTextObjectSprite

InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite(void* data) : InterfaceTextObjectSprite(data) {
    mFormatter.mMeasurePositions = 1;
}

InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite() {}

// The password object's destructor's callees (textobj_callee, include/ui_DrawObject.h). Unit v5L49: weak, so the test library's fakes win;
// the launcher's generated stand-ins did nothing. 0x1018f2b8 with the hidden delete flag: NULL does nothing; the destructor, then the block
// goes back through operator delete (glue 0x10238218) for a positive flag.
__attribute__((weak)) void textobj_callee::CursorDtor(void* self, int16_t deleteFlag) {
    if (self == nullptr) return;
    static_cast<InterfaceCursorPosTextObjectSprite*>(self)->InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite();
    if (deleteFlag > 0) textobj_callee::Delete(self);
}
__attribute__((weak)) void textobj_callee::Delete(void* self) { ::operator delete(self); }

// what 1: where character `value` is, in the UI's units, as the left and top of the rectangle (the right and bottom are 0); the end of the
// text when the index is negative or past the last character laid out (or nothing has been). what 2: the rectangle the text at
// `value` (an address; 0 answers 0) would take, measured in the object's box with its justification and font, less nothing drawn; its
// bottom is cut to the object's height. The answer is the address of gUIQueryRect.
intptr_t InterfaceCursorPosTextObjectSprite::Query(uint32_t what, intptr_t value) {
    if (what == 1) {
        const int32_t index = static_cast<int32_t>(value);
        if (index >= 0) {
            const TbPoint* positions = mFormatter.GetCharPositionArray();
            const int32_t count = mFormatter.GetCharPositionArraySize();
            if (count != 0 && index < count) {
                gUIQueryRect.Set(static_cast<int16_t>(ToUnitsX(positions[index].x)), static_cast<int16_t>(ToUnitsY(positions[index].y)), 0, 0);
                return reinterpret_cast<intptr_t>(&gUIQueryRect);
            }
        }
        gUIQueryRect.Set(static_cast<int16_t>(ToUnitsX(mFormatter.mCaret.x)), static_cast<int16_t>(ToUnitsY(mFormatter.mCaret.y)), 0, 0);
        return reinterpret_cast<intptr_t>(&gUIQueryRect);
    }
    if (what != 2) return 0;
    if (value == 0) return 0;
    const TextBox box = Justify(*this);
    gText.mRenderParams.SetFont(FontOf(*this));
    const int8_t mode = static_cast<int8_t>(mColourMode);
    if (mode == 1) {
        gText.mRenderParams.UsePreColoured();
    } else if (mode == 0) {
        const uint8_t* colour = TextColour();
        gText.mRenderParams.UseReColour(MakeColour(0xff, colour[0], colour[1], colour[2], 0));
    }
    TbRect bounds = {0, 0, 0, 0};
    const TbRect rect = {box.mLeft, box.mTop, box.mRight, box.mBottom};
    mFormatter.Measure(rect, UIMain_ConvertString(reinterpret_cast<char16_t*>(value)), gText.mRenderParams, &bounds);
    gUIQueryRect.Set(static_cast<int16_t>(ToUnitsX(bounds.left)), static_cast<int16_t>(ToUnitsY(bounds.top)),
                     static_cast<int16_t>(ToUnitsX(bounds.right)), static_cast<int16_t>(ToUnitsY(bounds.bottom)));
    const int32_t height = static_cast<int32_t>(mRect.mBottom) - static_cast<int32_t>(mRect.mTop);
    if (gUIQueryRect.mBottom > height) gUIQueryRect.mBottom = static_cast<int16_t>(height);
    return reinterpret_cast<intptr_t>(&gUIQueryRect);
}

// Unit v5948: the world lobby window's creation and the satellite number windows' message function (main app).
//   CreateWorldLobbyController 0x1022b038 (1600), SatelliteMsg 0x1022ae30 (464)

// Weak; the test library fakes it (the constructor is another file's).
namespace worldlobby_callee {
__attribute__((weak)) InterfaceDrawObject* NewNumberText() {
    void* memory = ::operator new(std::max<size_t>(0x19c, sizeof(InterfaceNumberTextObject)));   // the host object is larger than the Mac's
    lender_callee::NumberCtor(memory);   // 0x1018d930 with NULL
    return static_cast<InterfaceDrawObject*>(memory);
}
}  // namespace worldlobby_callee

namespace vsp_callee {
// Stand-in: InterfaceVisitorTimeTextObject (0x1018edf4) is not declared anywhere; the sprite constructor stands for it.
__attribute__((weak)) void TimeCtor(void* mem) { lender_callee::SpriteCtor(mem); }
}  // namespace vsp_callee

// Unit v51114: the status panel's small members (EnableNameClick, EditNamePanelGone, GetPreviousObject, ObjectGone, CloseObject,
// ZoomTo, CloseWindow, GetName, the empty base versions and the constructor), the SamsUtilities byte swaps next to them and the
// rank text object's destructor.

namespace SamsUtilities {

TagId USwap32(TagId value) { return static_cast<TagId>(__builtin_bswap32(static_cast<uint32_t>(value))); }
TagButtonAction USwap32(TagButtonAction value) {
    return static_cast<TagButtonAction>(__builtin_bswap32(static_cast<uint32_t>(value)));
}
void USwapBlock16(char16_t* words, int32_t count) { _USwapBlock16(reinterpret_cast<uint16_t*>(words), count); }
}  // namespace SamsUtilities

// 0x1018fa18: with the hidden delete flag; NULL does nothing. The sprite chain's destructors run in the order the Mac inlines them.
InterfaceRankTextObject* InterfaceRankTextObject_Dtor(InterfaceRankTextObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        rank_callee::FormatterDtor(&self->mFormatter, -1);   // (the Mac passes self + 0xe8, the formatter)
        rank_callee::SpriteListDtor(self, -1);
        rank_callee::EngineDataDtor(self, -1);
        rank_callee::DrawObjectDtor(self, 0);
        if (deleteFlag > 0) rank_callee::Delete(self);
    }
    return self;
}

// UIMenu::Init (main app 0x1018ad20, 260 bytes) and UIMenu::AddItem (0x1018ae78, 572 bytes; the inventory files them under ui_utils.cpp;
// unit v5163): Init makes the menu's full-screen window and hooks the handlers; AddItem adds a text button.
//
// The word at 0 (mF00) holds the item handler (a function pointer on the Mac). The host's pointers are wider than the word, so the
// word holds a handle that UIMenuHandlerHandle makes of the pointer (0 for NULL), see the end of this file.
// The callees that the host cannot call as the Mac does are the menu_callee functions below (weak: the test library defines its own).

// The callees the host cannot call as the Mac does: the program's versions here (weak), the test library's fakes in
// tests/unit_UIMenu_AddItem.cpp replace them.
namespace menu_callee {
// 0x1018fbe8: InterfaceTitleTextObject::InterfaceTitleTextObject(NULL).
__attribute__((weak)) void TitleTextCtor(void* mem) { new (mem) InterfaceTitleTextObject(nullptr); }
}  // namespace menu_callee

// Unit v51042: UVersion's destructor, the object-state text, the rank text object's Change, the lender list callbacks, the thought and
// name edit handlers (main app, the neighbours of ui_main_panel_UI_Congratulations.cpp).

namespace {
// 0x1024dd1a: the rank text's formats ("%d"); the second is the string 0x16 bytes on (shown for the value 0x7fffffff).
const char16_t kRankFormat[] = u"%d";
const char16_t kRankFormatNone[] = u"-";
}  // namespace

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
