// Unit w37b. Fakes record every semantic argument; fixtures use native pointers.
#include <cstdint>
#include <cstring>
#include <new>
#include "calllog.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "InterfaceFont.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "TbColour.h"

using namespace calllog;

int32_t gUIAutoRedraw;
UIButtonState gUIButtons[3];
InterfaceWindow* gUIFocusWindow;
InterfaceWindow* gUICaptureDevice;
InterfaceHelpText* gUIHelpText;
struct TbPalette { uint8_t mOpaque[1]; };
TbPalette _lbGlobalPalette;
#include "LbDrawText.h"
const TbTextRender _lbSpriteRender = {}, _lbOneColourSpriteRender = {};

// Window fakes keep the object layout and vtable, and record their arguments.
InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t attr, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b) {
    mParent = parent;
    mAttributes = attr;
    mId = id;
    mRect.mLeft = l;
    mRect.mTop = t;
    mRect.mRight = r;
    mRect.mBottom = b;
}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Enable", {lab(this), on});
    return static_cast<int16_t>(ret("Enable"));
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return static_cast<int32_t>(ret("SetValue"));
}
int32_t InterfaceWindow::GetValue() {
    ev("GetValue", {lab(this)});
    return static_cast<int32_t>(ret("GetValue"));
}
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
InterfaceWindow* InterfaceWindow::GetParent() { return mParent; }
void InterfaceWindow::CheckFocus() {
    ev("CheckFocus", {lab(this)});
    gUIFocusWindow = this;
}
int32_t InterfaceWindow::CaptureDevice() {
    ev("CaptureDevice", {lab(this)});
    if (gUICaptureDevice != nullptr) return 0;
    gUICaptureDevice = this;
    return 1;
}
int32_t InterfaceWindow::UnCaptureDevice() {
    ev("UnCaptureDevice", {lab(this)});
    if (gUICaptureDevice != this) return 0;
    gUICaptureDevice = nullptr;
    return 1;
}
int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    ev("SetPosition", {lab(this), rect->mLeft, rect->mTop, rect->mRight, rect->mBottom});
    mRect.mLeft = rect->mLeft;
    mRect.mTop = rect->mTop;
    mRect.mRight = rect->mRight;
    mRect.mBottom = rect->mBottom;
    return 1;
}
int32_t InterfaceWindow::DrawMe() {
    ev("DrawMe", {lab(this)});
    return static_cast<int32_t>(ret("DrawMe"));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        if (child->mId == id) return child;
    }
    return nullptr;
}
int32_t UI_PostMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Post", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("Post"));
}
int32_t UI_SendMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Send", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("Send"));
}
int32_t UI_DefaultWindowCallback(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Default", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("Default"));
}

// These constructors only furnish vtables. Every tested field is filled by the fixture.
InterfaceListBox::InterfaceListBox(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
int32_t InterfaceListBox::WhatAmI() { return 4; }
InterfaceListControl2::InterfaceListControl2(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
int32_t InterfaceListControl2::WhatAmI() { return 7; }
void InterfaceListControl2::RemoveAllItems() { ev("Control.RemoveAll", {lab(this)}); }
InterfaceScrollBar::InterfaceScrollBar(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
InterfaceScrollBar::~InterfaceScrollBar() {}
void InterfaceScrollBar::DrawFunction() {}
void InterfaceScrollBar::CalcClientArea() {}
int32_t InterfaceScrollBar::WhatAmI() { return 3; }
void InterfaceScrollBar::ClientDraw() {}
int16_t InterfaceScrollBar::Enable(int16_t a) { return InterfaceWindow::Enable(a); }
int32_t InterfaceScrollBar::SetValue(int32_t a) { return InterfaceWindow::SetValue(a); }
int32_t InterfaceScrollBar::GetValue() { return InterfaceWindow::GetValue(); }
void InterfaceScrollBar::ReCalcValue() { ev("ReCalcValue", {lab(this)}); }
int16_t InterfaceScrollBar::ReCalcThumbPos(int32_t mode, int32_t value) {
    ev("ReCalcThumbPos", {lab(this), mode, value});
    return static_cast<int16_t>(ret("ReCalcThumbPos"));
}
void InterfaceScrollBar::SetMinMax(int32_t a, int32_t b) { ev("SetMinMax", {lab(this), a, b}); }
InterfaceButton::InterfaceButton(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
InterfaceButton::~InterfaceButton() {}
int32_t InterfaceButton::WhatAmI() { return 2; }
int16_t InterfaceButton::Enable(int16_t a) { return InterfaceWindow::Enable(a); }
int32_t InterfaceButton::SetText(char16_t*) { return 1; }
char16_t* InterfaceButton::GetText() { return nullptr; }
int32_t InterfaceButton::GetHelpText() { return mHelpText; }
int32_t InterfaceButton::SetState(int8_t a, int8_t b) {
    ev("SetState", {lab(this), a, b});
    return static_cast<int32_t>(ret("SetState"));
}
int32_t InterfaceButton::HiLight(int8_t a, int8_t b) {
    ev("HiLight", {lab(this), a, b});
    return static_cast<int32_t>(ret("HiLight"));
}

// Help callbacks record the parent and both signed short coordinates.
InterfaceHelpText::InterfaceHelpText() {}
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t x, int16_t y) { ev("Help.Move", {lab(this), x, y}); }
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow* w) { ev("Help.In", {lab(this), lab(w)}); }
void InterfaceHelpText::OnMouseMovedOut() { ev("Help.Out", {lab(this)}); }
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() {}
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow*) {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

// Shared draw object: Show can change its reference fields, as a virtual callee may.
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t a) {
    ev("Draw.Show", {lab(this), a, mResizable, mInLibrary});
    if (ret("ShowMutate") != 0) {
        mResizable = static_cast<int16_t>(ret("ShowRefs"));
        mInLibrary = static_cast<int16_t>(ret("ShowLibrary"));
    }
}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// Palette, render and draw imports are fakes; colours include all five bytes.
TbColour::TbColour() { mAlpha = mRed = mGreen = mBlue = mIndex = 0; }
uint8_t LbPalette_FindColour(const TbPalette* p, uint8_t r, uint8_t g, uint8_t b) {
    ev("Palette", {lab(p), r, g, b});
    return static_cast<uint8_t>(ret("Palette"));
}
TbResult LbDraw_SetTextFont(const TbTextRender* render, void* font) {
    ev("Font", {lab(render), lab(font)});
    return {static_cast<int32_t>(ret("Font"))};
}
void LbDraw_SetClipRect(const TbRect* rect) {
    if (rect == nullptr) ev("Clip", {0});
    else ev("Clip", {1, rect->left, rect->top, rect->right, rect->bottom});
}
void LbDraw_RectangleFilled(const TbRect* r, TbColour c) {
    ev("Fill", {r->left, r->top, r->right, r->bottom, c.mAlpha, c.mRed, c.mGreen, c.mBlue, c.mIndex});
}
void LbDraw_Text(int32_t x, int32_t y, const char* s, TbColour c) {
    ev("Text", {x, y, c.mAlpha, c.mRed, c.mGreen, c.mBlue, c.mIndex}, {s});
}

namespace {
InterfaceWindow* windows[8];
tagLBEntry* entries[24];
int entryCount;
InterfaceHelpText help;
CFont::FontEntry fonts[16];
uint8_t fontTokens[16];
InterfaceDrawObject* sharedDraw;
CoolRect output;

// Fixtures start at zero; C++ constructors provide each native vtable.
template<class T> T* make() {
    void* memory = ::operator new(sizeof(T));
    std::memset(memory, 0, sizeof(T));
    return new (memory) T(nullptr, 0, 0, 0, 0, 0, 0);
}
void rect(CoolRect& r, const int64_t* a) {
    r.mLeft = static_cast<int16_t>(a[0]);
    r.mTop = static_cast<int16_t>(a[1]);
    r.mRight = static_cast<int16_t>(a[2]);
    r.mBottom = static_cast<int16_t>(a[3]);
}
CoolRect& rectangle(InterfaceWindow* w, int which) {
    if (which == 0) return w->mRect;
    if (which == 1) return w->mPreviousRect;
    if (which == 2) return w->mClientRect;
    if (which == 3 && w == windows[1]) return static_cast<InterfaceListBox*>(w)->mListRect;
    if (which == 3 && w == windows[2]) return static_cast<InterfaceListControl2*>(w)->mListRect;
    return w->mOriginalRect;
}
}

extern "C" {
// Setup, outside the observed heap interval. Reset frees abandoned fixture blocks without destruction.
void tpw_lb_reset() {
    tpw_cl_on = 0;
    output.mLeft = output.mTop = output.mRight = output.mBottom = 0;
    for (auto*& w : windows) {
        if (w != nullptr) ::operator delete(w);
        w = nullptr;
    }
    for (auto*& e : entries) {
        if (e != nullptr) ::operator delete(e);
        e = nullptr;
    }
    windows[0] = make<InterfaceWindow>();
    windows[1] = make<InterfaceListBox>();
    windows[2] = make<InterfaceListControl2>();
    windows[3] = make<InterfaceScrollBar>();
    windows[4] = make<InterfaceButton>();
    for (int i = 5; i < 8; ++i) windows[i] = make<InterfaceWindow>();
    if (sharedDraw != nullptr) ::operator delete(sharedDraw);
    void* block = ::operator new(sizeof(InterfaceDrawObject));
    std::memset(block, 0, sizeof(InterfaceDrawObject));
    sharedDraw = new (block) InterfaceDrawObject(nullptr);
    gUIHelpText = &help;
    gUIFocusWindow = gUICaptureDevice = nullptr;
    std::memset(gUIButtons, 0, sizeof gUIButtons);
    gCFontTable = fonts;
    for (int i = 0; i < 16; ++i) fonts[i].mFont = &fontTokens[i];
    entryCount = 0;
}
// Fixed labels: windows 10..17, entries 100..123, palette 30, help 31, font render 32/33, fonts 40..55.
void tpw_lb_labels() {
    for (int i = 0; i < 8; ++i) tpw_cl_label(10 + i, windows[i]);
    for (int i = 0; i < entryCount; ++i) tpw_cl_label(100 + i, entries[i]);
    tpw_cl_label(30, &_lbGlobalPalette);
    tpw_cl_label(31, &help);
    tpw_cl_label(32, &_lbOneColourSpriteRender);
    tpw_cl_label(33, &_lbSpriteRender);
    for (int i = 0; i < 16; ++i) tpw_cl_label(40 + i, &fontTokens[i]);
    tpw_cl_label(60, sharedDraw);
    auto* control = static_cast<InterfaceListControl2*>(windows[2]);
    tpw_cl_label(61, control->mItemWindows);
    tpw_cl_label(62, control->mItemStorage);
    tpw_cl_label(63, control->mColumns);
}
void tpw_lb_setup(int32_t op, const int64_t* a) {
    auto* box = static_cast<InterfaceListBox*>(windows[1]);
    auto* control = static_cast<InterfaceListControl2*>(windows[2]);
    if (op == 0) {
        InterfaceWindow* w = windows[a[0]];
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mId = static_cast<int32_t>(a[2]);
        w->mParent = a[3] < 0 ? nullptr : windows[a[3]];
        w->mChild = a[4] < 0 ? nullptr : windows[a[4]];
        w->mNext = a[5] < 0 ? nullptr : windows[a[5]];
        w->mButtonsHeld = static_cast<uint32_t>(a[6]);
    } else if (op == 1) {
        rect(rectangle(windows[a[0]], static_cast<int>(a[1])), a + 2);
    } else if (op == 2) {
        box->mF148 = static_cast<int32_t>(a[0]);
        box->mF14C = static_cast<int32_t>(a[1]);
        box->mF150 = static_cast<int32_t>(a[2]);
        box->mF154 = static_cast<int16_t>(a[3]);
        box->mF156 = static_cast<int16_t>(a[4]);
    } else if (op == 3) {
        entryCount = static_cast<int>(a[0]);
        for (int i = 0; i < entryCount; ++i) {
            entries[i] = new tagLBEntry{};
            entries[i]->mText[0] = u'A' + i;
            entries[i]->mText[1] = u'0' + i % 10;
            entries[i]->mData = 1000 + i;
        }
        box->mEntries = entryCount == 0 ? nullptr : entries[0];
    } else if (op == 4) {
        tagLBEntry* e = entries[a[0]];
        e->mNext = a[1] < 0 ? nullptr : entries[a[1]];
        e->mChild = a[2] < 0 ? nullptr : entries[a[2]];
        e->mParent = a[3] < 0 ? nullptr : entries[a[3]];
    } else if (op == 5) {
        gUIAutoRedraw = static_cast<int32_t>(a[0]);
        gUIRealScreenWidth = static_cast<int16_t>(a[1]);
        gUIRealScreenHeight = static_cast<int16_t>(a[2]);
        gCFontTable = a[3] == 0 ? nullptr : fonts;
        for (int i = 0; i < 16; ++i) fonts[i].mFont = (a[4] & (1 << i)) == 0 ? nullptr : &fontTokens[i];
        gUIFocusWindow = a[5] < 0 ? nullptr : windows[a[5]];
        gUICaptureDevice = a[6] < 0 ? nullptr : windows[a[6]];
    } else if (op == 6) {
        UIButtonState& b = gUIButtons[a[0]];
        b.mState = static_cast<int16_t>(a[1]);
        b.mX = static_cast<int16_t>(a[2]);
        b.mY = static_cast<int16_t>(a[3]);
        b.mF06 = static_cast<int16_t>(a[4]);
        b.mTime = static_cast<int32_t>(a[5]);
    } else if (op == 7) {
        auto* bar = static_cast<InterfaceScrollBar*>(windows[3]);
        bar->mValue = static_cast<int32_t>(a[0]);
        bar->mStyle = static_cast<int16_t>(a[1]);
    } else if (op == 8) {
        control->mVisibleRows = static_cast<int16_t>(a[0]);
        if (a[1] != 0) {
            control->mItemWindows = new InterfaceWindow*[3];
            for (int i = 0; i < 3; ++i) control->mItemWindows[i] = a[2 + i] < 0 ? nullptr : windows[a[2 + i]];
            tpw_cl_label(61, control->mItemWindows);
        }
        if (a[5] != 0) {
            control->mItemStorage = new uint8_t[16];
            tpw_cl_label(62, control->mItemStorage);
        }
        if (a[6] >= 0) {
            control->mColumns = new CoolRect[a[6]];
            tpw_cl_label(63, control->mColumns);
        }
        control->mItemDrawObject = a[7] != 0 ? sharedDraw : nullptr;
        sharedDraw->mResizable = static_cast<int16_t>(a[8]);
        sharedDraw->mInLibrary = static_cast<int16_t>(a[9]);
    }
}
// Function dispatch. Out[0] is a result only when the original specifies one.
void tpw_lb_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = 0;
    auto* box = static_cast<InterfaceListBox*>(windows[1]);
    auto* control = static_cast<InterfaceListControl2*>(windows[2]);
    switch (op) {
    case 20: {
        CoolRect track;
        rect(output, a);
        rect(track, a + 4);
        CalcThumbtrack(&output, track, static_cast<int32_t>(a[8]), static_cast<int32_t>(a[9]), static_cast<int32_t>(a[10]));
        break;
    }
    case 21: out[0] = windows[a[0]]->UpdatePreviousPosition(); break;
    case 22: out[0] = windows[a[0]]->GetPreviousPosition(a[1] != 0 ? nullptr : &output); break;
    case 23: box->CalcClientArea(); break;
    case 24: control->CalcClientArea(); break;
    case 25: out[0] = lab(box->GetIndexedEntry(static_cast<int32_t>(a[0]))); break;
    case 26:
        out[0] = recurseDelete(box->mEntries);
        for (auto*& e : entries) e = nullptr;
        box->mEntries = nullptr;
        break;
    case 27:
        box->RemoveAllItems();
        for (auto*& e : entries) e = nullptr;
        break;
    case 28:
        if (static_cast<int16_t>(a[0]) > 0) delete box;
        else box->~InterfaceListBox();
        windows[1] = nullptr;
        for (auto*& e : entries) e = nullptr;
        break;
    case 29: out[0] = CFont::Set(static_cast<CFont::FontId>(a[0]), static_cast<CFont::RenderType>(a[1])); break;
    case 30: box->ClientDraw(); break;
    case 31: box->DrawFunction(); break;
    case 32: control->ClientDraw(); break;
    case 33:
        if (static_cast<int16_t>(a[0]) > 0) delete control;
        else control->~InterfaceListControl2();
        windows[2] = nullptr;
        for (int i = 5; i < 8; ++i) {
            if (lab(windows[i]) == -1) windows[i] = nullptr;
        }
        if (lab(sharedDraw) == -1) sharedDraw = nullptr;
        break;
    case 34: out[0] = UI_DefaultListBoxCallback(box, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 35: out[0] = UI_DefaultThumbtrackCallback(windows[4], static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    }
}
// Complete window state, normalizing each native pointer to its fixed label.
int32_t tpw_lb_dump(int64_t* out) {
    int64_t* start = out;
    auto put = [&](int64_t v) { *out++ = v; };
    auto r = [&](const CoolRect& c) {
        put(c.mLeft);
        put(c.mTop);
        put(c.mRight);
        put(c.mBottom);
    };
    for (int i = 0; i < 8; ++i) {
        InterfaceWindow* w = windows[i];
        put(w != nullptr);
        if (w == nullptr) continue;
        put(w->mDestroying);
        r(w->mRect);
        r(w->mOriginalRect);
        r(w->mPreviousRect);
        r(w->mClientRect);
        r(w->mRect34);
        put(lab(w->mRegion));
        put(static_cast<int32_t>(w->mAttributes));
        put(lab(w->mFont));
        put(w->mCursor);
        put(lab(w->mNext));
        put(lab(w->mPrevious));
        put(lab(w->mChild));
        put(lab(w->mParent));
        put(w->mF60);
        put(w->mF64);
        for (uint8_t b : w->mF68) put(b);
        put(w->mId);
        put(w->mHelpText);
        put(w->mF90);
        put(lab(w->mDrawObject));
        put(lab(w->mClientDrawObject));
        for (void* p : w->mExtraParams) put(lab(p));
        put(w->mZ);
        put(w->mZRot);
        put(w->mPivotX);
        put(w->mPivotY);
        put(lab(w->mAnim));
        put(w->mAnimFrame);
        put(w->mAnimTicks);
        put(w->mAnimStart);
        for (int16_t v : w->mAnimOffset) put(v);
        put(lab(w->mSlideTimer));
        for (int16_t v : w->mSlideStep) put(v);
        put(w->mSlideSteps);
        for (uint8_t b : w->mF10A) put(b);
        put(w->mCallback != nullptr);
        put(w->mDefaultCallback != nullptr);
        put(w->mButtonsHeld);
        for (void* p : w->mAnimTimers) put(lab(p));
        put(w->mF130);
        put(w->mF134);
        put(w->mAnimating);
        put(w->mSliding);
        if (i == 1) {
            auto* b = static_cast<InterfaceListBox*>(w);
            r(b->mListRect);
            put(lab(b->mEntries));
            put(b->mF148);
            put(b->mF14C);
            put(b->mF150);
            put(b->mF154);
            put(b->mF156);
        } else if (i == 2) {
            auto* c = static_cast<InterfaceListControl2*>(w);
            r(c->mListRect);
            put(lab(c->mItemStorage));
            put(lab(c->mItemWindows));
            put(c->mCurrentEntry);
            put(c->mItemCount);
            put(c->mTopEntry);
            put(c->mBlockWords);
            put(c->mF15C);
            put(c->mFirstEntry);
            put(c->mColumnCount);
            put(c->mF166);
            put(c->mSortColumn);
            put(c->mSortDescending);
            put(lab(c->mColumns));
            put(c->mRowHeight);
            put(c->mVisibleRows);
            put(lab(c->mItemDrawObject));
        } else if (i == 3) {
            auto* b = static_cast<InterfaceScrollBar*>(w);
            put(b->mMax);
            put(b->mMin);
            put(b->mValue);
            put(b->mStep);
            put(b->mPage);
            put(b->mStyle);
        }
    }
    r(output);
    put(gUIAutoRedraw);
    put(gUIRealScreenWidth);
    put(gUIRealScreenHeight);
    put(lab(gUIFocusWindow));
    put(lab(gUICaptureDevice));
    for (const auto& b : gUIButtons) {
        put(b.mState);
        put(b.mX);
        put(b.mY);
        put(b.mF06);
        put(b.mTime);
    }
    for (int i = 0; i < entryCount; ++i) {
        put(entries[i] != nullptr);
        if (entries[i] == nullptr) continue;
        for (char16_t c : entries[i]->mText) put(c);
        put(entries[i]->mData);
        put(lab(entries[i]->mNext));
        put(lab(entries[i]->mChild));
        put(lab(entries[i]->mParent));
    }
    return static_cast<int32_t>(out - start);
}
}
