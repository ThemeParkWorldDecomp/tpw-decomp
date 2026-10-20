// InterfaceWindow's window script (main app, UI toolkit 0x101dec14): the interpreter that builds a panel from a table of 16-bit words
// (Build and ProcessScript), the two draw object setters it calls, and three small members: GetVisibility, SetCallbackFunction and
// CaptureKb. Unit w31a.
//
// THE SCRIPT. A window script is an array of shorts (the Mac's, big-endian; here the host's), read through ONE position that all
// the nested calls share (gUIScriptPosition; Build sets it to 0). A record starts with a word that says what it is; a word that is
// not 0 to 0x12 (a negative one included) ends the level like record 5. A 32-bit value is two words, the LOW half first (read
// unsigned) and then the high half (signed). The records, with the words that follow the type:
//   0  a child window: object kind, attributes (2 words), id (2), left, top, right, bottom (CreateObject). The records after it,
//      up to the 5 that ends them, are about the NEW window (they run in a nested level on it).
//   1  the window's draw object: the hash of its name (2 words; DrawObjectLibrary::FindDrawObject) -> SetDrawObject
//   2  the client draw object (2 words) -> SetClientDrawObject
//   3  the client rectangle: left, top, right, bottom; made absolute when the window's attribute 0x400 is set
//   4  the window's region: a shape number and its numbers: 1 rectangle (4 words), 2 circle (x, y, radius), 3 ellipse (x, y, radii),
//      4 polygon (count, then x and y of each point); any other number changes nothing and takes no more words
//   5  end of this level
//   6, 7, 8  of a scroll bar (kind 3): its up button, its down button and its thumb (four words: the rectangle; a kind 2 child
//      with ids 1, 2 and 3; the thumb gets UI_DefaultThumbtrackCallback); the children's own records follow
//   9  a child of id 1 (kind 3, attributes 0x10: the scroll bar) for a list box (kind 4) or a list control (kind 7) that has no
//      child of id 1 yet (four words); on any other window it takes no words
//   10 the list rectangle of a list box (kind 4) or a list control (kind 7): four words; on any other window no words
//   11 a list control's columns: the count, then left and right of each (SetColumns, SetColumnPos)
//   12 a list control's column header: a column number (the id is 0x10 + it) and a rectangle (a kind 2 child)
//   13 a knob's angles: smallest and largest (SetMinMaxAngles)
//   14, 15  of a vary box (kind 0xc): its plus and minus buttons (kind 2 children, ids 1 and 2): four words, the rectangle
//   16 a light of a light panel (kind 0xd): the light number 0 to 31 (the id is 0x11100000 | it) and a rectangle (a kind 6 child: an image)
//   17 the window's help text number (2 words)
//   18 the word at +0x90 (2 words)
// The records that need a kind of window say so with an assertion (the Mac's strings, below) and go on when it fails.
#include "InterfaceWindow.h"

#include <new>

#include "InterfaceFont.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

int32_t gUIScriptPosition;   // 0x10395c5c

namespace {

// The kinds of window the records test for (the Mac source's UIOBJ_CREATE_ names, from its assertion strings) and the kinds of
// child they make.
constexpr int32_t kScrollBar = 3;
constexpr int32_t kListBox = 4;
constexpr int32_t kListControl = 7;
constexpr int32_t kKnob = 0xb;
constexpr int32_t kVaryBox = 0xc;
constexpr int32_t kLightPanel = 0xd;
constexpr int32_t kChildButton = 2;
constexpr int32_t kChildScrollBar = 3;
constexpr int32_t kChildImage = 6;
constexpr int32_t kLightBase = 0x11100000;      // UIID_LIGHTBASE
constexpr uint32_t kButtonlessScroll = 0x40;    // UIATTR_BUTTLESS_SCROLL, a scroll bar's attribute

const char* const kFile = "UI_Window.cpp";

// The words of the script, through the shared position.
int16_t NextWord(const int16_t* script) { return script[gUIScriptPosition++]; }

// A 32-bit value: the low half first (unsigned), then the high half (signed).
int32_t NextLong(const int16_t* script) {
    const uint32_t low = static_cast<uint16_t>(script[gUIScriptPosition++]);
    const int32_t high = script[gUIScriptPosition++];
    return static_cast<int32_t>(low | (static_cast<uint32_t>(high) << 16));
}

struct ScriptRect {
    int16_t left, top, right, bottom;
};
ScriptRect NextRect(const int16_t* script) {
    ScriptRect rect;
    rect.left = NextWord(script);
    rect.top = NextWord(script);
    rect.right = NextWord(script);
    rect.bottom = NextWord(script);
    return rect;
}

// Asks the window for a child (an assertion when it could not make one; the answer is NULL then).
InterfaceWindow* CreateChild(InterfaceWindow* window, int32_t kind, int32_t attributes, int32_t id, const ScriptRect& rect,
                             int32_t line) {
    InterfaceWindow* child = window->CreateObject(kind, attributes, id, rect.left, rect.top, rect.right, rect.bottom);
    if (child == nullptr) msl_assertion_failed("lpNew", kFile, line);
    return child;
}

// The first child with that id, or NULL.
InterfaceWindow* FindChild(InterfaceWindow* window, int32_t id) {
    InterfaceWindow* found = nullptr;
    for (InterfaceWindow* child = window->mChild; child != nullptr && found == nullptr; child = child->mNext)
        if (child->mId == id) found = child;
    return found;
}

// Takes the place of the window's region: the old one goes unless it is the window's own rectangle; the new one is the window's
// own rectangle when it could not be made.
void ReplaceRegion(InterfaceWindow* window, CoolRegion* region) {
    CoolRegion* old = window->mRegion;
    if (old != nullptr && old != &window->mRect) delete old;
    window->mRegion = region != nullptr ? region : &window->mRect;
}

// Record 4. A shape number that is not 1 to 4 changes nothing.
void ReadRegion(InterfaceWindow* window, const int16_t* script) {
    const int16_t shape = NextWord(script);
    if (shape == 1) {
        const ScriptRect rect = NextRect(script);
        ReplaceRegion(window, new (std::nothrow) CoolRect(rect.left, rect.top, rect.right, rect.bottom));
    } else if (shape == 2) {
        const int16_t x = NextWord(script);
        const int16_t y = NextWord(script);
        const int16_t radius = NextWord(script);
        ReplaceRegion(window, new (std::nothrow) CoolCircle(x, y, radius));
    } else if (shape == 3) {
        const int16_t x = NextWord(script);
        const int16_t y = NextWord(script);
        const int16_t radiusX = NextWord(script);
        const int16_t radiusY = NextWord(script);
        ReplaceRegion(window, new (std::nothrow) CoolEllipse(x, y, radiusX, radiusY));
    } else if (shape == 4) {
        // The points are read into a temporary array that the polygon copies and that goes when the region is in place. (A
        // negative count never ends the loop, as in the original.)
        const int16_t count = NextWord(script);
        int16_t* points = new int16_t[static_cast<uint32_t>(count) * 2];
        for (int32_t remaining = count; remaining != 0; --remaining) {
            const int32_t index = count - remaining;
            const int16_t x = NextWord(script);
            const int16_t y = NextWord(script);
            points[2 * index] = x;
            points[2 * index + 1] = y;
        }
        ReplaceRegion(window, new (std::nothrow) CoolPoly(count, points));
        delete[] points;
    }
}

// The interpreter: the records of one level, on `window` (NULL after a CreateObject that failed; the records that use it then
// crash, as the original's do). Answers the last window a record of this level made.
InterfaceWindow* ProcessLevel(InterfaceWindow* window, int16_t* script) {
    InterfaceWindow* last = nullptr;
    bool going = true;
    while (going) {
        const int16_t record = NextWord(script);
        if (static_cast<uint32_t>(static_cast<int32_t>(record)) > 0x12) {   // (unsigned: the negative ones end the level too)
            going = false;
            continue;
        }
        switch (record) {
        case 0: {
            const int32_t kind = NextWord(script);
            const int32_t attributes = NextLong(script);
            const int32_t id = NextLong(script);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kind, attributes, id, rect, 0xa3d);
            ProcessLevel(last, script);
            break;
        }
        case 1:
            window->SetDrawObject(DrawObjectLibrary::FindDrawObject(static_cast<uint32_t>(NextLong(script))));
            break;
        case 2:
            window->SetClientDrawObject(DrawObjectLibrary::FindDrawObject(static_cast<uint32_t>(NextLong(script))));
            break;
        case 3: {
            const ScriptRect rect = NextRect(script);
            window->mClientRect.Set(rect.left, rect.top, rect.right, rect.bottom);
            if ((window->mAttributes & 0x400) != 0) window->MakeRectAbsolute(&window->mClientRect);
            break;
        }
        case 4:
            ReadRegion(window, script);
            break;
        case 5:
            going = false;
            break;
        case 6:
        case 7: {
            // The up and the down button of a scroll bar that has neither it nor the attribute that says it has no buttons.
            const int32_t id = record == 6 ? 1 : 2;
            bool ok = false;
            bool available = false;
            if (window->WhatAmI() == kScrollBar && window->GetWindow(id) == nullptr) available = true;
            if (available && (window->mAttributes & kButtonlessScroll) == 0) ok = true;
            if (!ok)
                msl_assertion_failed(record == 6 ? "WhatAmI() == UIOBJ_CREATE_SCROLLBAR && !GetWindow(UIID_SCROLLUP) && "
                                                   "!(attributes & UIATTR_BUTTLESS_SCROLL)"
                                                 : "WhatAmI() == UIOBJ_CREATE_SCROLLBAR && !GetWindow(UIID_SCROLLDN) && "
                                                   "!(attributes & UIATTR_BUTTLESS_SCROLL)",
                                     kFile, record == 6 ? 0xa90 : 0xa9d);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 0x21, id, rect, record == 6 ? 0xa96 : 0xaa3);
            ProcessLevel(last, script);
            break;
        }
        case 8: {
            // The thumb of a scroll bar that has none: a button with the thumb track's handler.
            bool ok = false;
            if (window->WhatAmI() == kScrollBar && window->GetWindow(3) == nullptr) ok = true;
            if (!ok) msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_SCROLLBAR && !GetWindow(UIID_SCROLLTHUMB)", kFile, 0xaaa);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 1, 3, rect, 0xab0);
            last->SetCallbackFunction(UI_DefaultThumbtrackCallback);
            ProcessLevel(last, script);
            break;
        }
        case 9: {
            // The scroll bar of a list box, or else of a list control; each only when it has no child of id 1 (a list box that
            // has one is asked again what it is).
            bool made = false;
            if (window->WhatAmI() == kListBox && FindChild(window, 1) == nullptr) {
                const ScriptRect rect = NextRect(script);
                last = CreateChild(window, kChildScrollBar, 0x10, 1, rect, 0xaee);
                ProcessLevel(last, script);
                made = true;
            }
            if (!made && window->WhatAmI() == kListControl && FindChild(window, 1) == nullptr) {
                const ScriptRect rect = NextRect(script);
                last = CreateChild(window, kChildScrollBar, 0x10, 1, rect, 0xaf9);
                ProcessLevel(last, script);
            }
            break;
        }
        case 10:
            // (a window that is not a list box is asked again whether it is a list control)
            if (window->WhatAmI() == kListBox) {
                const ScriptRect rect = NextRect(script);
                static_cast<InterfaceListBox*>(window)->SetListRect(rect.left, rect.top, rect.right, rect.bottom);
            } else if (window->WhatAmI() == kListControl) {
                const ScriptRect rect = NextRect(script);
                static_cast<InterfaceListControl2*>(window)->SetListRect(rect.left, rect.top, rect.right, rect.bottom);
            }
            break;
        case 11: {
            if (window->WhatAmI() != kListControl)
                msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_LISTCONTROL", kFile, 0xb1a);
            auto* list = static_cast<InterfaceListControl2*>(window);
            const int16_t count = NextWord(script);
            list->SetColumns(count);
            for (int32_t column = 0; static_cast<int16_t>(column) < count; ++column) {
                const int16_t left = NextWord(script);
                const int16_t right = NextWord(script);
                list->SetColumnPos(static_cast<int16_t>(column), left, right);
            }
            break;
        }
        case 12: {
            if (window->WhatAmI() != kListControl)
                msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_LISTCONTROL", kFile, 0xb2b);
            const int32_t id = NextWord(script) + 0x10;
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 1, id, rect, 0xb34);
            ProcessLevel(last, script);
            break;
        }
        case 13: {
            if (window->WhatAmI() != kKnob) msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_KNOB", kFile, 0xb3c);
            int32_t minimum = NextWord(script);
            int32_t maximum = NextWord(script);
            static_cast<InterfaceKnob*>(window)->SetMinMaxAngles(&minimum, &maximum);
            break;
        }
        case 14:
        case 15: {
            // The plus and the minus button of a vary box that has neither.
            const int32_t id = record == 14 ? 1 : 2;
            bool ok = false;
            if (window->WhatAmI() == kVaryBox && window->GetWindow(id) == nullptr) ok = true;
            if (!ok)
                msl_assertion_failed(record == 14 ? "WhatAmI() == UIOBJ_CREATE_VARYBOX && !GetWindow(UIID_VARYBOXPLUSBUT)"
                                                  : "WhatAmI() == UIOBJ_CREATE_VARYBOX && !GetWindow(UIID_VARYBOXMINUSBUT)",
                                     kFile, record == 14 ? 0xab9 : 0xac6);
            const ScriptRect rect = NextRect(script);
            last = CreateChild(window, kChildButton, 0x21, id, rect, record == 14 ? 0xabf : 0xacc);
            ProcessLevel(last, script);
            break;
        }
        case 16: {
            // A light of a light panel: a number below 32 for which the panel has no light yet.
            if (window->WhatAmI() != kLightPanel)
                msl_assertion_failed("WhatAmI() == UIOBJ_CREATE_LIGHTPANEL", kFile, 0xad5);
            const int16_t light = NextWord(script);
            const ScriptRect rect = NextRect(script);
            bool inRange = false;
            bool ok = false;
            if (light >= 0 && light < 0x20) inRange = true;
            if (inRange && window->GetWindow(light | kLightBase) == nullptr) ok = true;
            if (!ok)
                msl_assertion_failed("(lightnum >= 0 && lightnum < 32) && !GetWindow(lightnum | UIID_LIGHTBASE)", kFile, 0xadd);
            last = CreateChild(window, kChildImage, 1, light | kLightBase, rect, 0xae1);
            ProcessLevel(last, script);
            break;
        }
        case 17:
            window->mHelpText = NextLong(script);
            break;
        case 18:
            window->mF90 = NextLong(script);
            break;
        }
    }
    return last;
}

// The text style a draw object is told with a font: the low byte of the font's id (the Mac's byte +8 of the style).
void SetFontStyle(const InterfaceFont* font, DOTextStyle* style) { style->mF06[2] = static_cast<uint8_t>(font->mID); }

}  // namespace

// ---- Build and ProcessScript

// Makes the windows of a script under this one (the callback goes to the first window the script made at its top level).
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    gUIScriptPosition = 0;
    InterfaceWindow* window = ProcessScript(description);
    if (window != nullptr && callback != nullptr) window->SetCallbackFunction(callback);
    return window;
}

InterfaceWindow* InterfaceWindow::ProcessScript(int16_t* script) { return ProcessLevel(this, script); }

// ---- the draw objects

// The two setters differ in the slot (+0x94 or +0x98), the depth (the window's z or z + 1) and the rectangle (the window's or the
// client's). SetClientDrawObject's release deletes mDrawObject, not the client object (see the header).
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) {
    InterfaceDrawObject* old = mDrawObject;
    if (object == old) return;
    if (old != nullptr) {
        old->mResizable = static_cast<int16_t>(old->mResizable - 1);
        old->Show(0);
        if (static_cast<int16_t>(old->mResizable - (1 - old->mInLibrary)) == -1 && mDrawObject != nullptr) delete mDrawObject;
    }
    mDrawObject = object;
    if (mDrawObject == nullptr) return;
    mDrawObject->mResizable = 1;
    DOChange change{};
    DOTextStyle style{};
    change.mFrame = 0;
    change.mDepth = mZ;
    change.mF08 = mZRot;
    change.mRect = &mRect;
    change.mF0C = mPivotX;
    change.mF0E = mPivotY;
    uint32_t flags = 0x57;
    if (mFont != nullptr) {
        SetFontStyle(mFont, &style);
        change.mText = &style;
        flags |= 0x10000;
    }
    mDrawObject->Change(flags, &change);
    if (GetVisibility() != 0) mDrawObject->Show(1);
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) {
    InterfaceDrawObject* old = mClientDrawObject;
    if (object == old) return;
    if (old != nullptr) {
        old->mResizable = static_cast<int16_t>(old->mResizable - 1);
        old->Show(0);
        if (static_cast<int16_t>(old->mResizable - (1 - old->mInLibrary)) == -1 && mDrawObject != nullptr) delete mDrawObject;
    }
    mClientDrawObject = object;
    if (mClientDrawObject == nullptr) return;
    mClientDrawObject->mResizable = 1;
    DOChange change{};
    DOTextStyle style{};
    change.mFrame = 0;
    change.mDepth = static_cast<int16_t>(mZ + 1);
    change.mF08 = mZRot;
    change.mRect = &mClientRect;
    change.mF0C = mPivotX;
    change.mF0E = mPivotY;
    uint32_t flags = 0x57;
    if (mFont != nullptr) {
        SetFontStyle(mFont, &style);
        change.mText = &style;
        flags |= 0x10000;
    }
    mClientDrawObject->Change(flags, &change);
    if (GetVisibility() != 0) mClientDrawObject->Show(1);
}

// ---- the small members

int16_t InterfaceWindow::GetVisibility() {
    int16_t visible = 1;
    for (InterfaceWindow* window = this; window != nullptr && visible != 0; window = window->mParent)
        if ((window->mAttributes & 1) == 0) visible = 0;
    return visible;
}

void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    if (callback != nullptr) mCallback = callback;
}

void InterfaceWindow::CaptureKb(int32_t on) {
    if (gUIKeyboardWindow == this) return;
    if (gUIKeyboardWindow != nullptr) UI_PostMessage(gUIKeyboardWindow, 0x20, on, 0);
    gUIKeyboardWindow = this;
    UI_PostMessage(gUIKeyboardWindow, 0x1f, on, 0);
}
