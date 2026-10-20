// Unit w37c. Recording boundaries; real rectangles, default handler and window SetText.
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <new>
#include "calllog.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "InterfaceFont.h"
#include "platform/msl.h"
using namespace calllog;

// Label text allocations so scalar deletion of an array stays visible.
static void allocated(std::size_t bytes, bool array, void* p) {
    if (array && on()) ev("new[]", {newlab(p), static_cast<int64_t>(bytes)});
}
#define TPW_ALLOCATION_OBSERVER allocated
#include "calllog.cpp"

InterfaceWindow* gUIFocusWindow;
InterfaceWindow* gUICaptureDevice;
InterfaceWindow* gUIKeyboardWindow;
InterfaceWindow* gUIPointerWindow;
bool gUIHebrew;
InterfaceHelpText* gUIHelpText;
static uint32_t clockTick, clockStep;

// Window virtuals outside this unit furnish the base vtable.
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
int16_t InterfaceWindow::Enable(int16_t a) {
    ev("Enable", {lab(this), a});
    return 1;
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
int32_t InterfaceWindow::Show(int16_t a) {
    ev("Show", {lab(this), a});
    return 1;
}
int32_t UI_UpdatePointerIn() {
    ev("Pointer");
    return 1;
}
void UI_SetCursor(int16_t a) { ev("Cursor", {a}); }
uint32_t IveGotTicks() {
    ev("Tick", {clockTick});
    const uint32_t answer = clockTick;
    clockTick += clockStep;
    return answer;
}
int32_t UI_PostMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Post", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("Post"));
}
int32_t UI_SendMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Send", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("Send"));
}
void InterfaceWindow::CaptureKb(int32_t on) {
    ev("CaptureKb", {lab(this), on});
    if (gUIKeyboardWindow != this) {
        if (gUIKeyboardWindow != nullptr) UI_PostMessage(gUIKeyboardWindow, 0x20, on, 0);
        gUIKeyboardWindow = this;
        UI_PostMessage(this, 0x1f, on, 0);
    }
}
int16_t InterfaceWindow::GetVisibility() {
    ev("Visibility", {lab(this)});
    for (auto* w = this; w != nullptr; w = w->mParent) if ((w->mAttributes & 1) == 0) return 0;
    return 1;
}
void UI_RemoveTimer(InterfaceWindow* w, int32_t id) { ev("RemoveTimer", {lab(w), id}); }
InterfaceTimer::InterfaceTimer() {}
InterfaceTimer::~InterfaceTimer() {}
InterfaceEditBox::InterfaceEditBox(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
int32_t InterfaceEditBox::WhatAmI() { return 5; }
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
int32_t InterfaceFont::GetHeight() {
    ev("Height", {lab(this)});
    return mHeight;
}

// Help callbacks reached by the real default handler record all arguments.
InterfaceHelpText::InterfaceHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t x, int16_t y) { ev("Help.Move", {lab(this), x, y}); }
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow* w) { ev("Help.In", {lab(this), lab(w)}); }
void InterfaceHelpText::OnMouseMovedOut() { ev("Help.Out", {lab(this)}); }
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() { ev("Help.Frame", {lab(this)}); }
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow*) {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

namespace {
InterfaceEditBox* box;
InterfaceWindow* parent;
InterfaceDrawObject* draws[3];
InterfaceFont font;
InterfaceTimer timer;
InterfaceHelpText help;
CoolRect queryRect;
char16_t texts[5][1024];
char16_t* fixtureBuffer;
int32_t columns, glyphWidth, rowHeight, originX, originY;
int16_t offsetX, offsetY;
int32_t queryNull;
bool alive;
// ASCII text for calllog without allocating temporary std::strings.
const char* shown(const char16_t* text) {
    static char out[160];
    if (text == nullptr) return "(null)";
    int n = 0;
    for (; n < 159 && text[n] != 0; ++n) out[n] = text[n] < 128 ? static_cast<char>(text[n]) : '?';
    out[n] = 0;
    return out;
}
void setRect(CoolRect& r, const int64_t* a) {
    r.Set(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]));
}
}

// Draw Query uses a controllable character grid, including negative and wrapped coordinates.
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t a, CoolRect* r, int16_t b) {
    ev("Draw", {lab(this), a, r->mLeft, r->mTop, r->mRight, r->mBottom, b});
}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* c) {
    const char16_t* text = (flags & 0x100) != 0 ? c->mText->mText : nullptr;
    int16_t x = 0, y = 0;
    if ((flags & 0x4000) != 0) std::memcpy(&x, c->mText->mF0A, 2);
    if ((flags & 0x8000) != 0) std::memcpy(&y, c->mText->mF0A + 2, 2);
    const CoolRect* r = (flags & 0x10) != 0 ? c->mRect : nullptr;
    ev("Change", {lab(this), flags, (flags & 1) != 0 ? c->mFrame : 0, (flags & 2) != 0 ? c->mDepth : 0,
                   (flags & 4) != 0 ? c->mF08 : 0, r != nullptr ? r->mLeft : 0, r != nullptr ? r->mTop : 0,
                   r != nullptr ? r->mRight : 0, r != nullptr ? r->mBottom : 0, x, y},
                  {(flags & 0x100) != 0 ? shown(text) : ""});
    if (this == draws[0]) {
        if ((flags & 0x4000) != 0) offsetX = x;
        if ((flags & 0x8000) != 0) offsetY = y;
    }
}
intptr_t InterfaceDrawObject::Query(uint32_t what, intptr_t value) {
    ev("Query", {lab(this), what, what == 2 ? lab(reinterpret_cast<void*>(value)) : value},
                {what == 2 ? shown(reinterpret_cast<char16_t*>(value)) : ""});
    if (queryNull == static_cast<int32_t>(what)) return 0;
    const int32_t index = static_cast<int32_t>(value);
    queryRect.Set(static_cast<int16_t>(originX + (what == 2 ? 0 : index % columns * glyphWidth) + offsetX),
                  static_cast<int16_t>(originY + (what == 2 ? 0 : index / columns * rowHeight) + offsetY),
                  123, 456);
    return reinterpret_cast<intptr_t>(&queryRect);
}
void InterfaceDrawObject::Show(int16_t a) {
    ev("Draw.Show", {lab(this), a, mResizable, mInLibrary});
    mShown = a;
    if (ret("ShowMutate") != 0) {
        mResizable = static_cast<int16_t>(ret("ShowRefs"));
        mInLibrary = static_cast<int16_t>(ret("ShowLibrary"));
    }
}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// The UTF-16 runtime is the real platform/msl.cpp, with functional PPC import counterparts.

extern "C" {
// Reset raw fixture storage without invoking game destruction outside an operation.
void tpw_eb_reset() {
    tpw_cl_on = 0;
    if (alive && box != nullptr && box->mBuffer != nullptr && lab(box->mBuffer) >= 100) ::operator delete(box->mBuffer);
    if (fixtureBuffer != nullptr && lab(fixtureBuffer) == 50) ::operator delete(fixtureBuffer);
    if (box != nullptr && lab(box) > 0) ::operator delete(box);
    if (parent != nullptr) ::operator delete(parent);
    for (auto* d : draws) if (d != nullptr && lab(d) > 0) ::operator delete(d);
    fixtureBuffer = new char16_t[1024]();
    box = static_cast<InterfaceEditBox*>(::operator new(sizeof(InterfaceEditBox)));
    std::memset(static_cast<void*>(box), 0, sizeof(*box));
    new (box) InterfaceEditBox(nullptr, 0, 0, 0, 0, 0, 0);
    parent = static_cast<InterfaceWindow*>(::operator new(sizeof(InterfaceWindow)));
    std::memset(static_cast<void*>(parent), 0, sizeof(*parent));
    new (parent) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
    for (auto*& d : draws) {
        d = static_cast<InterfaceDrawObject*>(::operator new(sizeof(InterfaceDrawObject)));
        std::memset(static_cast<void*>(d), 0, sizeof(*d));
        new (d) InterfaceDrawObject(nullptr);
    }
    std::memset(texts, 0, sizeof texts);
    std::memset(static_cast<void*>(&timer), 0, sizeof timer);
    gUIFocusWindow = gUICaptureDevice = gUIKeyboardWindow = gUIPointerWindow = nullptr;
    gUIHelpText = &help;
    help.mWindow = nullptr;
    std::memset(box->mF68, 0xa5, sizeof box->mF68);
    std::memset(box->mF10A, 0xa5, sizeof box->mF10A);
    std::memset(box->mF15A, 0xa5, sizeof box->mF15A);
    box->mF163 = 0xa5;
    gUIHelpTextResult = 0;
    gUIDoubleClickTime = 500;
    std::memset(gUIButtons, 0, sizeof gUIButtons);
    queryRect.Set(0, 0, 0, 0);
    offsetX = offsetY = 0;
    alive = true;
}
void tpw_eb_labels() {
    tpw_cl_label(10, box);
    tpw_cl_label(11, parent);
    for (int i = 0; i < 3; ++i) tpw_cl_label(20 + i, draws[i]);
    for (int i = 0; i < 5; ++i) tpw_cl_label(30 + i, texts[i]);
    tpw_cl_label(40, &font);
    tpw_cl_label(41, &timer);
    tpw_cl_label(42, &help);
    tpw_cl_label(50, fixtureBuffer);
}
// Setup records select text/field/rectangle/global grids independently of operations.
void tpw_eb_setup(int32_t op, const int64_t* a) {
    if (op == 0) {
        const int slot = static_cast<int>(a[0]);
        for (int i = 0; i < a[1]; ++i) texts[slot][i] = static_cast<char16_t>(a[2 + i]);
        texts[slot][a[1]] = 0;
    } else if (op == 1) {
        box->mAttributes = static_cast<uint32_t>(a[0]);
        parent->mAttributes = static_cast<uint32_t>(a[1]);
        box->mParent = parent;
        std::memcpy(fixtureBuffer, texts[0], sizeof texts[0]);
        box->mBuffer = a[2] < 0 ? nullptr : a[2] == 0 ? fixtureBuffer : texts[a[2]];
        box->mMaxLength = static_cast<int32_t>(a[3]);
        box->mFont = a[4] ? &font : nullptr;
        box->mClientDrawObject = a[5] ? draws[0] : nullptr;
        box->mDrawObject = a[6] ? draws[1] : nullptr;
        box->mCursorDrawObject = a[7] ? draws[2] : nullptr;
        box->mTimer = a[8] ? &timer : nullptr;
        box->mId = static_cast<int32_t>(a[9]);
        box->mZ = static_cast<int16_t>(a[10]);
        box->mZRot = static_cast<int16_t>(a[11]);
    } else if (op == 2) {
        box->mF148 = static_cast<int16_t>(a[0]);
        box->mF14A = static_cast<int16_t>(a[1]);
        box->mF14C = static_cast<int16_t>(a[2]);
        box->mF14E = static_cast<int16_t>(a[3]);
        box->mF150 = static_cast<int16_t>(a[4]);
        box->mF152 = static_cast<int16_t>(a[5]);
        box->mF154 = static_cast<int16_t>(a[6]);
        box->mF156 = static_cast<int16_t>(a[7]);
        box->mF158 = static_cast<int16_t>(a[8]);
        box->mF160 = static_cast<uint8_t>(a[9]);
        box->mF161 = static_cast<uint8_t>(a[10]);
        box->mRightToLeft = static_cast<uint8_t>(a[11]);
        box->mAcceptedChars = a[12] < 0 ? nullptr : texts[a[12]];
        box->mUnAcceptedChars = a[13] < 0 ? nullptr : texts[a[13]];
    } else if (op == 3) setRect(a[0] == 0 ? box->mRect : box->mClientRect, a + 1);
    else if (op == 4) {
        gUIAutoRedraw = static_cast<int32_t>(a[0]);
        gUIRealScreenHeight = static_cast<int16_t>(a[1]);
        gUIHebrew = a[2] != 0;
        clockTick = static_cast<uint32_t>(a[3]);
        clockStep = static_cast<uint32_t>(a[4]);
        font.mHeight = static_cast<int16_t>(a[5]);
        box->mButtonsHeld = static_cast<uint32_t>(a[6]);
        columns = static_cast<int32_t>(a[7]);
        glyphWidth = static_cast<int32_t>(a[8]);
        rowHeight = static_cast<int32_t>(a[9]);
        originX = static_cast<int32_t>(a[10]);
        originY = static_cast<int32_t>(a[11]);
        queryNull = static_cast<int32_t>(a[12]);
    } else if (op == 5) {
        auto& s = gUIButtons[a[0]];
        s.mState = static_cast<int16_t>(a[1]);
        s.mX = static_cast<int16_t>(a[2]);
        s.mY = static_cast<int16_t>(a[3]);
        s.mTime = static_cast<int32_t>(a[4]);
    } else if (op == 6) {
        draws[a[0]]->mResizable = static_cast<int16_t>(a[1]);
        draws[a[0]]->mInLibrary = static_cast<int16_t>(a[2]);
    } else if (op == 7) {
        timer.mStart = static_cast<uint32_t>(a[0]);
        timer.mRunning = static_cast<int32_t>(a[1]);
        box->mAnimating = static_cast<uint8_t>(a[2]);
        box->mAnim = nullptr;
        box->mAnimTicks = 0;
        box->mAnimStart = 0;
    } else if (op == 8) {
        for (int64_t i = a[0]; i < a[1]; ++i) box->mBuffer[i] = static_cast<char16_t>(a[2]);
        box->mBuffer[a[1]] = 0;
    }
}
void tpw_eb_op(int32_t op, const int64_t* a, int64_t* out) {
    *out = 0;
    switch (op) {
    case 0:
        *out = UI_DefaultEditBoxCallback(box, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
        break;
    case 1:
        box->Edit(static_cast<int16_t>(a[0]));
        break;
    case 2:
        box->Keydown(static_cast<int16_t>(a[0]), static_cast<int32_t>(a[1]));
        break;
    case 3:
        box->MoveCursor(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]));
        break;
    case 4:
        *out = NearestCharToPos(a[0] ? draws[0] : nullptr, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]));
        break;
    case 5:
        *out = box->_SetText(a[0] < 0 ? nullptr : a[0] == 5 ? box->mBuffer : texts[a[0]], a[1] != 0);
        break;
    case 6:
        *out = box->SetText(a[0] < 0 ? nullptr : a[0] == 5 ? box->mBuffer : texts[a[0]]);
        break;
    case 7:
        box->SetCursorDrawObject(a[0] < 0 ? nullptr : draws[a[0]]);
        break;
    case 8:
        box->SelectAll();
        break;
    case 9:
        *out = box->SetStaticTextArea(a[0] < 0 ? nullptr : texts[a[0]], static_cast<int32_t>(a[1]));
        break;
    case 10:
        *out = box->GetCurrentLen();
        break;
    case 11:
        box->LockOutMouse(a[0] != 0);
        break;
    case 12:
        box->SetUnAcceptedChars(a[0] < 0 ? nullptr : texts[a[0]]);
        break;
    case 13:
        box->CalcClientArea();
        break;
    case 14:
        if (static_cast<int16_t>(a[0]) > 0) delete box;
        else box->~InterfaceEditBox();
        alive = false;
        break;
    case 15:
        box->DrawFunction();
        break;
    case 16:
        box->ClientDraw();
        break;
    case 17:
        timer.Start();
        break;
    case 18:
        box->CheckClientMouse(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
        break;
    }
}
// Dump every field read or written, all text capacity bytes, timer and global state.
int32_t tpw_eb_dump(int64_t* out) {
    int n = 0;
    auto put = [&](int64_t v) { out[n++] = v; };
    auto rect = [&](const CoolRect& r) {
        put(r.mLeft);
        put(r.mTop);
        put(r.mRight);
        put(r.mBottom);
    };
    put(alive);
    if (alive) {
        rect(box->mRect);
        rect(box->mOriginalRect);
        rect(box->mPreviousRect);
        rect(box->mClientRect);
        rect(box->mRect34);
        put(box->mAttributes);
        put(lab(box->mFont));
        put(box->mCursor);
        put(lab(box->mNext));
        put(lab(box->mPrevious));
        put(lab(box->mChild));
        put(lab(box->mParent));
        put(box->mF60);
        put(box->mF64);
        put(box->mId);
        put(box->mHelpText);
        put(box->mF90);
        put(lab(box->mDrawObject));
        put(lab(box->mClientDrawObject));
        for (auto p : box->mExtraParams) put(lab(p));
        put(box->mZ);
        put(box->mZRot);
        put(box->mPivotX);
        put(box->mPivotY);
        put(lab(box->mAnim));
        put(box->mAnimFrame);
        put(box->mAnimTicks);
        put(box->mAnimStart);
        for (auto v : box->mAnimOffset) put(v);
        put(lab(box->mSlideTimer));
        for (auto v : box->mSlideStep) put(v);
        put(box->mSlideSteps);
        put(box->mButtonsHeld);
        for (auto p : box->mAnimTimers) put(lab(p));
        put(box->mF130);
        put(box->mF134);
        put(box->mAnimating);
        put(box->mSliding);
        put(lab(box->mBuffer));
        put(box->mMaxLength);
        put(lab(box->mAcceptedChars));
        put(lab(box->mUnAcceptedChars));
        put(box->mF148);
        put(box->mF14A);
        put(box->mF14C);
        put(box->mF14E);
        put(box->mF150);
        put(box->mF152);
        put(box->mF154);
        put(box->mF156);
        put(box->mF158);
        put(lab(box->mCursorDrawObject));
        put(box->mF160);
        put(box->mF161);
        put(box->mRightToLeft);
        put(box->mF163);
        put(lab(box->mTimer));
        if (box->mBuffer != nullptr) {
            for (int i = 0; i <= box->mMaxLength && i < 1024; ++i) put(box->mBuffer[i]);
        }
    }
    if (alive) {
        put(box->mDestroying);
        put(lab(box->mRegion));
        for (auto v : box->mF68) put(v);
        for (auto v : box->mF10A) put(v);
        for (auto v : box->mF15A) put(v);
    }
    for (const auto& text : texts) for (auto v : text) put(v);
    put(lab(timer.mWindow));
    put(timer.mStart);
    put(timer.mSlice);
    put(timer.mId);
    put(timer.mRunning);
    put(timer.mFree);
    put(lab(timer.mNext));
    put(lab(timer.mPrevious));
    for (auto* d : draws) {
        // A deleted draw object's label is forgotten; never dereference it again.
        put(lab(d));
        if (lab(d) > 0) {
            put(d->mShown);
            put(d->mResizable);
            put(d->mInLibrary);
        }
    }
    rect(queryRect);
    put(offsetX);
    put(offsetY);
    put(clockTick);
    put(gUIAutoRedraw);
    put(gUIRealScreenHeight);
    put(gUIHebrew);
    put(lab(gUIFocusWindow));
    put(lab(gUICaptureDevice));
    put(lab(gUIKeyboardWindow));
    put(gUIHelpTextResult);
    for (const auto& s : gUIButtons) {
        put(s.mState);
        put(s.mX);
        put(s.mY);
        put(s.mF06);
        put(s.mTime);
    }
    return n;
}
}
