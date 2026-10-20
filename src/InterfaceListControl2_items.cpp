// The list control's item management (main app, unit w93): the block of entries, the display of the visible rows, the column
// formats, adding and changing items, and the current entry.
//
// The item block (mItemStorage) is an array of words (LCWord, 4 bytes on the Mac). An entry is mColumnCount + 4 words: the next
// entry, the caller's data word, the first child and the parent (-1 none, -2 for a free entry), then one cell per column. An entry
// is named by the index of its first word. AllocNewBlock chains every new entry to the next one, so the chain runs through the used
// entries first and the free ones after them; AddItem takes the first free entry.
#include "ui_controls.h"

#include <cstring>

#include "platform/msl.h"
#include "ui_DrawObject.h"

namespace {

// The words after the cells of an entry are not part of it: an entry occupies kLCCells + mColumnCount words.
constexpr int32_t kUpdateMessage = 0x403;   // told to a row window: the address of the entry's cells plus 4 words, and the data word

// The sort test of the Mac compares a long with the other long: a 32-bit signed compare on the cells' low halves.
int32_t CellInt(const LCWord& word) { return word.mInt; }

}  // namespace

// 0x101d1eb4 (368 bytes). The old object is released first: one user fewer, told Show(0), and deleted when nobody uses it and the
// library does not hold it (the reference expression is the one the window's own SetDrawObject uses).
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) {
    InterfaceDrawObject* old = mItemDrawObject;
    if (object == old) return;
    if (old != nullptr) {
        old->mResizable = static_cast<int16_t>(old->mResizable - 1);
        old->Show(0);
        if (static_cast<int16_t>(old->mResizable - (1 - old->mInLibrary)) == -1 && mItemDrawObject != nullptr) {
            delete mItemDrawObject;
        }
    }
    mItemDrawObject = object;
    if (mItemDrawObject == nullptr) return;
    mItemDrawObject->mResizable = 1;
    DOChange change{};
    change.mFrame = 0;
    change.mDepth = static_cast<int16_t>(mZ + 2);
    change.mF08 = mZRot;
    CoolRect rect(mListRect.mLeft, mListRect.mTop, mListRect.mRight, static_cast<int16_t>(mListRect.mTop + mRowHeight));
    change.mRect = &rect;
    mItemDrawObject->Change(0x17, &change);
    if (GetVisibility() != 0 && mCurrentEntry != -1) mItemDrawObject->Show(1);
}

// 0x101d207c (360 bytes). A new block of mBlockWords + count entries' worth of words, cleared; the new entries are chained in
// order (the last one ends the chain, every new entry is free and childless); the old words are copied and the last used entry
// (FindFreeEntry's second answer, -1 when the first entry is free: the original then writes before the block) is linked to the
// first new one.
void* InterfaceListControl2::AllocNewBlock(int32_t count) {
    if (count == 0 || mColumnCount == 0) return nullptr;
    uint8_t* old = mItemStorage;
    const int32_t added = count * (mColumnCount + 4);
    int32_t freeEntry = 0;
    int32_t previous = 0;
    if (old != nullptr) FindFreeEntry(freeEntry, previous);
    const uint32_t total = static_cast<uint32_t>(mBlockWords + added);
    uint8_t* bytes = new uint8_t[static_cast<size_t>(total) * sizeof(LCWord)];
    LCWord* block = reinterpret_cast<LCWord*>(bytes);
    std::memset(bytes, 0, static_cast<size_t>(total) * sizeof(LCWord));
    int32_t at = mBlockWords;
    for (int32_t i = 0; i < count - 1; ++i) {
        block[at + kLCNext].mInt = at + mColumnCount + 4;
        block[at + kLCChild].mInt = -1;
        block[at + kLCParent].mInt = -2;
        at = mColumnCount + at + 4;
    }
    block[at + kLCNext].mInt = -1;
    block[at + kLCChild].mInt = -1;
    block[at + kLCParent].mInt = -2;
    if (old != nullptr) {
        std::memcpy(bytes, old, static_cast<size_t>(static_cast<uint32_t>(mBlockWords)) * sizeof(LCWord));
        block[previous].mInt = mBlockWords;
        delete[] old;
    }
    mItemStorage = bytes;
    mBlockWords += added;
    return bytes;
}

// 0x101d2220 (556 bytes). Walks the visible rows from mTopEntry. The indentation of a row is 0x14 pixels a level (the depth is
// counted from the first row's ancestors and then followed with AdvanceIndex's changes). A row past the last entry is hidden; a
// row of an invisible list is hidden too.
// QUIRK: the message 0x403 carries the address of word 4 of the entry's cells, i.e. 8 words into the entry, as the original; the
// host passes the low half of that address.
void InterfaceListControl2::UpdateDisplay() {
    int32_t depth = 0;
    LCWord* cells = Words();
    if (mItemWindows == nullptr) return;
    int32_t entry = GetIndexedEntry(mTopEntry);
    const int16_t visibility = GetVisibility();
    if (entry > -1) {
        for (int32_t parent = Words()[entry + kLCParent].mInt; parent > -1; parent = Words()[parent + kLCParent].mInt) ++depth;
    }
    for (int32_t row = 0; row < mVisibleRows; ++row) {
        InterfaceWindow* window = mItemWindows[row];
        // row + mTopEntry < mItemCount as a signed compare
        const bool inList = static_cast<int32_t>(static_cast<uint32_t>(row) + static_cast<uint32_t>(mTopEntry)) < mItemCount;
        window->Show((visibility != 0 && inList) ? 1 : 0);
        if (inList) cells = Words() + entry + kLCCells;
        for (int32_t column = 0; column < mColumnCount; ++column) {
            if (inList) {
                UI_SendMessage(window, kUpdateMessage, static_cast<int32_t>(reinterpret_cast<intptr_t>(cells + 4)),
                               Words()[entry + kLCData].mInt);
                if (GetColumnType(static_cast<int16_t>(column)) == kLCNumber) {
                    window->SetValue(cells[column].mInt);
                } else {
                    if (column == 0) window->SetTextOffset(depth * 0x14, 0);
                    window->SetText(cells[column].mString->mRep->mData);
                }
            }
            window = column == 0 ? window->GetChild(0) : window->GetNext();
        }
        if (inList) {
            int32_t change = 0;
            entry = AdvanceIndex(entry, &change);
            depth += change;
        }
    }
}

// 0x101d2614 (48 bytes). The type is the bottom of the column's rectangle.
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    if (column >= mColumnCount) return;
    if (mColumns == nullptr) return;
    mColumns[column].mBottom = static_cast<int16_t>(type);
}

// 0x101d268c (24 bytes).
int32_t InterfaceListControl2::GetColumnType(int16_t column) { return mColumns[column].mBottom; }

// 0x101d26e0 (308 bytes). The original makes and unmakes a CoolRect it never uses. The row's top is the list's top plus the row
// height times the row (32-bit arithmetic, then cut to 16 bits); the bottom is that plus the row height.
void InterfaceListControl2::SetColumnFormat(int32_t row, InterfaceWindow* window) {
    CoolRect unused;
    if (window != nullptr && mItemWindows != nullptr && mColumns != nullptr && row < mVisibleRows) {
        mItemWindows[row] = window;
        const int32_t top = mListRect.mTop + mRowHeight * row;
        for (int32_t column = 0; column < mColumnCount; ++column) {
            const int16_t y = static_cast<int16_t>(top);
            window->SetPosition(mColumns[column].mLeft, y, mColumns[column].mRight, y + mRowHeight);
            window->setZ(static_cast<int16_t>(mZ + 3));
            window = column == 0 ? window->GetChild(0) : window->GetNext();
        }
    }
}

// 0x101d2864 (436 bytes). The row height is the first window's (adjusted) height; the rows that fit are the list rectangle's height
// divided by it (a height of 0 divides by zero on the Mac, with an undefined answer: 0 here). The array of row windows is made
// once, cleared (the original leaves it as the heap had it). The selection bar is resized to one row (Change flag 0x10).
// Rows are then made from the last to the first; row 0 takes the window the first call made.
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    if (makeRow == nullptr) return;
    InterfaceWindow* window = makeRow();
    if (window != nullptr) {
        int16_t width = 0;
        int16_t height = 0;
        window->GetDims(&width, &height, 1);
        mRowHeight = height;
        mVisibleRows = mRowHeight == 0 ? static_cast<int16_t>(0)
                                       : static_cast<int16_t>((mListRect.mBottom - mListRect.mTop) / mRowHeight);
        if (mItemWindows == nullptr) mItemWindows = new InterfaceWindow*[static_cast<uint32_t>(static_cast<int32_t>(mVisibleRows))]();
        if (mItemDrawObject != nullptr) {
            CoolRect rect(mListRect.mLeft, mListRect.mTop, mListRect.mRight, static_cast<int16_t>(mListRect.mTop + mRowHeight));
            DOChange change{};
            change.mRect = &rect;
            mItemDrawObject->Change(0x10, &change);
        }
    }
    int16_t rows = mVisibleRows;
    InterfaceWindow* bar = GetWindow(1);
    if (bar != nullptr) static_cast<InterfaceScrollBar*>(bar)->SetPageValue(rows);
    if (rows != 0) {
        SetColumnFormat(0, window);
        if (attach != nullptr) attach(window);
        while (rows = static_cast<int16_t>(rows - 1), rows != 0) {
            window = makeRow();
            SetColumnFormat(rows, window);
            if (attach != nullptr) attach(window);
        }
    }
    UpdateDisplay();
}

// 0x101d2a84 (164 bytes). Preorder: the child, else the next entry, else the next entry of the nearest ancestor that has one.
// With no parent left the answer is -2 (an entry that ends the chain); with ancestors it is the last ancestor's next entry, which
// is -1 (or whatever a free-chain link holds) when none of them has one.
int32_t InterfaceListControl2::AdvanceIndex(int32_t index, int32_t* change) {
    LCWord* block = Words();
    int32_t result = -2;
    int32_t levels = 0;
    if (block[index + kLCChild].mInt >= 0) {
        result = block[index + kLCChild].mInt;
        levels = 1;
    } else if (block[index + kLCNext].mInt >= 0) {
        result = block[index + kLCNext].mInt;
    } else if (block[index + kLCParent].mInt >= 0) {
        int32_t up = block[index + kLCParent].mInt;
        levels = -1;
        while (block[up + kLCNext].mInt < 0 && block[up + kLCParent].mInt >= 0) {
            up = block[up + kLCParent].mInt;
            --levels;
        }
        result = block[up + kLCNext].mInt;
    }
    if (change != nullptr) *change = levels;
    return result;
}

// 0x101d2b64 (140 bytes). A position out of range, or no block, answers the first entry.
int32_t InterfaceListControl2::GetIndexedEntry(int32_t position) {
    int32_t walked = 0;
    int32_t entry = mFirstEntry;
    if (position > -1 && position < mItemCount && mItemStorage != nullptr) {
        while (walked < position && entry != -1) {
            entry = AdvanceIndex(entry, nullptr);
            ++walked;
        }
    }
    return entry;
}

// 0x101d2d6c (224 bytes). Walks the chain from the first entry to the first free one (or off the end). The original first runs a
// walk of 0 steps (its loop bound is 0), which does nothing.
void InterfaceListControl2::FindFreeEntry(int32_t& freeEntry, int32_t& previous) {
    int32_t found = mFirstEntry;
    if (mItemStorage == nullptr) return;
    int32_t walk = found;
    int32_t before = -1;
    bool done = false;
    while (!done) {
        if (walk < 0 || Words()[walk + kLCParent].mInt == -2) done = true;
        if (!done) {
            walk = Words()[walk + kLCNext].mInt;
            before = found;
            found = walk;
        }
    }
    freeEntry = found;
    previous = before;
}

// 0x101d2e8c (276 bytes). By the sort column: numbers compare as signed longs, strings with wcscmp. Ascending: the new item goes
// before the entry when it is smaller; descending: when the entry is smaller. A string entry cell that is NULL always says yes.
int32_t InterfaceListControl2::ShouldIInsertEntryHere(UI_LCAdd* item, tagLCDataEntry* entry) {
    const int32_t column = mSortColumn;
    if (mSortDescending == 0) {
        if (mColumns[column].mBottom == kLCNumber) {
            if (CellInt(item[column]) < CellInt(entry[column])) return 1;
        } else {
            if (entry[column].mString == nullptr) return 1;
            if (wcsncmp(item[column].mText, entry[column].mString->mRep->mData, UINT32_MAX) < 0) return 1;
        }
    } else {
        if (mColumns[column].mBottom == kLCNumber) {
            if (CellInt(entry[column]) < CellInt(item[column])) return 1;
        } else {
            if (entry[column].mString == nullptr) return 1;
            if (wcsncmp(entry[column].mString->mRep->mData, item[column].mText, UINT32_MAX) < 0) return 1;
        }
    }
    return 0;
}

// Copies an item's cells into the cells of a free entry: a string column makes a new string from the text (a NULL text leaves the
// cell as it was), any other column takes the word.
static void FillCells(InterfaceListControl2& list, LCWord* cells, const UI_LCAdd* item) {
    for (int32_t column = 0; column < list.mColumnCount; ++column) {
        if (list.mColumns[static_cast<int16_t>(column)].mBottom == kLCText) {
            if (item[column].mText != nullptr) cells[column].mString = new LCString(item[column].mText);
        } else {
            cells[column].mRaw = item[column].mRaw;
        }
    }
}

// 0x101d3000 (1,564 bytes). See the header. The block grows (20 entries the first time, then 10) when the entries in use fill it.
// A position in range finds the entry there (bVar2: the item is to become a child of it); the item is then linked as the first
// child when that entry has none, else (attribute 0x10, sorted lists only) among the children by ShouldIInsertEntryHere; the
// first entry of an empty list is entry 0. After the link the entry's data word is set, the count goes up (the first item becomes
// the current entry), the display is updated when asked (and a redraw posted when the UI auto-redraws), the scroll bar is set to
// the rows that do not fit, and the parent is told the visible range (0x405).
int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    int32_t result = -1;
    bool asChild = false;
    int32_t head = mFirstEntry;
    if (item == nullptr) return result;
    result = 0;
    if (mBlockWords <= mItemCount * (mColumnCount + 4)) {
        if (mBlockWords == 0) AllocNewBlock(0x14);
        else AllocNewBlock(10);
    }
    int32_t first;   // iVar7: the first child of the entry at `position`, or the first entry of the list
    if (position == -1 || mItemCount <= position) {
        first = mFirstEntry;
    } else {
        int32_t entry = mFirstEntry;
        int32_t walked = 0;
        if (position > -1 && position < mItemCount && mItemStorage != nullptr) {
            while (walked < position && entry != -1) {
                entry = AdvanceIndex(entry, nullptr);
                ++walked;
            }
        }
        asChild = true;
        first = Words()[entry + kLCChild].mInt;
        position = entry;
    }
    int32_t newHead = head;
    int32_t freeEntry = 0;
    int32_t previous = 0;
    if (first == -1) {
        if (asChild) {
            FindFreeEntry(freeEntry, previous);
            FillCells(*this, Words() + freeEntry + kLCCells, item);
            Words()[position + kLCChild].mInt = freeEntry;
            Words()[previous].mInt = Words()[freeEntry + kLCNext].mInt;
            Words()[freeEntry + kLCNext].mInt = -1;
            Words()[freeEntry + kLCParent].mInt = position;
        } else {
            FillCells(*this, Words() + kLCCells, item);
            freeEntry = 0;
            newHead = 0;
        }
    } else {
        FindFreeEntry(freeEntry, previous);
        FillCells(*this, Words() + freeEntry + kLCCells, item);
        if ((mAttributes & 0x10) != 0) {
            if (ShouldIInsertEntryHere(item, Words() + first + kLCCells) == 0) {
                bool linked = false;
                int32_t seen = 1;
                int32_t before;
                while (before = first, !linked && seen < mItemCount) {
                    ++seen;
                    first = Words()[before + kLCNext].mInt;
                    if (first == -1 || ShouldIInsertEntryHere(item, Words() + first + kLCCells) != 0) {
                        Words()[previous].mInt = Words()[freeEntry + kLCNext].mInt;
                        Words()[freeEntry + kLCNext].mInt = first;
                        Words()[before + kLCNext].mInt = freeEntry;
                        if (asChild) Words()[freeEntry + kLCParent].mInt = position;
                        linked = true;
                    }
                }
            } else {
                Words()[previous].mInt = Words()[freeEntry + kLCNext].mInt;
                Words()[freeEntry + kLCNext].mInt = first;
                newHead = freeEntry;
                if (asChild) {
                    Words()[position + kLCChild].mInt = freeEntry;
                    Words()[freeEntry + kLCParent].mInt = position;
                    newHead = head;
                }
            }
        }
    }
    LCWord* parentWord = &Words()[freeEntry + kLCParent];
    if (parentWord->mInt < 0) parentWord->mInt = -1;
    Words()[freeEntry + kLCData].mInt = data;
    mFirstEntry = newHead;
    ++mItemCount;
    if (mItemCount == 1) ChangeCurEntry(0);
    if (update != 0) {
        UpdateDisplay();
        if (gUIAutoRedraw != 0) UI_PostMessage(this, 3, 0, 0);
    }
    const int32_t top = mTopEntry;
    const int16_t rows = mVisibleRows;
    const int32_t count = mItemCount;
    InterfaceWindow* bar = GetWindow(1);
    if (bar != nullptr) {
        if (rows < count) {
            bar->Show(1);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(count - rows, 0);
            bar->SetValue(top);
        } else {
            bar->Show(0);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0);
        }
    }
    int32_t last = mTopEntry + mVisibleRows;
    if (mItemCount <= last) last = mItemCount - 1;
    UI_SendMessage(mParent, 0x405, mTopEntry, last);
    return result;
}

// 0x101d3660 (368 bytes). The columns named by the bits of `columns` (a shift count of 32 or more gives no bit, as the PowerPC's
// slw): a string column assigns the text to the existing string (nothing when either is NULL), another column takes the word.
void InterfaceListControl2::ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused) {
    bool changed = false;
    if (mItemStorage == nullptr || item == nullptr || position >= mItemCount) return;
    int32_t entry = mFirstEntry;
    int32_t walked = 0;
    if (position > -1 && position < mItemCount && mItemStorage != nullptr) {
        while (walked < position && entry != -1) {
            entry = AdvanceIndex(entry, nullptr);
            ++walked;
        }
    }
    if (entry >= 0) {
        LCWord* cells = Words() + entry + kLCCells;
        for (int32_t column = 0; column < mColumnCount; ++column) {
            const uint32_t shift = static_cast<uint32_t>(column) & 0x3f;
            const uint32_t bit = shift < 32 ? 1u << shift : 0u;
            if ((static_cast<uint32_t>(columns) & bit) != 0) {
                changed = true;
                if (mColumns[static_cast<int16_t>(column)].mBottom == kLCText) {
                    const char16_t* text = item[column].mText;
                    if (text != nullptr && cells[column].mString != nullptr) {
                        cells[column].mString->assign(text, wcslen(text));
                    }
                } else {
                    cells[column].mRaw = item[column].mRaw;
                }
            }
        }
    }
    if (changed && update != 0) UpdateDisplay();
}

// 0x101d41f0 (196 bytes). 1 with the entry's data word, 0 when the position (in range, else the first entry) names no entry.
int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    int32_t walked = 0;
    int32_t entry = mFirstEntry;
    if (position > -1 && position < mItemCount && mItemStorage != nullptr) {
        while (walked < position && entry != -1) {
            entry = AdvanceIndex(entry, nullptr);
            ++walked;
        }
    }
    if (entry == -1) return 0;
    if (data != nullptr) *data = Words()[entry + kLCData].mInt;
    return 1;
}

// 0x101d4854 (448 bytes). The selection bar follows: hidden when the current entry goes to -1, shown when it leaves -1 in a visible
// list, then placed over the row (Change flag 0x10; the top and bottom are 32-bit sums cut to 16 bits by the rectangle).
void InterfaceListControl2::ChangeCurEntry(int32_t row) {
    int32_t target;
    if (row < 0) {
        target = -1;
    } else if (row < mVisibleRows && row < mItemCount) {
        target = row + mTopEntry;
    } else {
        if ((mAttributes & 0x100) == 0) return;
        target = -1;
    }
    if (target != mCurrentEntry) {
        if (mItemDrawObject != nullptr) {
            if (target == -1 && mCurrentEntry != -1) {
                mItemDrawObject->Show(0);
            } else if (target != -1 && mCurrentEntry == -1 && GetVisibility() != 0) {
                mItemDrawObject->Show(1);
            }
        }
        mCurrentEntry = target;
        UI_PostMessage(mParent, 0x401, mId, mCurrentEntry);
        if (gUIAutoRedraw != 0) UI_PostMessage(this, 3, 0, 0);
    }
    if (mItemDrawObject != nullptr && mCurrentEntry != -1) {
        const int32_t y1 = mListRect.mTop + mRowHeight * (mCurrentEntry - mTopEntry);
        const int32_t y2 = mListRect.mTop + mRowHeight * (mCurrentEntry + 1 - mTopEntry);
        CoolRect rect(mListRect.mLeft, static_cast<int16_t>(y1), mListRect.mRight, static_cast<int16_t>(y2));
        DOChange change{};
        change.mRect = &rect;
        mItemDrawObject->Change(0x10, &change);
    }
}
