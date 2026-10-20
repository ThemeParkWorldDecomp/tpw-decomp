// ltms, the rest of the text formatter's callees (unit w44): the Metrowerks std::vector members of the formatter's three vectors that
// ltms_textformatter.cpp (unit w35a) called as stand-ins, and the line processor.
//   MslVector<CCharacterInfo>: clear 0x10007d74, push_back 0x10007e0c
//   MslVector<CLineInfo>:      clear 0x10008454, push_back 0x100084e4
//   MslVector<TbPoint>:        clear 0x1000884c, resize 0x100088d4 with erase 0x10008be0 and insert 0x10008cf4
//   ProcessLine 0x10006cfc and FixParenthesis 0x1000691c (file-static in the original)
// The vector is three words: capacity, size and the data. clear only drops the size (the original's loop over the elements does nothing:
// they have no destructors). A vector that is full grows by doubling from 1 (new block, copy, delete the old one).
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>

#include "TbTextFormatter.h"

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

// The bracket kind of a character: 1 for '(', 2 for ')', 0 otherwise.
int ParenthesisKind(const CCharacterInfo* character) {
    if (character->mChar == 0x28) return 1;
    if (character->mChar == 0x29) return 2;
    return 0;
}

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

// 0x1000691c. `first` should hold a bracket; its kind is compared with each character of the range, counting up for the same kind and down
// for the other bracket (the characters that are no bracket count for nothing unless the first is none either). When the count falls back
// to 0 that character and the first are swapped, whole records. An empty range or no match changes nothing. The first character always
// counts 1, so the match is never the first itself.
void FixParenthesis(CCharacterInfo* first, CCharacterInfo* last) {
    const int kind = ParenthesisKind(first);
    int depth = 0;
    if (last <= first) return;
    for (CCharacterInfo* p = first; p < last; ++p) {
        const int other = ParenthesisKind(p);
        if (kind == other) depth = depth + 1;
        else if (other != 0) depth = depth - 1;
        if (depth == 0) {
            std::swap(*first, *p);
            return;
        }
    }
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
            if (ParenthesisKind(start) > 0) FixParenthesis(start, end);
        } else {
            InvertLine(start, end);
        }
        cursor = end;
    }
}
