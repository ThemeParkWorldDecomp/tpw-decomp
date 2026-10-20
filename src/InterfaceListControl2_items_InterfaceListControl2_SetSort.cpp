// InterfaceListControl2: FindData 0x101d4384, PropagateMessage 0x101d4650, SelectClick 0x101d4a50, SetCurrent 0x101d4c64,
// SetSort 0x101d4db8, ColumnButtonClick 0x101d4fa4 (unit v5908).
#include "ui_controls.h"
#include <cstring>
#include "platform/msl.h"
#include "ui_DrawObject.h"

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
