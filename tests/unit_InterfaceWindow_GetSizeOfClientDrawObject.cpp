// Test library for six InterfaceWindow leaves (unit v5119; libtpw_unit_InterfaceWindow_GetSizeOfClientDrawObject.dylib,
// tests/test_InterfaceWindow_GetSizeOfClientDrawObject.py), main app:
//   src/InterfaceWindow_draw_InterfaceWindow_GetSizeOfClientDrawObject.cpp
//
// The callees outside the unit are fakes that record what the real ones read: the CoolRect members, SetPosition, the timers, the UI
// posts and the draw object's Change. gUIKeyboardWindow is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"

using namespace calllog;

InterfaceWindow* gUIKeyboardWindow;

// ---- CoolRegion and CoolRect (UI_Base.cpp): the rectangle's members record
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }

CoolRect::CoolRect() {
    mLeft = 0;
    mTop = 0;
    mRight = 0;
    mBottom = 0;
}
CoolRect::CoolRect(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("CoolRect", {left, top, right, bottom});
    mLeft = left;
    mTop = top;
    mRight = right;
    mBottom = bottom;
}
CoolRect::~CoolRect() { ev("~CoolRect", {mLeft, mTop, mRight, mBottom}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("Adjust", {mLeft, mTop, mRight, mBottom, left, top, right, bottom});
    mLeft = static_cast<int16_t>(mLeft + left);
    mTop = static_cast<int16_t>(mTop + top);
    mRight = static_cast<int16_t>(mRight + right);
    mBottom = static_cast<int16_t>(mBottom + bottom);
}

// ---- the draw object: only Change records; the rest is here for the vtable
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    DOTextStyle* style = change->mText;
    const char16_t* text = style != nullptr ? style->mText : nullptr;
    int64_t c[4];
    for (int i = 0; i < 4; i++) c[i] = text != nullptr ? text[i] : 0;
    ev("Change", {lab(this), static_cast<int32_t>(flags), 1, style != nullptr ? 1 : 0, text != nullptr ? 1 : 0, c[0], c[1], c[2], c[3]});
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// ---- the other callees
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return 0;
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn", {});
    return 0;
}
void InterfaceTimer::SetSlice(int32_t slice) { ev("SetSlice", {static_cast<int32_t>(reinterpret_cast<uintptr_t>(this)), slice}); }
InterfaceTimer* UI_AddTimer(InterfaceWindow* window, int32_t slice, int32_t id) {
    ev("AddTimer", {lab(window), slice, id});
    return reinterpret_cast<InterfaceTimer*>(static_cast<uintptr_t>(ret("AddTimer")));
}
int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    ev("SetPosition", {lab(this), rect->mLeft, rect->mTop, rect->mRight, rect->mBottom});
    return 1;
}

extern "C" {

uint64_t tpw_iw_addr(const char* name) {
    if (std::strcmp(name, "kb") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gUIKeyboardWindow));
    return 0;
}

uint64_t tpw_iw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A draw object with a real vtable (Change records).
uint64_t tpw_iw_alloc_do() {
    void* block = tpw::ArenaAllocZeroed(sizeof(InterfaceDrawObject));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (block) InterfaceDrawObject(nullptr)));
}

// rect, drawobj, clientdrawobj, anim, animframe, animoffset, animating, animtimers, sizeof window, CoolRect left, sizeof CoolRect,
// draw object rect2 left, sizeof draw object
int32_t tpw_iw_layout(int64_t* out) {
    auto* w = static_cast<InterfaceWindow*>(tpw::ArenaAllocZeroed(sizeof(InterfaceWindow)));
    auto* c = static_cast<CoolRect*>(tpw::ArenaAllocZeroed(sizeof(CoolRect)));
    auto* d = static_cast<InterfaceDrawObject*>(tpw::ArenaAllocZeroed(sizeof(InterfaceDrawObject)));
    auto off = [](const void* base, const void* p) { return static_cast<int64_t>(static_cast<const char*>(p) - static_cast<const char*>(base)); };
    int32_t n = 0;
    out[n++] = off(w, &w->mRect.mLeft);
    out[n++] = off(w, &w->mDrawObject);
    out[n++] = off(w, &w->mClientDrawObject);
    out[n++] = off(w, &w->mAnim);
    out[n++] = off(w, &w->mAnimFrame);
    out[n++] = off(w, &w->mAnimOffset[0]);
    out[n++] = off(w, &w->mAnimating);
    out[n++] = off(w, &w->mAnimTimers[0]);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    out[n++] = off(c, &c->mLeft);
    out[n++] = static_cast<int64_t>(sizeof(CoolRect));
    out[n++] = off(d, &d->mRect2.mLeft);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceDrawObject));
    return n;
}

static InterfaceWindow* W(uint64_t p) { return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(p)); }

void tpw_iw_size(uint64_t window, uint64_t rect) { W(window)->GetSizeOfClientDrawObject(reinterpret_cast<CoolRect*>(static_cast<uintptr_t>(rect))); }
void tpw_iw_adjust(uint64_t window, int32_t a, int32_t b, int32_t c, int32_t d) {
    W(window)->AdjustPosition(static_cast<int16_t>(a), static_cast<int16_t>(b), static_cast<int16_t>(c), static_cast<int16_t>(d));
}
uint32_t tpw_iw_uncapture(uint64_t window, int32_t on) {
    // the declaration answers void (shared with another unit); the Mac's answer is whether this window held the keyboard
    uint32_t held = gUIKeyboardWindow == W(window) ? 1 : 0;
    W(window)->UnCaptureKb(on);
    return held;
}
uint32_t tpw_iw_stop(uint64_t window) { return W(window)->stopAnim() ? 1 : 0; }
uint32_t tpw_iw_timer(uint64_t window, int32_t id, int32_t ms) { return static_cast<uint16_t>(W(window)->setAnimTimer(id, ms)); }
void tpw_iw_text(uint64_t window, uint64_t text) { W(window)->SetWindowDrawText(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text))); }

}  // extern "C"
