// TbTextFormatter.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbTextFormatter code, named after it.

#include "helpers/TbTextFormatterHelpers.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>

// ltms, the rest of the text formatter's callees (unit w44): the Metrowerks std::vector members of the formatter's three vectors that
// ltms_textformatter.cpp (unit w35a) called as stand-ins, and the line processor.
//   MslVector<CCharacterInfo>: clear 0x10007d74, push_back 0x10007e0c
//   MslVector<CLineInfo>:      clear 0x10008454, push_back 0x100084e4
//   MslVector<TbPoint>:        clear 0x1000884c, resize 0x100088d4 with erase 0x10008be0 and insert 0x10008cf4
//   ProcessLine 0x10006cfc and FixParenthesis 0x1000691c (file-static in the original)
// The vector is three words: capacity, size and the data. clear only drops the size (the original's loop over the elements does nothing:
// they have no destructors). A vector that is full grows by doubling from 1 (new block, copy, delete the old one).

namespace {

// The element copy of the character list: the original copies the character, the index, the type byte, the width and the position, and
// not the padding bytes (so they stay as the allocator gave them).
void CopyElement(CCharacterInfo* to, const CCharacterInfo& from) {
    to->mChar = from.mChar;
    to->mIndex = from.mIndex;
    to->mType = from.mType;
    to->mWidth = from.mWidth;
    to->mPosition = from.mPosition;
}

void CopyElement(CLineInfo* to, const CLineInfo& from) {
    to->mBegin = from.mBegin;
    to->mEnd = from.mEnd;
    to->mWidth = from.mWidth;
}

void CopyElement(TbPoint* to, const TbPoint& from) {
    to->x = from.x;
    to->y = from.y;
}

// push_back of both record vectors (they differ only in the element and in the length limit, the largest size the original allows before
// it throws length_error: counts in the Mac's element sizes).
template <class T>
void PushBack(MslVector<T>& vector, const T& element, uint32_t limit) {
    if (vector.mSize > limit) throw std::length_error("vector::push_back length error");
    const uint32_t capacity = vector.mCapacity;
    if (vector.mSize < capacity) {
        CopyElement(vector.mData + vector.mSize, element);
        vector.mSize = vector.mSize + 1;
        return;
    }
    T* old = vector.mData;
    const uint32_t newCapacity = capacity == 0 ? 1 : capacity << 1;
    vector.mData = static_cast<T*>(::operator new(newCapacity * sizeof(T)));
    const uint32_t count = vector.mSize;
    for (uint32_t i = 0; i < count; ++i) CopyElement(vector.mData + i, old[i]);
    CopyElement(vector.mData + vector.mSize, element);
    if (old != nullptr) ::operator delete(old);
    vector.mSize = vector.mSize + 1;
    vector.mCapacity = newCapacity;
}

}  // namespace

// ---- the vectors

template <>
void MslVector<CCharacterInfo>::clear() {   // 0x10007d74
    mSize = 0;
}

template <>
void MslVector<CCharacterInfo>::push_back(const CCharacterInfo& element) {   // 0x10007e0c
    PushBack(*this, element, 0xffffffe);
}

template <>
void MslVector<CLineInfo>::clear() {   // 0x10008454
    mSize = 0;
}

template <>
void MslVector<CLineInfo>::push_back(const CLineInfo& element) {   // 0x100084e4
    PushBack(*this, element, 0x15555554);
}

template <>
void MslVector<TbPoint>::clear() {   // 0x1000884c
    mSize = 0;
}

// 0x100088d4: bigger inserts zero points at the end, smaller erases the tail (the size stays the same when equal).
template <>
void MslVector<TbPoint>::resize(uint32_t size) {
    const uint32_t current = mSize;
    if (current < size) {
        const TbPoint zero = {0, 0};
        insert(mData + mSize, size - mSize, zero);
    } else if (size < current) {
        erase(mData + size, mData + current);
    }
}

// 0x10008be0: the elements from `last` to the end move down to `first` and the size drops by the number erased. Answers `first`.
template <>
TbPoint* MslVector<TbPoint>::erase(TbPoint* first, TbPoint* last) {
    if (first == last) return first;
    TbPoint* end = mData + mSize;
    const std::ptrdiff_t after = end - last;
    TbPoint* to = first;
    if (after != 0) {
        for (TbPoint* from = last; from != end; ++from) {
            to->x = from->x;
            to->y = from->y;
            ++to;
        }
    }
    mSize = mSize - static_cast<uint32_t>(last - first);
    return first;
}

// 0x10008cf4: `count` copies of `value` go in at `position`. Without room (size + count above the capacity) the block is replaced by one
// that is the capacity (1 when it was 0) doubled until it holds the new size: the part before `position`, the copies, the part after, then
// the old block is deleted. With room the elements after `position` move up (those that land beyond the end are copied into raw storage,
// the others copied backwards), then the copies are written. Too large a count throws length_error (0x1fffffff elements at most).
template <>
void MslVector<TbPoint>::insert(TbPoint* position, uint32_t count, const TbPoint& value) {
    if (count == 0) return;
    if (count > 0x1fffffff || 0x1fffffff - count < mSize) throw std::length_error("vector::insert length error");
    uint32_t capacity = mCapacity;
    const uint32_t newSize = mSize + count;
    if (capacity < newSize) {
        TbPoint* old = mData;
        const uint32_t before = static_cast<uint32_t>(position - old);
        if (capacity == 0) capacity = 1;
        for (; capacity < newSize; capacity = capacity << 1) {}
        mData = static_cast<TbPoint*>(::operator new(capacity * sizeof(TbPoint)));
        if (before != 0) {
            TbPoint* to = mData;
            for (TbPoint* from = old; from != old + before; ++from) {
                CopyElement(to, *from);
                ++to;
            }
        }
        TbPoint* fill = mData + before;
        for (uint32_t n = count; n != 0; --n) {
            CopyElement(fill, value);
            ++fill;
        }
        const uint32_t size = mSize;
        if (before < size) {
            TbPoint* to = mData + before + count;
            for (TbPoint* from = old + before; from != old + size; ++from) {
                CopyElement(to, *from);
                ++to;
            }
        }
        if (old != nullptr) ::operator delete(old);
        mSize = newSize;
        mCapacity = capacity;
        return;
    }
    TbPoint* end = mData + mSize;
    const uint32_t after = static_cast<uint32_t>(end - position);
    if (after == 0) {
        for (uint32_t n = count; n != 0; --n) {
            CopyElement(position, value);
            ++position;
        }
    } else {
        const uint32_t moved = after < count ? after : count;
        const uint32_t beyond = after < count ? count - after : 0;
        TbPoint* to = end + beyond;
        for (TbPoint* from = end - moved; from != end; ++from) {
            CopyElement(to, *from);
            ++to;
        }
        if (count < after) {
            TbPoint* back = end;
            for (TbPoint* from = end - moved; from != position; --from) {
                CopyElement(back - 1, *(from - 1));
                --back;
            }
        }
        TbPoint* assigned = position + moved;
        for (; position < assigned; ++position) CopyElement(position, value);
        TbPoint* last = position + beyond;
        for (; position != last; ++position) CopyElement(position, value);
    }
    mSize = mSize + count;
}

// ---- the line processor

namespace {

// A comma or a space separates the segments of a line.
bool IsDelimiter(const CCharacterInfo* character) { return character->mChar == 0x20 || character->mChar == 0x2c; }

// The Hebrew letters and points: 0x5b1 to 0x5ea (unsigned, as the original compares).
bool HasHebrew(const CCharacterInfo* first, const CCharacterInfo* last) {
    for (const CCharacterInfo* p = first; p < last; ++p)
        if (p->mChar > 0x5b0 && p->mChar < 0x5eb) return true;
    return false;
}

// Reverses [first, last) as whole records (all sixteen bytes).
void InvertLine(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* back = last - 1; first < back; ++first, --back) std::swap(*first, *back);
}

}  // namespace

// CLineList (ltms, 0x10005cc0 and 0x10005f28): the lines of the text formatter.

// 0x10005cc0
void CLineList::AddLine(CCharacterInfo* begin, CCharacterInfo* hideFrom, CCharacterInfo* end, uint32_t width) {
    CLineInfo line;
    line.mBegin = begin;
    line.mEnd = end;
    line.mWidth = width;
    mLines.push_back(line);
    mMaxWidth = (mMaxWidth < width) ? width : mMaxWidth;
    for (CCharacterInfo* c = hideFrom; c != end; ++c) c->mType |= kCharHidden;
}

// 0x10005f28
CLineList* CLineList::Destruct(int16_t deleteFlag) {
    if (this != nullptr) {
        mLines.tear_down();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(this));
    }
    return this;
}

// ltms TbTextFormatter: construction, the justification setters, the character list, the line layout and the drawing
// (0x10005dc4..0x10007c64), and the three std::vector tear-downs they use (0x10007cb4, 0x1000839c, 0x1000879c). Unit w35a.
// The vector members clear, push_back and resize, the file-static ProcessLine and the TbFontRenderParams getters are other
// functions of the library that no unit has translated yet (include/TbTextFormatter.h, include/TbFontRenderParams.h).

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

// The game's glue 0x10238e18 to the destructor above with the hidden delete flag (0x100061a8: NULL does nothing; the destructor, then the
// block goes back for a positive flag), as the rank text object's destructor calls it: rank_callee::FormatterDtor, declared with the other
// rank_callee callees in include/ui_DrawObject.h (not included by this library). Unit v5L49: weak, so the test library's fake wins; the
// launcher's generated stand-in did nothing.
namespace rank_callee {
void FormatterDtor(void* self, int16_t deleteFlag);
}  // namespace rank_callee
__attribute__((weak)) void rank_callee::FormatterDtor(void* self, int16_t deleteFlag) {
    if (self == nullptr) return;
    static_cast<TbTextFormatter*>(self)->TbTextFormatter::~TbTextFormatter();
    if (deleteFlag > 0) ::operator delete(self);
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

// 0x1000691c. `first` should hold a bracket; its kind is compared with each character of the range, counting up for the same kind and down
// for the other bracket (the characters that are no bracket count for nothing unless the first is none either). When the count falls back
// to 0 that character and the first are swapped, whole records. An empty range or no match changes nothing. The first character always
// counts 1, so the match is never the first itself.
void FixParenthesis(CCharacterInfo* first, CCharacterInfo* last) {
    const int kind = IsParenthesis(first);
    int depth = 0;
    if (last <= first) return;
    for (CCharacterInfo* p = first; p < last; ++p) {
        const int other = IsParenthesis(p);
        if (kind == other) depth = depth + 1;
        else if (other != 0) depth = depth - 1;
        if (depth == 0) {
            std::swap(*first, *p);
            return;
        }
    }
}

// 0x10006c3c: reverses [first, last) in place (same body as the file-local one in ltms_textformatter_rest.cpp).
namespace tpw_v5757 {
void InvertLine(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* back = last - 1; first < back; ++first, --back) std::swap(*first, *back);
}
}  // namespace tpw_v5757

// 0x10006a68: the first character of [first, last) that is a space or a comma; last when there is none.
CCharacterInfo* GetNextDelimiter(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* p = first; p < last; ++p)
        if (p->mChar == 0x20 || p->mChar == 0x2c) return p;
    return last;
}

// 0x10006b04: the first character of [first, last) that is neither a space nor a comma; NULL when there is none, or at once
// when first is past last - 2.
CCharacterInfo* GetNextNonDelimiter(CCharacterInfo* first, CCharacterInfo* last) {
    if (first > last - 2) return nullptr;
    for (CCharacterInfo* p = first; p < last; ++p)
        if (!(p->mChar == 0x20 || p->mChar == 0x2c)) return p;
    return nullptr;
}

// 0x10006bb4: true when a character of [first, last) lies strictly between 0x5b0 and 0x5eb (Hebrew letters).
bool IsHebInLine(CCharacterInfo* first, CCharacterInfo* last) {
    for (CCharacterInfo* p = first; p < last; ++p)
        if (p->mChar > 0x5b0 && p->mChar < 0x5eb) return true;
    return false;
}

// 0x10006cfc. A line with no Hebrew letter is left alone. Otherwise it is first reversed whole (right to left), then each run between
// the separators (spaces and commas) is looked at from the start of the line on: a run that has no Hebrew letter is reversed back, a run
// that has some and starts with a bracket gets FixParenthesis, and any other run stays reversed. The walk stops when fewer than two
// characters are left after a run; the separators themselves are skipped and stay where the first reversal put them.
void ProcessLine(CCharacterInfo* first, CCharacterInfo* last) {
    if (!HasHebrew(first, last)) return;
    InvertLine(first, last);
    CCharacterInfo* cursor = first;
    while (true) {
        CCharacterInfo* start = nullptr;
        if (!(cursor > last - 2)) {
            for (CCharacterInfo* p = cursor; p < last; ++p) {
                if (!IsDelimiter(p)) {
                    start = p;
                    break;
                }
            }
        }
        if (start == nullptr) return;
        CCharacterInfo* end = last;
        for (CCharacterInfo* p = start; p < last; ++p) {
            if (IsDelimiter(p)) {
                end = p;
                break;
            }
        }
        if (HasHebrew(start, end)) {
            if (IsParenthesis(start) > 0) FixParenthesis(start, end);
        } else {
            InvertLine(start, end);
        }
        cursor = end;
    }
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
