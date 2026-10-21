// InterfaceListControl2.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceListControl2 code, named after it.

#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "online.h"
#include "platform/msl.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ime.h"
#include <cstdint>
#include <cstring>

// List control drawing and teardown (main app, unit w37b).

// 0x101d154c (68 bytes).
void InterfaceListControl2::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
    mListRect.mLeft = mClientRect.mLeft;
    mListRect.mTop = mClientRect.mTop;
    mListRect.mRight = mClientRect.mRight;
    mListRect.mBottom = mClientRect.mBottom;
}

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceListControl2::InterfaceListControl2(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left,
                                             int16_t top, int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mCurrentEntry = -1;
    int32_t barAttributes = 0x10;
    mF144 = 0;
    mF148 = 0;
    mF174 = 0;
    mRowHeight = 0;
    mVisibleRows = 0;
    mTopEntry = 0;
    mItemCount = 0;
    mBlockWords = 0;
    mFirstEntry = -1;
    mColumnCount = 0;
    mF166 = 0;
    mColumns = nullptr;
    mSortColumn = 0;
    mSortDescending = 0;
    CalcClientArea();
    if ((attributes & 0x1000) != 0) {
        if ((attributes & 0x40) != 0) barAttributes |= 0x40;
        InterfaceWindow* bar = CreateObject(3, barAttributes, 1, static_cast<int16_t>(mClientRect.mRight + 4), mRect.mTop,
                                            mRect.mRight, mRect.mBottom);
        if (bar != nullptr) {
            bar->Enable(0);
            bar->Show(1);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0);
        }
    }
    mCallback = UI_DefaultListControl2Callback;
    mDefaultCallback = UI_DefaultListControl2Callback;
}

// 0x101d1768 (380 bytes). The shared draw object is released after the item arrays.
InterfaceListControl2::~InterfaceListControl2() {
    RemoveAllItems();
    delete[] mColumns;
    if (mItemWindows != nullptr) {
        for (int32_t i = 0; i < mVisibleRows; ++i) delete mItemWindows[i];
        delete[] mItemWindows;
    }
    delete[] mItemStorage;
    if (mItemDrawObject != nullptr) {
        InterfaceDrawObject* object = mItemDrawObject;
        object->mResizable = static_cast<int16_t>(object->mResizable - 1);
        object->Show(0);
        if (static_cast<int16_t>(object->mResizable - (1 - object->mInLibrary)) == -1) {
            delete mItemDrawObject;
        }
    }
}

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceListControl2::WhatAmI() { return kUIKindListControl; }   // 0x101d1918

// Unit v5203: InterfaceListControl2::Sort 0x101d1958 (1320 bytes), InterfaceListControl2::FindString 0x101d44b8 (348 bytes).

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

// The list control's item management (main app, unit w93): the block of entries, the display of the visible rows, the column
// formats, adding and changing items, and the current entry.
//
// The item block (mItemStorage) is an array of words (LCWord, 4 bytes on the Mac). An entry is mColumnCount + 4 words: the next
// entry, the caller's data word, the first child and the parent (-1 none, -2 for a free entry), then one cell per column. An entry
// is named by the index of its first word. AllocNewBlock chains every new entry to the next one, so the chain runs through the used
// entries first and the free ones after them; AddItem takes the first free entry.

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

// The interface's control classes: the setters the window script calls (main app, unit w31a). InterfaceKnob::SetMinMaxAngles,
// InterfaceListBox::SetListRect, InterfaceListControl2's SetListRect, SetColumns and SetColumnPos, and the constructors of the
// three region shapes a window can be given (CoolCircle, CoolEllipse, CoolPoly; src/ui_cool_shapes.cpp has the rest of them).

void InterfaceListControl2::SetColumns(int16_t count) {
    if (mColumnCount != 0) return;
    mColumns = new CoolRect[count];
    for (int32_t i = 0; i < count; ++i)
        mColumns[i].Set(static_cast<int16_t>(i << 5), 0, static_cast<int16_t>((i + 1) << 5), 1);
    mColumnCount = count;
}

void InterfaceListControl2::SetColumnPos(int16_t column, int16_t left, int16_t right) {
    if (column < mColumnCount && mColumns != nullptr) mColumns[column].Set(left, 0, right, 1);
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

// InterfaceListControl2: GetColumnPos, GetItem, SetData, GetNonHierIndexedEntry and the list's default message function (unit v5955).

// 0x102afbcc: the keys the list asks the keyboard capture for. The Mac's first word is 0x23000000; the rest of the table is a guess
// (the callback only passes the address on).
captureKeyList gListControl2Keys[] = {{0x2300, 0}, {0x2400, 0}, {0x2600, 0}, {0x2800, 0}, {0, -1}};

// 0x101d2c30: the entry `position` siblings along (links only) from the first child of the entry at `parent` (the chain's first
// entry for a parent of -1 or less).
int32_t InterfaceListControl2::GetNonHierIndexedEntry(int32_t position, int32_t parent) {
    int32_t counted = 0;
    int32_t entry;
    if (parent > -1) {
        int32_t walked = 0;
        int32_t node = mFirstEntry;
        entry = -1;
        if (parent > -1 && parent < mItemCount && mItemStorage != nullptr) {
            while (walked < parent && node != -1) {
                node = AdvanceIndex(node, nullptr);
                ++walked;
            }
        }
        if (node > -1) entry = Words()[node + kLCChild].mInt;
    } else {
        entry = mFirstEntry;
    }
    if (position > -1 && position < mItemCount && mItemStorage != nullptr) {
        while (counted < position && entry > -1) {
            ++counted;
            entry = Words()[entry + kLCNext].mInt;
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

// InterfaceListControl2: RemoveAllItems 0x101d3cc8, RemoveItem 0x101d39f0, RemoveMe 0x101d3818 (unit v5095), main app.

namespace online_callee {
void WStringDtor(void* self, int16_t flag);   // 0x101468f8 std::basic_string<wchar_t>::~basic_string
}

namespace {

// The tail both removals share: shows the scroll bar child (id 1) when more entries than rows exist, sets its range and value.
void UpdateScrollBar(InterfaceListControl2* list) {
    int32_t top = list->mTopEntry;
    int16_t rows = list->mVisibleRows;
    int32_t count = list->mItemCount;
    InterfaceWindow* window = list->GetWindow(1);
    if (window != nullptr) {
        if (count > rows) {
            window->Show(1);
            static_cast<InterfaceScrollBar*>(window)->SetMinMax(count - rows, 0);
            window->SetValue(top);   // vtable slot +0x24
        } else {
            window->Show(0);
            static_cast<InterfaceScrollBar*>(window)->SetMinMax(0, 0);
        }
    }
}

}  // namespace

// 0x101d3818 (412 bytes). QUIRK: with no used entry before the free one (previous -1) the original writes before the block.
void InterfaceListControl2::RemoveMe(int32_t position, int32_t entry, int32_t previous, int32_t removeNext) {
    int32_t child = Words()[entry + kLCChild].mInt;
    if (child >= 0) RemoveMe(position, child, -1, 1);
    if (removeNext != 0) {
        int32_t next = Words()[entry].mInt;
        if (next >= 0) RemoveMe(position, next, entry, 1);
    }
    for (int32_t column = 0; column < mColumnCount; ++column) {
        if (mColumns[static_cast<int16_t>(column)].mBottom == 0)
            online_callee::WStringDtor(Words()[entry + column + kLCCells].mString, 1);
    }
    int32_t freeEntry;
    int32_t usedBefore;
    FindFreeEntry(freeEntry, usedBefore);
    if (usedBefore != entry) {
        if (previous >= 0) {
            Words()[previous].mInt = Words()[entry].mInt;
        } else {
            int32_t parent = Words()[entry + kLCParent].mInt;
            if (parent >= 0) Words()[parent + kLCChild].mInt = -1;
        }
        Words()[entry].mInt = freeEntry;
        Words()[usedBefore].mInt = entry;
    }
    Words()[entry + kLCChild].mInt = -1;
    Words()[entry + kLCParent].mInt = -2;
    mItemCount = mItemCount - 1;
    if (mItemCount == 0) mFirstEntry = -1;
}
// 0x101d39f0 (672 bytes).
int32_t InterfaceListControl2::RemoveItem(int32_t position) {
    if (position >= mItemCount || mItemStorage == nullptr || position < 0) return 0;
    int32_t entry = mFirstEntry;
    int32_t newFirst = entry;
    int32_t steps = 0;
    while (steps < position && entry != -1) {
        entry = AdvanceIndex(entry, nullptr);
        ++steps;
    }
    int32_t previous = -1;
    if (position > 0) {
        LCWord* words = Words();
        int32_t parent = words[entry + kLCParent].mInt;
        if (parent < 0 || entry != words[parent + kLCChild].mInt) {
            int32_t walk = mFirstEntry;
            int32_t count = 0;
            if (position - 1 >= 0 && position - 1 < mItemCount && words != nullptr) {
                while (count < position - 1 && walk != -1) {
                    walk = AdvanceIndex(walk, nullptr);
                    ++count;
                }
            }
            previous = walk;
        }
    } else {
        newFirst = Words()[entry].mInt;
    }
    RemoveMe(position, entry, previous, 0);
    mFirstEntry = newFirst;
    if (mCurrentEntry == position) {
        int32_t row = mCurrentEntry;
        if (mItemCount > 0) {
            if (!(mCurrentEntry < mItemCount)) row = row - 1;
        } else {
            row = -1;
        }
        ChangeCurEntry(row);
    }
    if (gUIAutoRedraw != 0 && position >= mTopEntry && position < mTopEntry + mVisibleRows)
        UI_PostMessage(this, 3, 0, 0);
    UpdateScrollBar(this);
    int32_t last = mTopEntry + mVisibleRows;
    if (last >= mItemCount) last = mItemCount - 1;
    UI_SendMessage(mParent, 0x405, mTopEntry, last);
    UpdateDisplay();
    return 1;
}

// 0x101d3cc8 (712 bytes). Frees the string of every text cell of every entry, rebuilds the free chain over the whole block, empties
// the list. QUIRK: the first loop of the original (a walk of 0 steps) never runs; the walk to each entry is bounded by the count.
void InterfaceListControl2::RemoveAllItems() {
    if (mItemCount == 0) return;
    for (int32_t position = 0; position < mItemCount; ++position) {
        int32_t entry = mFirstEntry;
        int32_t steps = 0;
        if (position >= 0 && position < mItemCount && mItemStorage != nullptr) {
            while (steps < position && entry != -1) {
                entry = AdvanceIndex(entry, nullptr);
                ++steps;
            }
        }
        for (int32_t column = 0; column < mColumnCount; ++column) {
            if (mColumns[static_cast<int16_t>(column)].mBottom == 0)
                online_callee::WStringDtor(Words()[entry + kLCCells + column].mString, 1);
        }
    }
    int32_t entries = mBlockWords / (mColumnCount + 4);
    int32_t index = 0;
    for (int32_t i = 0; i < entries - 1; ++i) {
        Words()[index].mInt = index + mColumnCount + 4;
        Words()[index + kLCChild].mInt = -1;
        Words()[index + kLCParent].mInt = -2;
        index = index + mColumnCount + 4;
    }
    Words()[index].mInt = -1;
    Words()[index + kLCChild].mInt = -1;
    Words()[index + kLCParent].mInt = -2;
    if (gUIAutoRedraw != 0) UI_PostMessage(this, 3, 0, 0);
    mItemCount = 0;
    mCurrentEntry = -1;
    mFirstEntry = -1;
    mTopEntry = 0;
    UpdateScrollBar(this);
    if (mItemDrawObject != nullptr) mItemDrawObject->Show(0);   // vtable slot +0x1c
    UpdateDisplay();
}

// 0x101d3fcc: the data word of the entry at `position`. QUIRK: an entry of -2 (no entry after the end) writes before the block.
void InterfaceListControl2::SetData(int32_t position, int32_t data) {
    int32_t walked = 0;
    int32_t entry = mFirstEntry;
    if (position > -1 && position < mItemCount && mItemStorage != nullptr) {
        while (walked < position && entry != -1) {
            entry = AdvanceIndex(entry, nullptr);
            ++walked;
        }
    }
    if (entry == -1) return;
    Words()[entry + kLCData].mInt = data;
}

// 0x101d40bc: the cell of column `column` of the entry at `position`; a text column hands back the characters of its string.
void InterfaceListControl2::GetItem(int32_t position, int32_t column, UI_LCAdd& out) {
    int32_t walked = 0;
    int32_t entry = mFirstEntry;
    if (position >= 0 && position < mItemCount && mItemStorage != nullptr) {
        while (walked < position && entry != -1) {
            entry = AdvanceIndex(entry, nullptr);
            ++walked;
        }
    }
    if (entry < 0 || column >= mColumnCount) return;
    LCWord* cell = &Words()[entry + column + kLCCells];
    if (mColumns[static_cast<int16_t>(column)].mBottom == 0)
        out.mText = cell->mString->mRep->mData;
    else
        out.mInt = cell->mInt;
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

// 0x101d42ec: the column's left and right edges (the record's +4 and +8).
bool InterfaceListControl2::GetColumnPos(int32_t column, int16_t* left, int16_t* right) {
    if (mColumns != nullptr && left != nullptr && right != nullptr && column < mColumnCount) {
        *left = mColumns[column].mLeft;
        *right = mColumns[column].mRight;
        return true;
    }
    return false;
}

// InterfaceListControl2: FindData 0x101d4384, PropagateMessage 0x101d4650, SelectClick 0x101d4a50, SetCurrent 0x101d4c64,
// SetSort 0x101d4db8, ColumnButtonClick 0x101d4fa4 (unit v5908).

// The virtual SetValue of the window (slot 0x24 of the table at +0x118); the test library defines it again.
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }

namespace {

// labs of the C library: the most negative number stays as it is (wrapping negation).
int32_t MacLabs(int32_t value) { return value < 0 ? -value : value; }

// The data word of the entry at `index`: the word after the entry's own (the original reads storage + 4 * index + 4).
int32_t EntryData(const uint8_t* storage, int32_t index) {
    return *reinterpret_cast<const int32_t*>(storage + index * 4 + 4);
}

}  // namespace

// 0x101d4384 (252 bytes). The position of the first entry (in display order) whose data word is `data`, -1 for none. The original's
// first loop (before the search) never runs: its counter starts at 0 and the loop wants it negative.
int32_t InterfaceListControl2::FindData(int32_t data) {
    int32_t entry = mFirstEntry;
    int32_t position = -1;
    bool found = false;
    while (entry != -1 && !found && position < mItemCount - 1) {
        position = position + 1;
        if (data == EntryData(mItemStorage, entry)) found = true;
        entry = AdvanceIndex(entry, nullptr);
    }
    return found ? position : -1;
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

// 0x101d4650 (352 bytes). Sends the message to every cell of the visible rows. With message 0x1e the second argument becomes
// (row << 16 | column) and the third the data word of the next entry in display order (the arguments keep their last values).
void InterfaceListControl2::PropagateMessage(int32_t message, int32_t a, int32_t b) {
    if (mItemWindows == nullptr) return;
    int32_t entry = mFirstEntry;
    for (int32_t row = 0; row < mVisibleRows; row++) {
        InterfaceWindow* window = mItemWindows[row];
        if (window == nullptr) continue;
        if (message == 0x1e && entry != -1) {
            b = EntryData(mItemStorage, entry);
            entry = AdvanceIndex(entry, nullptr);
        }
        for (int32_t column = 0; column < mColumnCount; column++) {
            if (message == 0x1e) a = static_cast<int32_t>(static_cast<uint32_t>(row) << 16) | column;
            UI_SendMessage(window, message, a, b);
            if (column == 0) {
                window = window->GetChild(0);
            } else {
                window = window->GetNext();
            }
        }
    }
}

void InterfaceListControl2::SetListRect(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    mListRect.Set(left, top, right, bottom);
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

// 0x101d4a50 (472 bytes). A click at (x, y): inside the list rectangle it makes the row under it current (a row above the first
// is not taken); with a current entry, the column under x is posted (0x404), and with attribute 0x80 the current entry too (0x400).
void InterfaceListControl2::SelectClick(int16_t x, int16_t y) {
    if (mRowHeight != 0 && mListRect.PointIn(x, y) != 0) {
        int32_t row = (static_cast<int32_t>(y) - static_cast<int32_t>(mListRect.mTop)) / static_cast<int32_t>(mRowHeight);
        if (row >= 0) ChangeCurEntry(row);
    }
    if (mCurrentEntry >= 0) {
        for (int32_t column = 0; column < mColumnCount; column++) {
            const CoolRect& rect = mColumns[column];
            if (x > rect.mLeft && x < rect.mRight) {
                UI_PostMessage(mParent, 0x404, mId, column);
                break;
            }
        }
    }
    if ((mAttributes & 0x80) != 0 && mCurrentEntry >= 0) UI_PostMessage(mParent, 0x400, mId, mCurrentEntry);
}

// 0x101d4c64 (284 bytes). Makes the entry at `position` current and scrolls so that it shows; tells the scroll bar (child 1) and,
// when the first visible entry moved, the parent (0x405: first and last visible).
void InterfaceListControl2::SetCurrent(int32_t position) {
    if (mItemStorage == nullptr) return;
    int32_t oldTop = mTopEntry;
    if (position < 0) {
        ChangeCurEntry(-1);
    } else {
        if (position >= mItemCount) position = mItemCount - 1;
        if (position < oldTop || position >= oldTop + mVisibleRows) {
            mTopEntry = position;
            if (mTopEntry + mVisibleRows > mItemCount) mTopEntry = mItemCount - mVisibleRows;
        }
        ChangeCurEntry(position - mTopEntry);
    }
    UpdateDisplay();
    InterfaceWindow* bar = GetWindow(1);
    WindowSetValue(bar, mTopEntry);
    if (oldTop != mTopEntry) {
        int32_t last = mTopEntry + mVisibleRows;
        if (last >= mItemCount) last = mItemCount - 1;
        UI_SendMessage(mParent, 0x405, mTopEntry, last);
    }
}

// 0x101d4db8 (436 bytes). Sets the sort column (|column| - 1, as a short) and direction (descending when column is negative);
// tells the parent (0x406: column + 1, negated when descending) for each change, and sorts when something changed and `sortNow`.
void InterfaceListControl2::SetSort(int32_t column, bool sortNow) {
    int32_t wanted = MacLabs(column) - 1;
    bool columnChanged = false;
    if (static_cast<int16_t>(wanted) != mSortColumn) {
        mSortColumn = static_cast<int16_t>(wanted);
        int32_t value = mSortColumn + 1;
        if (mSortDescending == 1) value = -value;
        UI_SendMessage(mParent, 0x406, mId, value);
        columnChanged = true;
    }
    int16_t wantDescending = column < 0 ? 1 : 0;
    bool directionChanged = false;
    if (wantDescending != mSortDescending) {
        mSortDescending = wantDescending;
        int32_t value = mSortColumn + 1;
        if (mSortDescending == 1) value = -value;
        UI_SendMessage(mParent, 0x406, mId, value);
        directionChanged = true;
    }
    if ((directionChanged || columnChanged) && sortNow) Sort(-1);
}

// 0x101d4fa4 (424 bytes). A click on column button `id` (column id - 0x10) of a list with attribute 0x10: the same column again
// turns the direction (unless it is -1); another column sets the direction to 0 and becomes the sort column. Then sorts, updates
// the display and posts the current entry (0x401).
void InterfaceListControl2::ColumnButtonClick(int32_t id) {
    int16_t column = static_cast<int16_t>(id - 0x10);
    if ((mAttributes & 0x10) == 0) return;
    if (column < 0) return;
    if (column >= mColumnCount) return;
    if (column == mSortColumn) {
        if (mSortDescending != -1) {
            mSortDescending = mSortDescending == 0 ? 1 : 0;
            int32_t value = mSortColumn + 1;
            if (mSortDescending == 1) value = -value;
            UI_SendMessage(mParent, 0x406, mId, value);
        }
    } else {
        if (mSortDescending != 0) {
            mSortDescending = 0;
            int32_t value = mSortColumn + 1;
            if (mSortDescending == 1) value = -value;
            UI_SendMessage(mParent, 0x406, mId, value);
        }
        if (column != mSortColumn) {
            mSortColumn = column;
            int32_t value = mSortColumn + 1;
            if (mSortDescending == 1) value = -value;
            UI_SendMessage(mParent, 0x406, mId, value);
        }
    }
    Sort(-1);
    UpdateDisplay();
    if (mCurrentEntry >= 0) UI_PostMessage(mParent, 0x401, mId, mCurrentEntry);
}

// ---- unit w50

// The Z of the first item window (the word at +0x148 points at an array of window pointers; the first is read), 0 without one.
int16_t InterfaceListControl2::GetZOfEntries() {
    if (mItemWindows != nullptr && *mItemWindows != nullptr) return (*mItemWindows)->mZ;
    return 0;
}

// The row under a point of the list rectangle (the caller has tested mRowHeight).
static int32_t ListRowAt(InterfaceListControl2* list, int16_t y) {
    return (static_cast<int32_t>(y) - list->mListRect.mTop) / list->mRowHeight;
}

// A click or a move over the list (messages 0x10001 and 0x10003): the row under the pointer becomes the current one.
static void ListPointerRow(InterfaceListControl2* list, int32_t where) {
    if ((list->mAttributes & 0x80) == 0 || list->mButtonsHeld != 0) return;
    int16_t x = static_cast<int16_t>(where >> 16);
    int16_t y = static_cast<int16_t>(where);
    if (!list->mClientRect.PointIn(x, y)) return;
    if (list->mRowHeight == 0) return;
    if (!list->mListRect.PointIn(x, y)) return;
    int32_t row = ListRowAt(list, y);
    if (row >= 0) list->ChangeCurEntry(row);
}

// 0x101d51f0: the list control's message function. Message 0x10003 answers 0 without the default function.
int32_t UI_DefaultListControl2Callback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window);
    switch (message) {
    case 0x800: {   // the scroll bar moved
        InterfaceWindow* bar = list->GetWindow(1);
        if (bar != nullptr) {
            int32_t top = bar->GetValue();
            if (top != list->mTopEntry) {
                list->mTopEntry = top;
                list->UpdateDisplay();
                int32_t last = list->mTopEntry + list->mVisibleRows;
                if (last >= list->mItemCount) last = list->mItemCount - 1;
                UI_SendMessage(list->mParent, 0x405, list->mTopEntry, last);
                if (gUIAutoRedraw != 0) UI_PostMessage(list, 3, 0, 0);
            }
        }
        break;
    }
    case 0x100:
        list->ColumnButtonClick(a);
        break;
    case 0x1e:
        list->PropagateMessage(message, a, b);
        break;
    case 0x15:
    case 0x1a:
        if ((list->GetAttr() & 0x200) != 0) list->CaptureKeys(gListControl2Keys);
        break;
    case 0x13: {   // moved
        int16_t dx = static_cast<int16_t>(a);
        int16_t dy = static_cast<int16_t>(b);
        list->mListRect.Shift(dx, dy);
        if (list->mColumns != nullptr) {
            for (int32_t i = list->mColumnCount; i != 0; --i) list->mColumns[i - 1].Shift(dx, dy);
        }
        break;
    }
    case 0x11: {   // shown or hidden
        if (list->mItemWindows != nullptr) {
            for (int32_t i = 0; i < list->mVisibleRows; ++i) {
                int16_t on = 0;
                if (list->mTopEntry + i < list->mItemCount && static_cast<int16_t>(a) != 0) on = 1;
                InterfaceWindow* row = list->mItemWindows[i];
                if (row != nullptr) row->Show(on);
            }
        }
        if (list->mItemDrawObject != nullptr) {
            if (list->mCurrentEntry != -1 && static_cast<int16_t>(a) != 0)
                list->mItemDrawObject->Show(1);
            else
                list->mItemDrawObject->Show(0);
        }
        break;
    }
    case 0x10001:   // the pointer went down over the list (the position is in `a`)
        ListPointerRow(list, a);
        break;
    case 0x10003:   // the pointer moved over the list: the one path that does not reach the default function
        ListPointerRow(list, a);
        return 0;
    case 0x1000a: {   // a key
        if ((list->mAttributes & 0x200) != 0) {
            switch (static_cast<int16_t>(a)) {
            case 0x2600: list->SetCurrent(list->mCurrentEntry - 1); break;
            case 0x2400: list->SetCurrent(0); break;
            case 0x2300: list->SetCurrent(list->mItemCount - 1); break;
            case 0x2800: list->SetCurrent(list->mCurrentEntry + 1); break;
            default: break;
            }
        }
        break;
    }
    case 0x1000d: {   // the wheel
        InterfaceWindow* bar = list->GetWindow(1);
        if (bar != nullptr) {
            int32_t top = bar->GetValue();
            bar->SetValue(top - a);
        }
        break;
    }
    case 0x11006:   // a click (a == 0) or a double click; the position is in `b`
        if (a == 0) {
            list->SelectClick(static_cast<int16_t>(b >> 16), static_cast<int16_t>(b));
        } else if ((list->mAttributes & 0x80) != 0) {
            if (list->mRowHeight != 0) {
                int16_t x = static_cast<int16_t>(b >> 16);
                int16_t y = static_cast<int16_t>(b);
                if (list->mListRect.PointIn(x, y)) {
                    int32_t row = ListRowAt(list, y);
                    if (row >= 0) list->ChangeCurEntry(row);
                }
            }
            if (list->mCurrentEntry >= 0) UI_PostMessage(list->mParent, 0x402, list->mId, list->mCurrentEntry);
        }
        break;
    case 0x11007:
        if (a == 0 && list->mCurrentEntry >= 0) UI_PostMessage(list->mParent, 0x400, list->mId, list->mCurrentEntry);
        break;
    default:
        break;
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}
