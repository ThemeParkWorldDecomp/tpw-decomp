// ltms TbTextFormatter: construction, the justification setters, the character list, the line layout and the drawing
// (0x10005dc4..0x10007c64), and the three std::vector tear-downs they use (0x10007cb4, 0x1000839c, 0x1000879c). Unit w35a.
// The vector members clear, push_back and resize, the file-static ProcessLine and the TbFontRenderParams getters are other
// functions of the library that no unit has translated yet (include/TbTextFormatter.h, include/TbFontRenderParams.h).
#include "TbTextFormatter.h"

#include <new>

#include "TbResource.h"

namespace {

// A Hebrew letter or point: 0x5b1 to 0x5ea.
bool IsHebrew(uint16_t character) { return character > 0x5b0 && character < 0x5eb; }

}  // namespace

// ---- the vector tear-downs: the block is deleted when there is one. The original first counts down over the elements
// (a loop with no body, they have no destructors); nothing else is cleared.

template <>
void MslVector<CCharacterInfo>::tear_down() {   // 0x10007cb4
    if (mData != nullptr) ::operator delete(mData);
}

template <>
void MslVector<CLineInfo>::tear_down() {   // 0x1000839c
    if (mData != nullptr) ::operator delete(mData);
}

template <>
void MslVector<TbPoint>::tear_down() {   // 0x1000879c
    if (mData != nullptr) ::operator delete(mData);
}

// ---- the formatter

// 0x10005dc4: the render, the three vectors empty, the counter, the flag, the caret and the text block's rectangle at zero.
// The justifications, the wrap width and the line height are NOT set (the callers set them before use).
TbTextFormatter::TbTextFormatter()
    : mChars{0, 0, nullptr},
      mCharCounter(0),
      mLines{0, 0, nullptr},
      mMaxLineWidth(0),
      mPositions{0, 0, nullptr},
      mMeasurePositions(0),
      mCaret{0, 0},
      mBounds{0, 0, 0, 0} {}

// 0x100061a8: the three blocks are deleted (positions, lines, characters), then the render goes with the members.
TbTextFormatter::~TbTextFormatter() {
    mPositions.tear_down();
    mLines.tear_down();
    mChars.tear_down();
}

// 0x10006288: the vertical justifications are 2, 4 and 5. Anything else is refused and changes nothing.
TbError TbTextFormatter::SetVerticalJustification(eJustification value) {
    if (value != 2 && value != 4 && value != 5) return TbError(-1);
    mVerticalJustification = value;
    return TbError(0);
}

// 0x1000632c: BUG kept: 0, 1 and 2 are stored but the answer is -1 whatever the value.
TbError TbTextFormatter::SetHorizontalJustification(eJustification value) {
    if (value >= 0 && value < 3) mHorizontalJustification = value;
    return TbError(-1);
}

const TbPoint* TbTextFormatter::GetCharPositionArray() const { return mPositions.mData; }          // 0x100063b4

uint32_t TbTextFormatter::GetCharPositionArraySize() const { return mPositions.mSize; }            // 0x100063fc

// 0x10006448: a string with no characters has one position. It starts at the rectangle's top left; a vertical centre or
// bottom (2 or 5) moves it down (the centre halves an unsigned difference), a horizontal right or centre (1 or 2) across.
TbError TbTextFormatter::EmptyStringProcess(const TbRect& rect) {
    mPositions.resize(1);
    mCaret.x = rect.left;
    mCaret.y = rect.top;
    if (mVerticalJustification == 2) {
        mCaret.y = rect.top + static_cast<int32_t>(static_cast<uint32_t>((rect.bottom - rect.top) - mLineHeight) >> 1);
    } else if (mVerticalJustification == 5) {
        mCaret.y = rect.bottom - mLineHeight;
    }
    if (mHorizontalJustification == 1) {
        mCaret.x = rect.right;
    } else if (mHorizontalJustification == 2) {
        mCaret.x = rect.left + (rect.right - rect.left) / 2;
    }
    mPositions.mData[0] = mCaret;
    return TbError(0);
}

// 0x100065b8
TbError TbTextFormatter::Draw(const TbRect& rect, const uint8_t* text, TbFontRenderParams& params, TbRect* drawn) {
    if (CreateInfoList(text, params) < 0) return TbError(-1);
    if (mChars.mSize == 0) return EmptyStringProcess(rect);
    mWrapWidth = static_cast<uint32_t>(rect.right - rect.left);
    if (Layout(rect) < 0) return TbError(-1);
    DrawText(params, drawn, true);   // the answer is not looked at
    return TbError(0);
}

// 0x100066f4
TbError TbTextFormatter::Measure(const TbRect& rect, const uint8_t* text, TbFontRenderParams& params, TbRect* measured) {
    if (CreateInfoList(text, params) < 0) return TbError(-1);
    if (mChars.mSize == 0) return EmptyStringProcess(rect);
    mWrapWidth = static_cast<uint32_t>(rect.right - rect.left);
    if (Layout(rect) < 0) return TbError(-1);
    if (measured == nullptr) return TbError(-1);
    if (mMeasurePositions != 0) DrawText(params, nullptr, false);
    measured->left = mBounds.left;
    measured->top = mBounds.top;
    measured->right = mBounds.right;
    measured->bottom = mBounds.bottom;
    return TbError(0);
}

// 0x1000700c. The positions array is emptied and sized to one entry per character and one for the end. The render picks its
// method for `params` (a failure ends the call with -1 and nothing drawn). Each line goes through ProcessLine (the Hebrew
// reordering), is placed by the horizontal justification (an unsigned difference halved for the centre) and its characters
// are walked: each one's entry in the positions array is the pen position, indexed by the character's own NUMBER (so a
// reordered run fills the array in its original order). The caret follows the characters in the string's order; the rest
// draws, left to right in the reordered order, skipping the line-break characters (which take no room) and, when drawing,
// the hidden ones (they still take their width). The union of the rectangles DrawChar answers, ignoring empty ones, goes to
// `bounds`; the caret after the last character goes to the last entry.
TbError TbTextFormatter::DrawText(TbFontRenderParams& params, TbRect* bounds, bool draw) {
    mPositions.clear();
    mPositions.resize(mChars.mSize + 1);
    const TbError started = mRender.Initialise(params);
    if (!started.Succeeded()) return TbError(-1);

    TbPoint pen = {mBounds.left, mBounds.top};
    TbRect drawnBounds = {0, 0, 0, 0};
    bool haveBounds = false;
    for (CLineInfo* line = mLines.mData; line != mLines.mData + mLines.mSize; ++line) {
        ProcessLine(line->mBegin, line->mEnd);
        pen.x = mBounds.left;
        if (mHorizontalJustification == 1) {
            pen.x += (mBounds.right - mBounds.left) - static_cast<int32_t>(line->mWidth);
        } else if (mHorizontalJustification == 2) {
            pen.x += static_cast<int32_t>(static_cast<uint32_t>((mBounds.right - mBounds.left) - static_cast<int32_t>(line->mWidth)) >> 1);
        }
        uint32_t lastPosition = 0;
        TbRect charRect = {0, 0, 0, 0};
        mCaret.x = pen.x;
        mCaret.y = pen.y;
        for (CCharacterInfo* ch = line->mBegin; ch != line->mEnd; ++ch) {
            mPositions.mData[ch->mPosition] = pen;
            if (ch->mPosition >= (lastPosition & 0xffff)) {
                bool plain = !IsHebrew(ch->mChar);
                if (plain && (ch->mType & kCharDelimiter) != 0 && ch + 1 != line->mEnd && IsHebrew((ch + 1)->mChar)) plain = false;
                mCaret = mPositions.mData[ch->mPosition];
                lastPosition = ch->mPosition;
                if (plain) {
                    if ((ch->mType & kCharLineBreak) == 0) {
                        mCaret.x += ch->mWidth;
                    } else {
                        mCaret.x = mBounds.left;
                        if (mHorizontalJustification == 1) {
                            mCaret.x += mBounds.right - mBounds.left;
                        } else if (mHorizontalJustification == 2) {
                            mCaret.x += (mBounds.right - mBounds.left) / 2;
                        }
                        mCaret.y += mLineHeight;
                    }
                }
            }
            if ((ch->mType & kCharLineBreak) == 0) {
                if (draw && (ch->mType & kCharHidden) == 0) {
                    mRender.DrawChar(pen, ch->mIndex, charRect);   // the answer is not looked at
                    if (!(charRect.left == charRect.right && charRect.top == charRect.bottom)) {
                        drawnBounds = haveBounds ? drawnBounds.Bounding(charRect) : charRect;
                        haveBounds = true;
                    }
                }
                pen.x += ch->mWidth;
            }
        }
        pen.y += mLineHeight;
    }
    if (bounds != nullptr) *bounds = drawnBounds;
    mPositions.mData[mChars.mSize] = mCaret;
    return TbError(0);
}

// 0x10007514. The font is taken with GetFont (a reference that is never let go). Each byte of `text` is a character index
// less one; from the converter's first lead byte on, a second byte follows (and is read even when it is the terminator, after
// which the loop reads on past it). The Unicode value comes from the converter's table (0 beyond its count); the width is the
// first word of GetCharSize; the number counts up from 0 in a halfword.
TbError TbTextFormatter::CreateInfoList(const uint8_t* text, TbFontRenderParams& params) {
    mChars.clear();
    mCharCounter = 0;
    TbIFont* font = params.GetFont();
    if (font == nullptr) return TbError(-1);
    mLineHeight = font->GetVerticalSpacing() & 0xffff;
    const TbIMBToUnicode* converter = font->GetMBToUnicode();
    if (converter == nullptr) return TbError(-1);
    while (*text != 0) {
        uint32_t index = static_cast<uint32_t>(*text++) - 1;
        const uint32_t lead = converter->mFirstLead;
        if ((index & 0xffff) >= lead) {
            const uint32_t second = *text++;
            index = lead + ((index & 0xffff) - lead) * 0xff + second - 1;
        }
        const uint16_t glyph = static_cast<uint16_t>(index);
        const uint16_t character = glyph < converter->mCount ? converter->mTable[glyph] : 0;
        const TbFontCharSize size = font->GetCharSize(glyph);
        CCharacterInfo info;
        info.mChar = character;
        info.mIndex = glyph;
        info.mType = GetCharType(character);
        info.mPad5 = 0;
        info.mPad6 = 0;
        info.mWidth = static_cast<int32_t>(size.mWidth);
        info.mPosition = mCharCounter;
        info.mPadE = 0;
        ++mCharCounter;
        mChars.push_back(info);
    }
    return TbError(0);
}

// 0x100076ec: line feed 4, carriage return 0xc, tab and space 0x12, everything else 1.
uint8_t TbTextFormatter::GetCharType(char16_t character) {
    switch (character) {
        case 0x0a: return kCharLineBreak;
        case 0x0d: return kCharLineBreak | kCharCarriageReturn;
        case 0x09:
        case 0x20: return kCharDelimiter | kCharWordBreak;
        default: return kCharNormal;
    }
}

// 0x1000776c. The text is cut into lines at the line-break characters (a break takes the next carriage return with it), each
// run of characters goes to LayoutLines (which wraps it), and the block is placed in `rect`: vertically (5 bottom, 2 centre,
// anything else top; the centre halves a signed difference toward zero) and horizontally (1 right, 2 centre, anything else
// left). With no characters it only empties the lines and the widest width and leaves the block alone.
TbError TbTextFormatter::Layout(TbRect rect) {
    int32_t lines = 0;
    mLines.clear();
    mMaxLineWidth = 0;
    CCharacterInfo* const first = mChars.mData;
    if ((mChars.mSize << 4) == 0) return TbError(0);
    CCharacterInfo* start = first;
    CCharacterInfo* cursor = first;
    CCharacterInfo* end = mChars.mData + mChars.mSize;
    while (cursor != end) {
        if ((cursor->mType & kCharLineBreak) == 0) {
            ++cursor;
        } else {
            ++cursor;
            if (cursor != end && (cursor->mType & kCharLineBreak) != 0 && (cursor->mType & kCharCarriageReturn) != 0) ++cursor;
            lines += LayoutLines(start, cursor);
            start = cursor;
        }
        end = mChars.mData + mChars.mSize;
    }
    if (start != end) lines += LayoutLines(start, end);

    const int32_t height = lines * mLineHeight;
    const uint32_t width = mMaxLineWidth;
    if (mVerticalJustification == 5) {
        mBounds.top = rect.bottom - height;
        mBounds.bottom = rect.bottom;
    } else if (mVerticalJustification == 2) {
        mBounds.top = rect.top + ((rect.bottom - rect.top) - height) / 2;
        mBounds.bottom = mBounds.top + height;
    } else {
        mBounds.top = rect.top;
        mBounds.bottom = rect.top + height;
    }
    if (mHorizontalJustification == 1) {
        mBounds.left = rect.right - static_cast<int32_t>(width);
        mBounds.right = rect.right;
    } else if (mHorizontalJustification == 2) {
        mBounds.left = rect.left + ((rect.right - rect.left) - static_cast<int32_t>(width)) / 2;
        mBounds.right = mBounds.left + static_cast<int32_t>(width);
    } else {
        mBounds.left = rect.left;
        mBounds.right = rect.left + static_cast<int32_t>(width);
    }
    return TbError(0);
}

// 0x10007994. Wraps the characters [first, last) into lines no wider than mWrapWidth (an unsigned limit), adds each to
// mLines and raises mMaxLineWidth. A character that does not fit ends the line at the last word break (a character with the
// word-break bit) in front of it, or in front of itself when the line has none, and the delimiters at the break (and the
// ones in front of it, which leave the line's width) are marked hidden and skipped. The first character of a line goes on
// that line however wide it is. The characters of the LAST line (and of an empty one) are never marked: the marking loop
// of the original starts at the end of the run. Answers the number of lines added.
int32_t TbTextFormatter::LayoutLines(CCharacterInfo* first, CCharacterInfo* last) {
    int32_t lines = 0;
    uint32_t width = 0;
    if (first == last) {
        const CLineInfo line = {first, last, 0};
        const uint32_t lineWidth = 0;
        mLines.push_back(line);
        if (mMaxLineWidth < lineWidth) mMaxLineWidth = lineWidth;
        return 1;
    }
    for (CCharacterInfo* cursor = first; cursor != last; ++cursor) {
        if ((cursor->mType & kCharLineBreak) != 0) continue;
        const int32_t advance = cursor->mWidth;
        if (width + static_cast<uint32_t>(advance) <= mWrapWidth) {
            width += advance;
            continue;
        }
        CCharacterInfo* hideFrom;
        CCharacterInfo* next;
        uint32_t lineWidth;
        if (cursor == first) {
            width += advance;
            next = cursor + 1;
            hideFrom = next;
            lineWidth = width;
        } else {
            CCharacterInfo* scan = cursor;
            uint32_t back = width;
            while (scan != first && (scan->mType & kCharWordBreak) == 0) {
                back -= (scan - 1)->mWidth;
                --scan;
            }
            if (scan == first) {
                next = cursor;
                hideFrom = next;
                lineWidth = width;
            } else {
                next = scan;
                --scan;
                while (scan != first && (scan->mType & kCharDelimiter) != 0) {
                    back -= scan->mWidth;
                    --scan;
                }
                lineWidth = back;
                hideFrom = scan + 1;
            }
        }
        while (next != last && (next->mType & kCharDelimiter) != 0) ++next;
        const CLineInfo line = {first, next, lineWidth};
        mLines.push_back(line);
        if (mMaxLineWidth < lineWidth) mMaxLineWidth = lineWidth;
        for (CCharacterInfo* hidden = hideFrom; hidden != next; ++hidden) hidden->mType |= kCharHidden;
        first = next;
        width = 0;
        ++lines;
        cursor = next - 1;
    }
    if (first != last) {
        const CLineInfo line = {first, last, width};
        mLines.push_back(line);
        if (mMaxLineWidth < width) mMaxLineWidth = width;
        ++lines;
    }
    return lines;
}

// 0x10007c64: the original answers 0 and uses nothing.
int32_t TbTextFormatter::GetCharIdxFromPosition(TbPoint) { return 0; }
