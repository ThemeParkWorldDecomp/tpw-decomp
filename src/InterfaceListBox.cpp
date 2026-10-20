// List box drawing, selection and entry-tree teardown (main app, unit w37b).
#include "ui_controls.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

#include "InterfaceFont.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "TbColour.h"

// 0x1018a3a0: the original is empty.
void InterfaceListBox::DrawFunction() {}

// 0x101d0b98 (128 bytes): the scroll bar's strip is reserved unconditionally.
void InterfaceListBox::CalcClientArea() {
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = static_cast<int16_t>(mRect.mRight - 0x30);
    mClientRect.mBottom = mRect.mBottom;
    mListRect.mLeft = static_cast<int16_t>(mClientRect.mLeft + 4);
    mListRect.mTop = static_cast<int16_t>(mClientRect.mTop + 4);
    mListRect.mRight = static_cast<int16_t>(mClientRect.mRight - 4);
    mListRect.mBottom = static_cast<int16_t>(mClientRect.mBottom - 4);
}

// 0x101d0dc4 (132 bytes). Member and base destruction follow the C++ body.
InterfaceListBox::~InterfaceListBox() { RemoveAllItems(); }

// 0x101d0eb0 (104 bytes). A negative index selects the root when it is below the count.
tagLBEntry* InterfaceListBox::GetIndexedEntry(int32_t index) {
    if (index >= mF14C || mEntries == nullptr) return nullptr;
    tagLBEntry* entry = mEntries;
    for (int32_t i = 0; i < index; ++i) {
        if (entry->mChild != nullptr) {
            entry = entry->mChild;
        } else {
            while (entry->mNext == nullptr) entry = entry->mParent;
            entry = entry->mNext;
        }
    }
    return entry;
}

// 0x101d0f50 (328 bytes). The Mac unrolls five depths of this postorder traversal.
int32_t recurseDelete(tagLBEntry* entry) {
    int32_t count = 0;
    while (entry != nullptr) {
        if (entry->mChild != nullptr) count += recurseDelete(entry->mChild);
        tagLBEntry* next = entry->mNext;
        delete entry;
        ++count;
        entry = next;
    }
    return count;
}

// 0x101d10c8 (176 bytes): the scroll offset and row metrics are left alone.
void InterfaceListBox::RemoveAllItems() {
    recurseDelete(mEntries);
    mF14C = 0;
    mF148 = -1;
    mEntries = nullptr;
    if (gUIAutoRedraw != 0) UI_PostMessage(this, 3, 0, 0);
    auto* bar = static_cast<InterfaceScrollBar*>(GetWindow(1));
    if (bar != nullptr) {
        bar->Enable(0);
        bar->SetMinMax(0, 0);
    }
}

// 0x101d1210 (760 bytes). Row indices are checked only against their upper bounds.
int32_t UI_DefaultListBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    auto* box = static_cast<InterfaceListBox*>(window);
    auto select = [&](int16_t y) {
        if (box->mF154 == 0) return;
        int32_t row = (y - box->mListRect.mTop) / box->mF154;
        row = row < box->mF156 && row < box->mF14C ? row + box->mF150 : -1;
        if (row == box->mF148) return;
        box->mF148 = row;
        if (box->mF148 >= 0) UI_PostMessage(box->mParent, 0x201, box->mId, box->mF148);
        if (gUIAutoRedraw != 0) UI_PostMessage(box, 3, 0, 0);
    };
    switch (message) {
    case 0x13:
        box->mListRect.Shift(static_cast<int16_t>(a), static_cast<int16_t>(b));
        break;
    case 0x800: {
        InterfaceWindow* bar = box->GetWindow(1);
        if (bar != nullptr) {
            const int32_t value = bar->GetValue();
            if (value != box->mF150) {
                box->mF150 = value;
                if (gUIAutoRedraw != 0) UI_PostMessage(box, 3, 0, 0);
            }
        }
        break;
    }
    case 0x10003:
        if ((box->mAttributes & 0x80) != 0 &&
            box->mClientRect.PointIn(static_cast<int16_t>(static_cast<uint32_t>(a) >> 16), static_cast<int16_t>(a)) != 0) {
            select(static_cast<int16_t>(a));
        }
        break;
    case 0x11006:
        if ((box->mAttributes & 0x80) == 0) {
            select(static_cast<int16_t>(b));
        } else if (box->mF148 >= 0) {
            UI_PostMessage(box->mParent, 0x200, box->mId, box->mF148);
        }
        break;
    case 0x11007:
        if (box->mF148 >= 0) UI_PostMessage(box->mParent, 0x200, box->mId, box->mF148);
        break;
    default:
        break;
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}

// 0x1018a3dc (1088 bytes). Indentation is cleared entirely when walking up to a sibling.
void InterfaceListBox::ClientDraw() {
    int16_t left, top, right, bottom;
    mClientRect.Get(&left, &top, &right, &bottom);
    mListRect.Get(&left, &top, &right, &bottom);
    auto xPixel = [](int32_t x) { return (x * gUIRealScreenWidth) / 2048; };
    auto yPixel = [](int16_t y) { return (int32_t(y) * gUIRealScreenHeight) / 1536; };
    TbRect clip{xPixel(left), yPixel(top), xPixel(right), yPixel(bottom)};
    LbPalette_FindColour(&_lbGlobalPalette, 0, 0, 0);
    LbPalette_FindColour(&_lbGlobalPalette, 255, 255, 255);
    TbColour selected;
    selected.mAlpha = 255;
    selected.mRed = selected.mGreen = selected.mBlue = 128;
    selected.mIndex = LbPalette_FindColour(&_lbGlobalPalette, 128, 128, 128);
    TbColour textColour;
    textColour.mAlpha = textColour.mRed = textColour.mGreen = textColour.mBlue = 255;
    textColour.mIndex = LbPalette_FindColour(&_lbGlobalPalette, 255, 255, 255);
    tagLBEntry* entry = GetIndexedEntry(mF150);
    int32_t y = mListRect.mTop;
    selected.mRed = 200;
    selected.mGreen = selected.mBlue = 0;
    selected.mIndex = LbPalette_FindColour(&_lbGlobalPalette, 200, 0, 0);
    if (CFont::Set(CFont::kFont1, CFont::kOneColour) == 0) return;
    char indent[32];
    std::sprintf(indent, "%s", "");
    LbDraw_SetClipRect(&clip);
    if (entry != nullptr) {
        for (tagLBEntry* parent = entry; parent->mParent != nullptr; parent = parent->mParent) {
            std::strcat(indent, "    ");
        }
        for (int32_t row = 0; row < mF156 && entry != nullptr; ++row) {
            if (mF148 == row + mF150) {
                clip = {xPixel(static_cast<int16_t>(mListRect.mLeft + 2)), yPixel(static_cast<int16_t>(y)),
                        xPixel(static_cast<int16_t>(mListRect.mRight - 2)), yPixel(static_cast<int16_t>(y + mF154))};
                LbDraw_RectangleFilled(&clip, selected);
            }
            wchar_t wide[32];
            for (int i = 0; i < 32; ++i) wide[i] = entry->mText[i];
            char text[128];
            std::sprintf(text, "%s%ls", indent, wide);
            LbDraw_Text(xPixel(mListRect.mLeft + 2), yPixel(static_cast<int16_t>(y)), text, textColour);
            if (entry->mChild != nullptr) {
                entry = entry->mChild;
                std::strcat(indent, "    ");
            } else if (entry->mNext != nullptr) {
                entry = entry->mNext;
            } else {
                while (entry != nullptr && entry->mNext == nullptr) entry = entry->mParent;
                indent[0] = '\0';
                if (entry != nullptr) entry = entry->mNext;
            }
            y += mF154;
        }
    }
    LbDraw_SetClipRect(nullptr);
}
