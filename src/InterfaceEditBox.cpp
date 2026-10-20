// Main UI edit box, 0x101cd468..0x101cf1ac; Mac layout in ui_controls.h (unit w37c).
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "InterfaceFont.h"
#include "platform/msl.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
// Only the text-style words named by the flags are passed to Change.
void ChangeText(InterfaceEditBox* box, uint32_t flags = 0x4100) {
    DOTextStyle style{};
    style.mText = box->mBuffer;
    std::memcpy(style.mF0A, &box->mF148, 2);
    DOChange change{};
    change.mText = &style;
    if (box->mClientDrawObject != nullptr) box->mClientDrawObject->Change(flags, &change);
}
void ChangeVertical(InterfaceEditBox* box) {
    DOTextStyle style{};
    std::memcpy(style.mF0A + 2, &box->mF14E, 2);
    DOChange change{};
    change.mText = &style;
    if (box->mClientDrawObject != nullptr) box->mClientDrawObject->Change(0x8000, &change);
}
bool Contains(const char16_t* text, char16_t character) {
    do {
        if (*text == character) return true;
    } while (*text++ != 0);
    return false;
}
CoolRect* Query(InterfaceDrawObject* object, uint32_t what, intptr_t value) {
    return reinterpret_cast<CoolRect*>(object->Query(what, value));
}
// The signed-short expression is evaluated after Show, which may change these words.
void ReleaseCursor(InterfaceEditBox* box) {
    InterfaceDrawObject* object = box->mCursorDrawObject;
    if (object == nullptr) return;
    object->mResizable = static_cast<int16_t>(object->mResizable - 1);
    object->Show(0);
    if (static_cast<int16_t>(object->mResizable - (1 - object->mInLibrary)) == -1 && box->mCursorDrawObject != nullptr)
        delete box->mCursorDrawObject;
}
}

void InterfaceEditBox::DrawFunction() {} // 0x1018a850
void InterfaceEditBox::ClientDraw() {} // 0x1018a88c
void InterfaceEditBox::CalcClientArea() {
    mClientRect.Set(mRect.mLeft, mRect.mTop, mRect.mRight, mRect.mBottom);
}
InterfaceEditBox::~InterfaceEditBox() {
    // The original uses scalar delete for a buffer made with new[].
    if (mBuffer != nullptr && (mAttributes & 0x40) != 0) ::operator delete(mBuffer);
    if (mTimer != nullptr) UI_RemoveTimer(this, 0x75e88);
    ReleaseCursor(this);
}
int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    if ((mAttributes & 0x40) != 0 || text == nullptr || capacity == 0) return 0;
    mBuffer = text;
    mMaxLength = capacity;
    mF160 = 1;
    return 1;
}
int32_t InterfaceEditBox::_SetText(char16_t* text, bool notify) {
    if (text == nullptr) return 0;
    bool changed = false;
    if ((mAttributes & 0x40) != 0) {
        // QUIRK: no copy at all unless the new length exceeds the current capacity.
        if (wcslen(text) > static_cast<uint32_t>(mMaxLength)) {
            if (mBuffer != nullptr) ::operator delete(mBuffer);
            mBuffer = new char16_t[wcslen(text) + 1];
            mMaxLength = static_cast<int32_t>(wcslen(text));
            if (mBuffer != nullptr) wcscpy(mBuffer, text);
            changed = true;
        }
    } else if (mMaxLength != 0 && mBuffer != nullptr &&
               (wcsncmp(mBuffer, text, UINT32_MAX) != 0 || mF160 != 0)) {
        wcsncpy(mBuffer, text, static_cast<uint32_t>(mMaxLength));
        mBuffer[mMaxLength] = 0;
        mF160 = 0;
        changed = true;
    } else if (mBuffer == text) {
        changed = true;
        mF160 = 0;
    }
    if (changed) {
        if (gUIAutoRedraw != 0) UI_PostMessage(this, 3, 0, 0);
        InterfaceWindow::SetText(mBuffer);
        MoveCursor(-5, 0, -1);
        if (notify) UI_PostMessage(mParent, 0x801, mId, 0);
    }
    return 0;
}
int32_t InterfaceEditBox::SetText(char16_t* text) { return _SetText(text, true); }
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) {
    if (object == mCursorDrawObject) return;
    ReleaseCursor(this);
    mCursorDrawObject = object;
    if (object == nullptr) return;
    object->mResizable = 1;
    DOChange change{};
    change.mFrame = 0;
    change.mDepth = static_cast<int16_t>(mZ + 3);
    change.mF08 = mZRot;
    CoolRect rect(static_cast<int16_t>(mClientRect.mLeft + mF150), static_cast<int16_t>(mClientRect.mTop + mF154),
                  static_cast<int16_t>(mClientRect.mLeft + mF150 + 8),
                  static_cast<int16_t>(mF156 + mClientRect.mTop + mF154));
    change.mRect = &rect;
    mCursorDrawObject->Change(0x17, &change);
    if (GetVisibility() != 0) mCursorDrawObject->Show(1);
}
int32_t NearestCharToPos(InterfaceDrawObject* object, int32_t x, int32_t y, int32_t length) {
    int32_t best = -1;
    uint32_t distance = UINT32_MAX;
    if (object != nullptr) {
        for (int32_t i = 0; i <= length; ++i) {
            CoolRect* rect = Query(object, 1, i);
            const uint32_t d = static_cast<uint32_t>(std::abs(x - rect->mLeft)) +
                               static_cast<uint32_t>(std::abs(y - rect->mTop)) * 0x4000u;
            if (d < distance) {
                distance = d;
                best = i;
            }
        }
    }
    return best;
}
void InterfaceEditBox::MoveCursor(int16_t position, int16_t show, int16_t selection) {
    if (mBuffer == nullptr || mFont == nullptr) return;
    const int16_t length = static_cast<int16_t>(wcslen(mBuffer));
    if (mF156 == 0) mF156 = static_cast<int16_t>(static_cast<int16_t>(mFont->GetHeight()) * 1536 / gUIRealScreenHeight);
    int16_t next = position;
    switch (position) {
    case -2: next = static_cast<int16_t>(mF14A - 1); if (next < 0) next = 0; break;
    case -3: next = static_cast<int16_t>(mF14A + 1); break;
    case -4: next = 0; break;
    case -5: next = static_cast<int16_t>(wcslen(mBuffer)); break;
    case -1: next = mF14A; break;
    case -6:
    case -7: {
        if ((mAttributes & 0x80) == 0) return;
        if (mClientDrawObject == nullptr) { next = mF14A; break; }
        if (Query(mClientDrawObject, 1, 0) == nullptr) msl_assertion_failed("r", "ui_editbox.cpp", 0x1c4);
        int32_t line = -9292843;
        int32_t distance = 0x4000;
        int32_t best = -1;
        const int32_t step = position == -6 ? -1 : 1;
        int32_t i = mF14A + step;
        bool done = false;
        while (static_cast<int16_t>(i) >= 0 && (step < 0 || static_cast<int16_t>(i) < length) && !done) {
            const int16_t index = static_cast<int16_t>(i);
            CoolRect* r = Query(mClientDrawObject, 1, index);
            if (line != -9292843) {
                if ((step < 0 && r->mTop < line) || (step > 0 && r->mTop > line)) done = true;
                else {
                    const int32_t d = std::abs(r->mLeft - mF150);
                    if (d < distance) {
                        distance = std::abs(r->mLeft - mF150);
                        best = index;
                        i += step;
                    } else done = true;
                }
            } else if ((step < 0 && r->mTop < mF154) || (step > 0 && r->mTop > mF154)) line = r->mTop;
            else i += step;
        }
        if (best <= -1) return;
        next = static_cast<int16_t>(best);
        break;
    }
    }
    if (selection == -2) {
        selection = static_cast<int16_t>((mF14C == -1 ? mF14A : mF14C) - 1);
        if (selection < 0) selection = 0;
    } else if (selection == -3) selection = static_cast<int16_t>((mF14C == -1 ? mF14A : mF14C) + 1);
    else if (selection == -4) selection = 0;
    else if (selection == -5) selection = static_cast<int16_t>(wcslen(mBuffer));
    if (selection > length) selection = length;
    if (selection == next) selection = -1;
    if (next > length) next = length;
    if (mRightToLeft != 0 && next != length) return;
    if (next == mF14A && selection == mF14C) return;
    if (Query(mClientDrawObject, 1, 0) == nullptr)
        msl_assertion_failed("lpClientDrawObject->Query(DOQ_TXT_CURPOS, 0)", "ui_editbox.cpp", 0x256);
    if (next < 0 || static_cast<uint32_t>(next) > wcslen(mBuffer)) return;
    mF14A = next;
    mF14C = selection;
    const int32_t widthWord = mClientRect.mRight - mClientRect.mLeft;
    const int16_t width = static_cast<int16_t>(widthWord);
    const int16_t height = static_cast<int16_t>(mClientRect.mBottom - mClientRect.mTop);
    CoolRect* r = nullptr;
    if ((mAttributes & 0x80) == 0) {
        r = Query(mClientDrawObject, 1, selection == -1 ? next : selection);
        if (r->mLeft > width) {
            mF148 = static_cast<int16_t>(mF148 + widthWord - (r->mLeft + 8));
            ChangeText(this);
        } else if (r->mLeft < 0) {
            mF148 = selection == 0 || next == 0 ? 0 : static_cast<int16_t>(mF148 - (r->mLeft + 8));
            ChangeText(this);
        }
    }
    if (width == 0) return;
    mFont->GetHeight();
    if (mClientDrawObject != nullptr) r = Query(mClientDrawObject, 1, next);
    // A null query here leaves the original's x register undefined in single-line mode.
    int16_t x = r != nullptr ? r->mLeft : 0;
    int16_t y = r != nullptr ? r->mTop : 0;
    int16_t endX = static_cast<int16_t>(x + 8);
    int16_t bottom = static_cast<int16_t>(y + mF156);
    if ((mAttributes & 0x80) != 0) {
        r = mClientDrawObject != nullptr ? Query(mClientDrawObject, 1, mF14A) : nullptr;
        x = r != nullptr ? r->mLeft : 0;
        y = r != nullptr ? r->mTop : 0;
        endX = static_cast<int16_t>(x + 8);
        r = mClientDrawObject != nullptr ? Query(mClientDrawObject, 2, reinterpret_cast<intptr_t>(mBuffer)) : r;
        if (r != nullptr) {
            r->mBottom = height;
            bool shifted = false;
            if (y + mF156 > r->mBottom) {
                mF14E = static_cast<int16_t>(mF14E - ((y + mF156 - r->mBottom) / mF156 + 1) * mF156);
                shifted = true;
            } else if (y < 0) {
                mF14E = static_cast<int16_t>(mF14E + (-y / mF156) * mF156);
                shifted = true;
            }
            if (shifted) {
                ChangeVertical(this);
                ChangeText(this);
                r = Query(mClientDrawObject, 1, mF14A);
                x = r != nullptr ? r->mLeft : 0;
                y = r != nullptr ? r->mTop : 0;
                endX = static_cast<int16_t>(x + 8);
            }
            bottom = static_cast<int16_t>(y + mF156);
            if (mF14C != -1) {
                const int16_t firstY = Query(mClientDrawObject, 1, 0)->mTop;
                r = Query(mClientDrawObject, 1, mF14C);
                endX = r->mTop > firstY ? static_cast<int16_t>(widthWord) : r->mLeft;
                bottom = static_cast<int16_t>(r->mTop + mF156);
            }
        }
    } else if (mF14C != -1) endX = Query(mClientDrawObject, 1, mF14C)->mLeft;
    if (endX > width) endX = width;
    else if (endX < 0) endX = 0;
    if (x > width) x = width;
    else if (x < 0) x = 0;
    if (bottom > height) bottom = height;
    if (x == mF150 && y == mF154 && endX == mF152) return;
    mF150 = x;
    mF152 = endX;
    mF154 = y;
    if (show != 0) mF158 = 1;
    if (gUIAutoRedraw != 0) UI_PostMessage(this, 3, 0, 0);
    if (mCursorDrawObject != nullptr) {
        CoolRect rect;
        rect.Set(static_cast<int16_t>(mClientRect.mLeft + std::min(mF150, mF152)),
                 static_cast<int16_t>(mClientRect.mTop + mF154),
                 static_cast<int16_t>(mClientRect.mLeft + std::max(mF150, mF152)),
                 static_cast<int16_t>(mClientRect.mTop + bottom));
        DOChange change{};
        change.mRect = &rect;
        if (mF158 != -1) {
            mCursorDrawObject->Show(1);
            mF158 = 1;
        }
        mCursorDrawObject->Change(0x10, &change);
    }
}
void InterfaceEditBox::SelectAll() {
    if (mRightToLeft != 0) MoveCursor(-5, 1, -1);
    else if (mBuffer != nullptr) MoveCursor(0, 1, static_cast<int16_t>(wcslen(mBuffer)));
}
void InterfaceEditBox::SetUnAcceptedChars(char16_t* text) { mUnAcceptedChars = text; }
void InterfaceEditBox::Edit(int16_t character) {
    if (mBuffer == nullptr) return;
    uint32_t length = wcslen(mBuffer);
    uint32_t at = UINT32_MAX;
    if (character == 13 || character == 9 || character == 8 || character == 27) return;
    if (mAcceptedChars != nullptr && !Contains(mAcceptedChars, static_cast<char16_t>(character))) return;
    if (mUnAcceptedChars != nullptr && Contains(mUnAcceptedChars, static_cast<char16_t>(character))) return;
    if (mF14C == -1) {
        if (length < static_cast<uint32_t>(mMaxLength)) {
            while ((at = static_cast<uint32_t>(mF14A)) < length) {
                mBuffer[length] = mBuffer[length - 1];
                --length;
            }
        }
    } else {
        at = static_cast<uint32_t>(std::min(mF14A, mF14C));
        const uint32_t last = static_cast<uint32_t>(std::max(mF14A, mF14C));
        const int32_t removed = static_cast<int32_t>(last - at - 1);
        uint32_t i = at;
        while (static_cast<int32_t>(++i) < static_cast<int32_t>(length - removed)) mBuffer[i] = mBuffer[i + removed];
        for (; i < static_cast<uint32_t>(mMaxLength); ++i) mBuffer[i] = 0;
    }
    if (static_cast<int32_t>(at) >= 0) {
        mBuffer[at] = static_cast<char16_t>(character);
        ChangeText(this);
        UI_PostMessage(mParent, 0x801, mId, 0);
        MoveCursor(static_cast<int16_t>(at + 1), 1, -1);
    }
}
void InterfaceEditBox::Keydown(int16_t key, int32_t modifiers) {
    if (mBuffer == nullptr) return;
    const bool shift = ((static_cast<uint32_t>(modifiers) >> 16) & 255) == 3;
    if (key == 0x2500 || key == 0x2700) {
        int16_t direction = key == 0x2500 ? -2 : -3;
        if (gUIHebrew != 0) direction = key == 0x2500 ? -3 : -2;
        MoveCursor(shift ? -1 : direction, 1, shift ? direction : -1);
    } else if (key == 0x2300 || key == 0x2400) {
        const int16_t direction = key == 0x2300 ? -5 : -4;
        MoveCursor(shift ? -1 : direction, 1, shift ? direction : -1);
    } else if (key == 13) {
        if ((mAttributes & 0x80) != 0) Edit(10);
    } else if (key == 0x2600 || key == 0x2800) {
        if ((mAttributes & 0x80) != 0) MoveCursor(key == 0x2600 ? -6 : -7, 1, -1);
        else UI_SendMessage(mParent, 0x805, mId, key);
    } else if (key == 8) {
        const int32_t length = static_cast<int32_t>(wcslen(mBuffer));
        if (mF14C != -1) {
            const int32_t first = std::min(mF14A, mF14C);
            const int32_t removed = std::max(mF14A, mF14C) - first;
            int32_t i = first;
            for (; i < length - removed; ++i) mBuffer[i] = mBuffer[i + removed];
            for (; i < mMaxLength; ++i) mBuffer[i] = 0;
            ChangeText(this);
            UI_PostMessage(mParent, 0x801, mId, 0);
            MoveCursor(static_cast<int16_t>(first), 1, -1);
        } else if (mF14A >= 1) {
            for (int32_t i = mF14A - 1; i < length; ++i) mBuffer[i] = mBuffer[i + 1];
            ChangeText(this);
            UI_PostMessage(mParent, 0x801, mId, 0);
            MoveCursor(-2, 1, -1);
        }
    } else if (key == 0x2e00) {
        const int32_t length = static_cast<int32_t>(wcslen(mBuffer));
        if (mF14C == -1) {
            for (int32_t i = mF14A; i < length; ++i) mBuffer[i] = mBuffer[i + 1];
        } else {
            const int32_t first = std::min(mF14A, mF14C);
            const int32_t removed = std::max(mF14A, mF14C) - first;
            int32_t i = first;
            for (; i < length - removed; ++i) mBuffer[i] = mBuffer[i + removed];
            for (; i < mMaxLength; ++i) mBuffer[i] = 0;
            // QUIRK: retains the old anchor, even for a backwards selection.
            MoveCursor(mF14A, 1, -1);
        }
        ChangeText(this);
        UI_PostMessage(mParent, 0x801, mId, 0);
    } else UI_SendMessage(mParent, 0x805, mId, key);
}
int32_t InterfaceEditBox::GetCurrentLen() { return mBuffer != nullptr ? static_cast<int16_t>(wcslen(mBuffer)) : 0; }
void InterfaceEditBox::LockOutMouse(bool lock) { mF161 = lock; }
