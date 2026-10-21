// InterfaceListBox.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceListBox code, named after it.

#include "InterfaceFont.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "TbColour.h"
#include "ui_controls.h"
#include <cstdio>
#include <cstring>
#include <cwchar>

// List box drawing, selection and entry-tree teardown (main app, unit w37b).

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

// The control factory and the controls' constructors (main app, UI toolkit): InterfaceWindow::CreateObject (0x101dc750) and
// the constructors of the bar, check buttons, edit box, image, knob, light panel, list box, list control, text box and
// vary box. The button's constructor and members are in src/InterfaceButton.cpp; the scroll bar's belongs to another unit.
//
// Every constructor runs the window's constructor first, stores its own vtable (the compiler's here) and its members, and
// for most of them a window handler. The destructors, the other virtual members and the handlers are untranslated.

InterfaceListBox::InterfaceListBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                   int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {
    mF148 = -1;
    int32_t barAttributes = 0x10;
    mF144 = 0;
    mF154 = 0;
    mF156 = 0;
    mF150 = 0;
    mF14C = 0;
    CalcClientArea();
    if ((attributes & 0x1000) != 0) {
        if ((attributes & 0x40) != 0) barAttributes |= 0x40;
        InterfaceWindow* bar = CreateObject(3, barAttributes | 0x80, 1, static_cast<int16_t>(mClientRect.mRight + 4),
                                            mRect.mTop, mRect.mRight, mRect.mBottom);
        if (bar != nullptr) {
            bar->Enable(0);
            bar->Show(1);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0);
        }
    }
    SetCallbackFunction(UI_DefaultListBoxCallback);
}

// 0x101d0dc4 (132 bytes). Member and base destruction follow the C++ body.
InterfaceListBox::~InterfaceListBox() { RemoveAllItems(); }

// The window classes' WhatAmI members, the base window's CalcClientArea and CallDefaultWindowHandler (main app, UI toolkit; unit w36).
// WhatAmI is the interface's run time type: each class answers its own kind number, the one InterfaceWindow::CreateObject
// (src/ui_controls_create.cpp) takes to make it and the window scripts' records test with (src/InterfaceWindow_script.cpp, whose
// records 6 to 16 assert on the answer). The button's and the scroll bar's are in their own files (InterfaceButton.cpp,
// InterfaceScrollBar.cpp); the kinds are the enum in include/ui_controls.h.

int32_t InterfaceListBox::WhatAmI() { return kUIKindListBox; }            // 0x101d0e78

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

// The interface's control classes: the setters the window script calls (main app, unit w31a). InterfaceKnob::SetMinMaxAngles,
// InterfaceListBox::SetListRect, InterfaceListControl2's SetListRect, SetColumns and SetColumnPos, and the constructors of the
// three region shapes a window can be given (CoolCircle, CoolEllipse, CoolPoly; src/ui_cool_shapes.cpp has the rest of them).

// ---- the lists

void InterfaceListBox::SetListRect(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    mListRect.Set(left, top, right, bottom);
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
