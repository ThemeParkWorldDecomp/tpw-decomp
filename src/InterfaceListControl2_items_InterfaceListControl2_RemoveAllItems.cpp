// InterfaceListControl2: RemoveAllItems 0x101d3cc8, RemoveItem 0x101d39f0, RemoveMe 0x101d3818 (unit v5095), main app.
#include "InterfaceScrollBar.h"
#include "UI_Base.h"
#include "ui_controls.h"

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
