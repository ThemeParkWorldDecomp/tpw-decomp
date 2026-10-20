// InterfaceListControl2: GetColumnPos, GetItem, SetData, GetNonHierIndexedEntry and the list's default message function (unit v5955).
#include "ui_controls.h"
#include <cstring>
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "UI_Base.h"
#include "InterfaceWindow.h"

// 0x102afbcc: the keys the list asks the keyboard capture for. The Mac's first word is 0x23000000; the rest of the table is a guess
// (the callback only passes the address on).
captureKeyList gListControl2Keys[] = {{0x2300, 0}, {0x2400, 0}, {0x2600, 0}, {0x2800, 0}, {0, -1}};

// 0x101d42ec: the column's left and right edges (the record's +4 and +8).
bool InterfaceListControl2::GetColumnPos(int32_t column, int16_t* left, int16_t* right) {
    if (mColumns != nullptr && left != nullptr && right != nullptr && column < mColumnCount) {
        *left = mColumns[column].mLeft;
        *right = mColumns[column].mRight;
        return true;
    }
    return false;
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
