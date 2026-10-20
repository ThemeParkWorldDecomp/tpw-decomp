// Unit v5203: InterfaceListControl2::Sort 0x101d1958 (1320 bytes), InterfaceListControl2::FindString 0x101d44b8 (348 bytes).
#include <cstdint>

#include "online.h"
#include "ui_controls.h"
#include "ui_ime.h"

namespace {

// compare(cell, 0, cell length, other text, other length) of the Metrowerks wide string (the cells are LCStrings: one pointer to the rep).
int32_t CompareCells(const LCString* a, const LCString* b) {
    return IME::MslWideString_compare(reinterpret_cast<const IME::MslWideString*>(a), 0, static_cast<uint32_t>(a->mRep->mLength),
                                      b->mRep->mData, static_cast<uint32_t>(b->mRep->mLength));
}

}  // namespace

// 0x101d1958. The argument is not used. First the entry with the least (greatest when mSortDescending is 1... the test below decides)
// key among the first mItemCount entries is moved to the front (its predecessor is bypassed, the old first entry is linked after it
// QUIRK: as the original, the old first is not unlinked from where it was), then an insertion sort over positions 2 on.
// A mSortDescending outside 0 and 1 never moves anything. The column type 1 is a number (signed long), anything else a string.
void InterfaceListControl2::Sort(int32_t) {
    if (mColumns == nullptr) return;
    if (mItemCount < 2) return;
    const bool isNumber = mColumns[mSortColumn].mBottom == 1;
    LCWord* block = Words();
    const int32_t column = mSortColumn;
    int32_t first = mFirstEntry;
    int32_t best = first;
    int32_t bestPrevious = -1;
    int32_t previous = -1;
    int32_t current = first;
    int32_t count = 0;
    LCWord* bestCell = &block[first + column + 4];
    while (count < mItemCount && Words()[current + 3].mInt != -2 && current != -1) {
        bool take = false;
        LCWord* cell = &block[current + column + 4];
        if (isNumber) {
            if (mSortDescending == 1) {
                if (cell->mInt > bestCell->mInt) take = true;
            } else if (mSortDescending == 0) {
                if (cell->mInt < bestCell->mInt) take = true;
            }
        } else {
            if (mSortDescending == 1) {
                if (CompareCells(cell->mString, bestCell->mString) > 0) take = true;
            } else if (mSortDescending == 0) {
                if (CompareCells(cell->mString, bestCell->mString) < 0) take = true;
            }
        }
        if (take) {
            best = current;
            bestPrevious = previous;
            bestCell = cell;
        }
        previous = current;
        current = block[current].mInt;
        ++count;
    }
    if (best < 0) return;
    first = mFirstEntry;
    if (best != first) {
        if (bestPrevious != -1) Words()[bestPrevious].mInt = Words()[best].mInt;
        Words()[best].mInt = first;
        mFirstEntry = best;
    }
    if (mItemCount <= 2) return;
    GetNonHierIndexedEntry(0, -1);
    GetNonHierIndexedEntry(1, -1);
    int32_t after = GetNonHierIndexedEntry(2, -1);
    count = 2;
    while (count < mItemCount && Words()[after + 3].mInt != -2 && after >= 0) {
        bool move = false;
        int32_t holder = GetNonHierIndexedEntry(count - 1, -1);
        LCWord* w = Words();
        int32_t item = w[holder].mInt;
        after = w[item].mInt;
        int32_t head = GetNonHierIndexedEntry(0, -1);
        w = Words();
        int32_t scan = w[head].mInt;
        int32_t scanNext = w[scan].mInt;
        int32_t scanParent = w[scan + 3].mInt;
        int32_t scanPrevious = head;
        while (!move && scanParent > -2 && scan != item) {
            w = Words();
            const LCWord& mine = w[item + mSortColumn + 4];
            const LCWord& other = w[scan + mSortColumn + 4];
            if (isNumber) {
                if (mSortDescending == 1) {
                    if (mine.mInt > other.mInt) move = true;
                } else if (mSortDescending == 0) {
                    if (mine.mInt < other.mInt) move = true;
                }
            } else {
                if (mSortDescending == 1) {
                    if (CompareCells(mine.mString, other.mString) > 0) move = true;
                } else if (mSortDescending == 0) {
                    if (CompareCells(mine.mString, other.mString) < 0) move = true;
                }
            }
            if (!move) {
                w = Words();
                scanPrevious = scan;
                scan = scanNext;
                scanParent = w[scanNext + 3].mInt;
                scanNext = w[scanNext].mInt;
            }
        }
        if (move && scanPrevious != item) {
            Words()[holder].mInt = after;
            Words()[scanPrevious].mInt = item;
            Words()[item].mInt = scan;
        }
        ++count;
    }
}

// 0x101d44b8. The position (counted from the first entry along AdvanceIndex) of the first entry whose text cell in column `column`
// (clamped to 1..mColumnCount) equals `text`, -1 for none; nothing when the column is not a text column or there are no columns.
// QUIRK: the original's first loop (a walk of a negative count) never runs.
int32_t InterfaceListControl2::FindString(char16_t* text, int32_t column) {
    int32_t entry = mFirstEntry;
    bool found = false;
    int32_t result = -1;
    int32_t index = -1;
    if (column < 1) column = 1;
    if (column > mColumnCount) column = mColumnCount;
    if (mColumns != nullptr && mColumns[static_cast<int16_t>(column - 1)].mBottom == 0) {
        while (entry != -1 && !found && index < mItemCount - 1) {
            ++index;
            LCString* cell = Words()[column + entry + 3].mString;
            uint32_t length = online_parser_callee::WcsLen(text);
            if (IME::MslWideString_compare(reinterpret_cast<const IME::MslWideString*>(cell), 0, static_cast<uint32_t>(cell->mRep->mLength), text,
                                           length) == 0) {
                found = true;
                result = index;
            }
            entry = AdvanceIndex(entry, nullptr);
        }
    }
    return result;
}
