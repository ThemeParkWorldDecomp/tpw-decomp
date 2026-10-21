// Test library for the window script interpreter and its small members (libtpw_unit_UIScript.dylib; see tests/test_UIScript.py):
// src/InterfaceWindow_script.cpp (Build, ProcessScript, SetDrawObject, SetClientDrawObject, GetVisibility, SetCallbackFunction,
// CaptureKb) and src/ui_controls_small.cpp (the knob, list box and list control setters and the three region constructors), under
// them the real window constructor (src/InterfaceWindow.cpp) and the real rectangles (src/UI_Base.cpp).
//
// Everything else the unit reaches is a fake that records through calllog: CreateObject (it makes a window of the asked kind with the
// real constructor, links it as the parent's last child and gives it the kind WhatAmI answers), WhatAmI, GetWindow, the draw object
// library's FindDrawObject, the draw objects (Show, Change and the destructor record), the message senders, the window's other
// untranslated virtual members, the region classes' virtual members, UI_DefaultThumbtrackCallback and the assertion.
//
// The Python module drives this library through extern "C" operations on a small world: SLOTS windows made by the test, up to
// CREATED more made by CreateObject, a few draw objects, fonts, two regions and the scripts. The dumps write the state in the order of
// the Python module's tables.
#include <cstdint>
#include <cstring>
#include <new>
#include <typeinfo>

#include "calllog.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kSlots = 8;
constexpr int kCreated = 320;
constexpr int kDraws = 6;
constexpr int kRegions = 2;
constexpr int kFonts = 3;
constexpr int kScripts = 6;
constexpr int kScriptWords = 4096;
constexpr int kBlock = 640;   // the room of a window (the largest control class fits)
constexpr int kWindowLabel = 10, kDrawLabel = 20, kRegionLabel = 30, kFontLabel = 50;
constexpr int kMaxColumns = 8;   // columns of a list control the dump reads
static_assert(sizeof(InterfaceListControl2) <= kBlock, "the window block is too small");

alignas(16) unsigned char gWorldBlocks[kSlots][kBlock];
alignas(16) unsigned char gCreatedBlocks[kCreated][kBlock];
InterfaceWindow* gWorld[kSlots];
InterfaceWindow* gCreatedWindows[kCreated];
int32_t gWorldKinds[kSlots];
int32_t gCreatedKinds[kCreated];
int gCreatedCount;
int16_t gScripts[kScripts][kScriptWords];
int64_t Heap[4];
}  // namespace

// ---- the unit allocates with new (std::nothrow); the library's default for that form does not go through the counting operator new.
// The queue "NewFails" makes a call answer NULL (a nonzero value), as the PowerPC side's operator new does.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (ret("NewFails") != 0) return nullptr;
    return operator new(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return operator new[](size); }

// ---- the senders and the pointer
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}

// ---- the globals the unit does not define
InterfaceWindow* gUIKeyboardWindow;

// ---- the handlers (never called: only their addresses matter)
int32_t UI_DefaultWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_DefaultThumbtrackCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
namespace {
int32_t CallbackA(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t CallbackB(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceWindowCallback CallbackOf(int64_t code) {
    return code == 1 ? CallbackA : code == 2 ? CallbackB : code == 3 ? UI_DefaultThumbtrackCallback : nullptr;
}
}  // namespace

// ---- the assertion
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }

namespace {
// The kind of a window: what the fake CreateObject was asked for (the world's windows are given theirs); 1 for any other.
int32_t KindOf(const InterfaceWindow* window) {
    for (int i = 0; i < kSlots; ++i)
        if (gWorld[i] == window) return gWorldKinds[i];
    for (int i = 0; i < gCreatedCount; ++i)
        if (gCreatedWindows[i] == window) return gCreatedKinds[i];
    return 1;
}
}  // namespace

// ---- the window's untranslated members
void InterfaceWindow::DrawFunction() {}
// (the real one copies the rectangle to the client rectangle; the constructor reaches it)
void InterfaceWindow::CalcClientArea() {
    ev("CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
int32_t InterfaceWindow::WhatAmI() {
    ev("WhatAmI", {lab(this)});
    return KindOf(this);
}
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
// The window with an id: the window itself when the queue says there is one, else NULL.
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") != 0 ? this : nullptr;
}

// The factory: a window of the asked kind, linked as the last child of its parent. The queue "CreateObject" says which calls fail
// (a nonzero value answers NULL).
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    if (ret("CreateObject") != 0 || gCreatedCount >= kCreated) return nullptr;
    unsigned char* block = gCreatedBlocks[gCreatedCount];
    std::memset(block, 0xa5, kBlock);
    newlab(block);   // (before the constructor, which records events)
    auto* window = new (block) InterfaceWindow(this, attributes, id, left, top, right, bottom);
    if (kind == 4 || kind == 7 || kind == 0xb) std::memset(block + sizeof(InterfaceWindow), 0, kBlock - sizeof(InterfaceWindow));
    gCreatedWindows[gCreatedCount] = window;
    gCreatedKinds[gCreatedCount] = kind;
    ++gCreatedCount;
    InterfaceWindow* parent = this;
    __asm__ volatile("" : "+r"(parent));   // (the parent is NULL when the script's window is: the compiler must not assume otherwise)
    if (parent != nullptr) {
        InterfaceWindow* last = parent->mChild;
        if (last == nullptr) {
            parent->mChild = window;
        } else {
            while (last->mNext != nullptr) last = last->mNext;
            last->mNext = window;
            window->mPrevious = last;
        }
    }
    return window;
}

// ---- the draw object library and the draw objects
namespace {
// Deleting an object of the world only records it: the storage is the world's, and a window may go on pointing at the object (the
// original's SetClientDrawObject leaves such a pointer). The destructor has put the base class's vtable pointer in the object by
// the time operator delete runs; it is put back, so that a later call or a second delete behaves as the PowerPC side's object,
// which the fake destructor does not change.
void* VtablePointerOf(const void* object) {
    void* pointer;
    std::memcpy(&pointer, object, sizeof pointer);
    return pointer;
}
void RecordDelete(void* object, void* vtablePointer) {
    std::memcpy(object, &vtablePointer, sizeof vtablePointer);
    ++tpw_cl_heap[2];
    if (calllog::on() && calllog::lab(object) > 0) {
        calllog::ev("delete", {calllog::lab(object)});
        calllog::forget(object);
    }
}

class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    static void operator delete(void* object);
};
FakeDraw gDraws[kDraws];
void* gDrawVtablePointer;
void FakeDraw::operator delete(void* object) { RecordDelete(object, gDrawVtablePointer); }
InterfaceFont gFonts[kFonts];

// Which rectangle of which window a rectangle is: 10 * the window's label + 1 its own rectangle, 2 the original, 3 the previous,
// 4 the client rectangle, 5 the one at 0x34; -1 for any other.
int64_t RectCode(const CoolRect* rect) {
    for (int i = 0; i < kSlots + gCreatedCount; ++i) {
        const InterfaceWindow* w = i < kSlots ? gWorld[i] : gCreatedWindows[i - kSlots];
        if (w == nullptr) continue;
        const int64_t base = 10 * static_cast<int64_t>(lab(w));
        if (rect == &w->mRect) return base + 1;
        if (rect == &w->mOriginalRect) return base + 2;
        if (rect == &w->mPreviousRect) return base + 3;
        if (rect == &w->mClientRect) return base + 4;
        if (rect == &w->mRect34) return base + 5;
    }
    return -1;
}
}  // namespace

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(uint32_t hash) {
    ev("FindDrawObject", {static_cast<int64_t>(hash)});
    const int64_t index = ret("FindDrawObject", -1);
    return index >= 0 && index < kDraws ? &gDraws[index] : nullptr;
}

InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
// What the window told it: the fields its flags name, then the text style's byte.
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    ev("Change", {lab(this), flags, change->mFrame, change->mDepth, change->mF08, RectCode(change->mRect), change->mRect->mLeft,
                  change->mRect->mTop, change->mRect->mRight, change->mRect->mBottom, change->mF0C, change->mF0E});
    if ((flags & 0x10000) != 0) ev("Change.style", {lab(this), change->mText->mF06[2]});
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t shown) { ev("Draw.Show", {lab(this), shown}); }
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// ---- the regions
namespace {
// A region of the world: deleting one records it and keeps the storage (see FakeDraw).
class FakeRegion : public CoolRegion {
public:
    ~FakeRegion() override { ev("FakeRegion.dtor", {lab(this)}); }
    static void operator delete(void* object);
};
FakeRegion gRegions[kRegions];
void* gRegionVtablePointer;
void FakeRegion::operator delete(void* object) { RecordDelete(object, gRegionVtablePointer); }
}  // namespace

// The shapes' other virtual members are not part of the unit: the destructors record, the others are never called.
bool CoolCircle::PointIn(int16_t, int16_t) { return false; }
void CoolCircle::Shift(int16_t, int16_t) {}
void CoolCircle::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolCircle::~CoolCircle() { ev("CoolCircle.dtor", {lab(this)}); }
bool CoolEllipse::PointIn(int16_t, int16_t) { return false; }
void CoolEllipse::Shift(int16_t, int16_t) {}
void CoolEllipse::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolEllipse::~CoolEllipse() { ev("CoolEllipse.dtor", {lab(this)}); }
bool CoolPoly::PointIn(int16_t, int16_t) { return false; }
void CoolPoly::Shift(int16_t, int16_t) {}
void CoolPoly::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolPoly::~CoolPoly() { ev("CoolPoly.dtor", {lab(this)}); }

// ---- the fonts: a font only has an id here
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}

namespace {
InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots ? gWorld[slot] : nullptr; }
int64_t L(const void* object) { return lab(object); }
void Put(int64_t*& out, int64_t value) { *out++ = value; }

bool Untouched(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xa5) return false;
    return true;
}
bool Zero(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0) return false;
    return true;
}
}  // namespace

extern "C" {

// Resets the world for a case: no windows, the draw objects clean and the globals as the Mac's data has them.
void tpw_uis_reset() {
    for (int i = 0; i < kSlots; ++i) {
        gWorld[i] = nullptr;
        gWorldKinds[i] = 1;
    }
    for (int i = 0; i < kCreated; ++i) gCreatedWindows[i] = nullptr;
    gCreatedCount = 0;
    for (FakeDraw& draw : gDraws) new (&draw) FakeDraw;
    gDrawVtablePointer = VtablePointerOf(&gDraws[0]);
    for (FakeRegion& region : gRegions) new (&region) FakeRegion;
    gRegionVtablePointer = VtablePointerOf(&gRegions[0]);
    for (int i = 0; i < kFonts; ++i) gFonts[i].mID = 0x100 + 7 * i + 1;   // (the low byte is the style's)
    gUIKeyboardWindow = nullptr;
    gUIScriptPosition = 0;
    gUILastWindow = nullptr;
    for (auto& script : gScripts)
        for (int16_t& word : script) word = 5;   // (after a script's words come only ends of levels, so that a stray read stops)
    std::memset(Heap, 0, sizeof Heap);
}

// Names the world's fixed objects for the call log (windows are named when made).
void tpw_uis_labels() {
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(kDrawLabel + i, &gDraws[i]);
    for (int i = 0; i < kRegions; ++i) tpw_cl_label(kRegionLabel + i, &gRegions[i]);
    for (int i = 0; i < kFonts; ++i) tpw_cl_label(kFontLabel + i, &gFonts[i]);
}

// A script: `count` words.
void tpw_uis_script(int32_t k, int32_t count, const int16_t* words) {
    for (int i = 0; i < count && i < kScriptWords; ++i) gScripts[k][i] = words[i];
}

// The font ids of the world (so that the Python side can give the same ones).
void tpw_uis_font_id(int32_t k, int32_t id) { gFonts[k].mID = id; }

// One operation of the script; a[] are its arguments, out[0] is the answer and out[1..4] more. The numbers are those of the
// Python module.
void tpw_uis_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    InterfaceWindow* w = Window(a[0]);
    const int64_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    switch (op) {
    case 1: {   // a window: slot, parent slot, attributes, id, left, top, right, bottom, kind
        unsigned char* block = gWorldBlocks[a[0]];
        std::memset(block, 0xa5, kBlock);
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);   // (before the constructor, which records events)
        gWorld[a[0]] = new (block) InterfaceWindow(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                                                   static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]),
                                                   static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]));
        gWorldKinds[a[0]] = static_cast<int32_t>(a[8]);
        const int kind = static_cast<int32_t>(a[8]);
        if (kind == 4 || kind == 7 || kind == 0xb) std::memset(block + sizeof(InterfaceWindow), 0, kBlock - sizeof(InterfaceWindow));
        out[0] = L(gWorld[a[0]]);
        return;
    }
    case 2:   // links: slot, parent, previous, next, child
        w->mParent = Window(a[1]);
        w->mPrevious = Window(a[2]);
        w->mNext = Window(a[3]);
        w->mChild = Window(a[4]);
        return;
    case 3:   // fields: slot, attributes, region (0 own rectangle, 1 and 2 a fake region, -1 none), font (-1 none), z, z rotation, pivot x and y
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mRegion = a[2] == 0 ? static_cast<CoolRegion*>(&w->mRect) : a[2] < 0 ? nullptr : &gRegions[a[2] - 1];
        w->mFont = a[3] >= 0 ? &gFonts[a[3]] : nullptr;
        w->mZ = static_cast<int16_t>(a[4]);
        w->mZRot = static_cast<int16_t>(a[5]);
        w->mPivotX = static_cast<int16_t>(a[6]);
        w->mPivotY = static_cast<int16_t>(a[7]);
        return;
    case 4:   // draw objects: slot, the window's (-1 none), the client's
        w->mDrawObject = a[1] >= 0 ? &gDraws[a[1]] : nullptr;
        w->mClientDrawObject = a[2] >= 0 ? &gDraws[a[2]] : nullptr;
        return;
    case 5:   // a draw object's counts: index, in library, resizable (the user count), shown
        gDraws[a[0]].mInLibrary = static_cast<int16_t>(a[1]);
        gDraws[a[0]].mResizable = static_cast<int16_t>(a[2]);
        gDraws[a[0]].mShown = static_cast<int16_t>(a[3]);
        return;
    case 6: {   // control state: slot, which (1 knob, 2 list box rectangle, 3 list control rectangle, 4 column count), values
        if (a[1] == 1) {
            auto* knob = static_cast<InterfaceKnob*>(w);
            knob->mMinAngle = static_cast<int32_t>(a[2]);
            knob->mMaxAngle = static_cast<int32_t>(a[3]);
            knob->mAngleRange = static_cast<int32_t>(a[4]);
        } else if (a[1] == 2) {
            static_cast<InterfaceListBox*>(w)->mListRect.Set(static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]),
                                                             static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]));
        } else if (a[1] == 3) {
            static_cast<InterfaceListControl2*>(w)->mListRect.Set(static_cast<int16_t>(a[2]), static_cast<int16_t>(a[3]),
                                                                  static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]));
        } else if (a[1] == 4) {
            static_cast<InterfaceListControl2*>(w)->mColumnCount = static_cast<int16_t>(a[2]);
        }
        return;
    }
    case 7:   // a global: which (1 the keyboard window, 2 the script position), value
        if (a[0] == 1) gUIKeyboardWindow = Window(a[1]);
        if (a[0] == 2) gUIScriptPosition = static_cast<int32_t>(a[1]);
        return;
    default:
        break;
    }
    // ---- the functions under test
    switch (op) {
    case 20: {   // ProcessScript: slot, script, start position (the window is NULL for a slot outside the world)
        gUIScriptPosition = static_cast<int32_t>(a[2]);
        out[0] = L(w->ProcessScript(gScripts[a[1]]));
        out[1] = gUIScriptPosition;
        break;
    }
    case 21: {   // Build: slot, script, callback
        out[0] = L(w->Build(gScripts[a[1]], CallbackOf(a[2])));
        out[1] = gUIScriptPosition;
        break;
    }
    case 22: w->SetDrawObject(a[1] >= 0 ? &gDraws[a[1]] : nullptr); break;
    case 23: w->SetClientDrawObject(a[1] >= 0 ? &gDraws[a[1]] : nullptr); break;
    case 24: out[0] = w->GetVisibility(); break;
    case 25: w->SetCallbackFunction(CallbackOf(a[1])); break;
    case 26: w->CaptureKb(static_cast<int32_t>(a[1])); break;
    case 27: {   // SetMinMaxAngles: slot, has minimum, has maximum, minimum, maximum
        int32_t minimum = static_cast<int32_t>(a[3]);
        int32_t maximum = static_cast<int32_t>(a[4]);
        static_cast<InterfaceKnob*>(w)->SetMinMaxAngles(a[1] != 0 ? &minimum : nullptr, a[2] != 0 ? &maximum : nullptr);
        break;
    }
    case 28:
        static_cast<InterfaceListBox*>(w)->SetListRect(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]),
                                                       static_cast<int16_t>(a[3]), static_cast<int16_t>(a[4]));
        break;
    case 29:
        static_cast<InterfaceListControl2*>(w)->SetListRect(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]),
                                                            static_cast<int16_t>(a[3]), static_cast<int16_t>(a[4]));
        break;
    case 30: static_cast<InterfaceListControl2*>(w)->SetColumns(static_cast<int16_t>(a[1])); break;
    case 31:
        static_cast<InterfaceListControl2*>(w)->SetColumnPos(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]),
                                                             static_cast<int16_t>(a[3]));
        break;
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - heapBefore[i];
}

// The region constructors: kind (1 circle, 2 ellipse, 3 polygon), the values, the script that holds the points. The object is built
// in a buffer filled with 0xa5. out: 1 (the object has the vtable of its class), then the words of the object (circle 3, ellipse 4,
// polygon: count, whether the points are a copy, the points). Answers how many words.
int32_t tpw_uis_region(int32_t kind, const int64_t* a, int32_t script, int64_t* out) {
    alignas(16) unsigned char buffer[64];
    std::memset(buffer, 0xa5, sizeof buffer);
    const int64_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    int n = 0;
    out[n++] = 1;
    int16_t* copy = nullptr;
    if (kind == 1) {
        auto* circle = new (buffer) CoolCircle(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]));
        out[n++] = circle->mX;
        out[n++] = circle->mY;
        out[n++] = circle->mRadius;
    } else if (kind == 2) {
        auto* ellipse = new (buffer) CoolEllipse(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]),
                                                 static_cast<int16_t>(a[3]));
        out[n++] = ellipse->mX;
        out[n++] = ellipse->mY;
        out[n++] = ellipse->mRadiusX;
        out[n++] = ellipse->mRadiusY;
    } else {
        auto* poly = new (buffer) CoolPoly(static_cast<int16_t>(a[0]), gScripts[script]);
        copy = poly->mPoints;
        out[n++] = poly->mCount;
        out[n++] = poly->mPoints != gScripts[script];
        for (int i = 0; i < 2 * poly->mCount; ++i) out[n++] = poly->mPoints[i];
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - heapBefore[i];
    delete[] copy;   // (the polygon's destructor is not part of the unit, so this is not counted)
    return n;
}

// A window's state: the words of the Mac's window in the order of the Python module's dump_window, the kind, the region and the
// control's own words. Answers the length.
int32_t tpw_uis_dump_window(int32_t index, int64_t* out) {
    const InterfaceWindow* w = index < kSlots ? gWorld[index] : gCreatedWindows[index - kSlots];
    int64_t* start = out;
    Put(out, w->mDestroying);
    for (const CoolRect* rect : {&w->mRect, &w->mOriginalRect, &w->mPreviousRect, &w->mClientRect, &w->mRect34}) {
        Put(out, rect->mLeft);
        Put(out, rect->mTop);
        Put(out, rect->mRight);
        Put(out, rect->mBottom);
    }
    Put(out, w->mRegion == nullptr ? 0 : w->mRegion == &w->mRect ? 1 : L(w->mRegion));
    Put(out, static_cast<int32_t>(w->mAttributes));
    Put(out, L(w->mFont));
    Put(out, w->mCursor);
    Put(out, L(w->mNext));
    Put(out, L(w->mPrevious));
    Put(out, L(w->mChild));
    Put(out, L(w->mParent));
    Put(out, static_cast<int32_t>(w->mF60));
    Put(out, static_cast<int32_t>(w->mF64));
    Put(out, Untouched(w->mF68, sizeof w->mF68));
    Put(out, w->mId);
    Put(out, w->mHelpText);
    Put(out, w->mF90);
    Put(out, L(w->mDrawObject));
    Put(out, L(w->mClientDrawObject));
    Put(out, Zero(w->mExtraParams, sizeof w->mExtraParams));
    Put(out, w->mZ);
    Put(out, w->mZRot);
    Put(out, w->mPivotX);
    Put(out, w->mPivotY);
    Put(out, L(w->mAnim));
    Put(out, w->mAnimFrame);
    Put(out, w->mAnimTicks);
    Put(out, w->mAnimStart);
    for (int i = 0; i < 4; ++i) Put(out, w->mAnimOffset[i]);
    Put(out, L(w->mSlideTimer));
    for (int i = 0; i < 4; ++i) Put(out, w->mSlideStep[i]);
    Put(out, w->mSlideSteps);
    Put(out, Untouched(w->mF10A, sizeof w->mF10A));
    // the message function: 0 none, 1 the default handler, 2 the thumb track's, 3 and 4 the world's two, 5 any other
    Put(out, w->mCallback == nullptr ? 0
             : w->mCallback == UI_DefaultWindowCallback ? 1
             : w->mCallback == UI_DefaultThumbtrackCallback ? 2
             : w->mCallback == CallbackA ? 3
             : w->mCallback == CallbackB ? 4 : 5);
    Put(out, w->mDefaultCallback == nullptr ? 0 : w->mDefaultCallback == UI_DefaultWindowCallback ? 1 : 2);
    Put(out, static_cast<int32_t>(w->mButtonsHeld));
    for (int i = 0; i < 4; ++i) Put(out, L(w->mAnimTimers[i]));
    Put(out, static_cast<int32_t>(w->mF130));
    Put(out, w->mF134);
    Put(out, w->mAnimating);
    Put(out, w->mSliding);
    // ---- the kind the fakes know and the region
    Put(out, KindOf(w));
    const CoolRegion* region = w->mRegion;
    if (region == nullptr || region == &w->mRect) {
        for (int i = 0; i < 5; ++i) Put(out, 0);
    } else if (typeid(*region) == typeid(CoolRect)) {
        const auto* rect = static_cast<const CoolRect*>(region);
        Put(out, 3);
        Put(out, rect->mLeft);
        Put(out, rect->mTop);
        Put(out, rect->mRight);
        Put(out, rect->mBottom);
    } else if (typeid(*region) == typeid(CoolCircle)) {
        const auto* circle = static_cast<const CoolCircle*>(region);
        Put(out, 4);
        Put(out, circle->mX);
        Put(out, circle->mY);
        Put(out, circle->mRadius);
        Put(out, 0);
    } else if (typeid(*region) == typeid(CoolEllipse)) {
        const auto* ellipse = static_cast<const CoolEllipse*>(region);
        Put(out, 5);
        Put(out, ellipse->mX);
        Put(out, ellipse->mY);
        Put(out, ellipse->mRadiusX);
        Put(out, ellipse->mRadiusY);
    } else if (typeid(*region) == typeid(CoolPoly)) {
        const auto* poly = static_cast<const CoolPoly*>(region);
        Put(out, 6);
        Put(out, poly->mCount);
        Put(out, poly->mPoints != nullptr);
        Put(out, 0);
        Put(out, 0);
        for (int i = 0; i < 2 * poly->mCount; ++i) Put(out, poly->mPoints[i]);
    } else {
        Put(out, 2);   // a fake region of the world
        Put(out, L(region));
        for (int i = 0; i < 3; ++i) Put(out, 0);
    }
    // ---- the control's own words
    const int32_t kind = KindOf(w);
    if (kind == 0xb) {
        const auto* knob = static_cast<const InterfaceKnob*>(w);
        Put(out, knob->mMinAngle);
        Put(out, knob->mMaxAngle);
        Put(out, knob->mAngleRange);
    } else if (kind == 4) {
        const auto* list = static_cast<const InterfaceListBox*>(w);
        Put(out, list->mListRect.mLeft);
        Put(out, list->mListRect.mTop);
        Put(out, list->mListRect.mRight);
        Put(out, list->mListRect.mBottom);
    } else if (kind == 7) {
        const auto* list = static_cast<const InterfaceListControl2*>(w);
        Put(out, list->mListRect.mLeft);
        Put(out, list->mListRect.mTop);
        Put(out, list->mListRect.mRight);
        Put(out, list->mListRect.mBottom);
        Put(out, list->mColumnCount);
        Put(out, list->mColumns != nullptr);
        if (list->mColumns != nullptr && list->mColumnCount > 0 && list->mColumnCount <= kMaxColumns) {
            for (int i = 0; i < list->mColumnCount; ++i) {
                Put(out, list->mColumns[i].mLeft);
                Put(out, list->mColumns[i].mTop);
                Put(out, list->mColumns[i].mRight);
                Put(out, list->mColumns[i].mBottom);
            }
        }
    }
    return static_cast<int32_t>(out - start);
}

// How many windows the fakes made.
int32_t tpw_uis_created() { return gCreatedCount; }

// The globals and the draw objects: the keyboard window, the script position, then for each draw object its label-free words.
void tpw_uis_dump_globals(int64_t* out) {
    Put(out, L(gUIKeyboardWindow));
    Put(out, gUIScriptPosition);
    for (const FakeDraw& draw : gDraws) {
        Put(out, draw.mShown);
        Put(out, draw.mFrameBase);
        Put(out, draw.mFrame);
        Put(out, draw.mInLibrary);
        Put(out, draw.mResizable);
    }
}

void tpw_uis_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) Put(out, Heap[i]);
}

}  // extern "C"
