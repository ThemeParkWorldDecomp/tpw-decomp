// Test library for the sprite draw object, the scroll bar and the window's show and position leaves (libtpw_unit_UIScrollBar.dylib; see
// tests/test_UIScrollBar.py): src/game/ui/InterfaceDrawObjectSprite.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceScrollBar.cpp and src/game/ui/InterfaceWindow.cpp (the 33 functions
// of unit w30b), under them the real window constructor (src/game/ui/InterfaceWindow.cpp, which builds the test trees) and the real rectangles
// (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp).
//
// Everything else the unit reaches is a fake that records through calllog: the message senders and the pointer, the default window
// handler and the thumb's, the window's other members (the destructor, Enable, DrawMe, GetPosition, SetPosition(CoolRect*), SetValue,
// ClientDraw, SetCallbackFunction, CreateObject, CalcClientArea, DrawFunction), the draw object base class, the fake draw objects,
// the help text handler, the engine's sprite surfaces (SSpriteEngineData, SSpriteGameData, CSpriteList) and the map's Link and Unlink.
//
// The Python module drives this library through extern "C" operations on a small world: SLOTS windows (plain windows or scroll bars)
// made on the heap, a few fake draw objects and sprite draw objects, a fake map and surface. The dumps write the state in the order of the
// Python module's tables.
#include <cstdint>
#include <cstring>
#include <new>

#include "InterfaceScrollBar.h"
#include "calllog.h"
#include "ui_DrawObject.h"
#include "fake_arena_on_heap.h"

using namespace calllog;

namespace {
constexpr int kSlots = 10;
constexpr int kDraws = 4;
constexpr int kSprites = 4;
constexpr int kTokens = 3;
// labels (the Python module's LAB_*): windows 10.., fake draws 20.., sprite objects 30.., and for each sprite its game data 40.., surface
// 50.., list 60.. (and its link base: the same label) and instance array 80..; then the pixels, the map, the help text and the data tokens
constexpr int kWindowLabel = 10, kDrawLabel = 20, kSpriteLabel = 30, kGameDataLabel = 40, kSurfaceLabel = 50, kListLabel = 60,
              kInstancesLabel = 80, kPixelsLabel = 90, kMapLabel = 91, kHelpLabel = 92, kTokenLabel = 95;
constexpr int kKindNone = 0, kKindWindow = 1, kKindBar = 2;
}  // namespace

// ---- the globals the unit does not define (the screen's size is UI_Base.cpp's)
int32_t gUIAutoRedraw;
InterfaceHelpText* gUIHelpText;
CMapWho* gei_MapWho;

// ---- the senders and the pointer
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("SendMessage"));
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("PostMessage"));
}
int32_t UI_UpdatePointerIn() {
    ev("UpdatePointerIn");
    return 1;
}

// ---- the handlers: the window's default and the thumb's are fakes; which function a window's handler is shows in the dump
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultHandler", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultHandler"));
}
int32_t UI_DefaultThumbtrackCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("ThumbHandler", {lab(window), message, a, b});
    return 0;
}

namespace {
InterfaceWindow* gWindows[kSlots];
int gKinds[kSlots];

InterfaceWindow* Window(int64_t slot) { return slot >= 0 && slot < kSlots ? gWindows[slot] : nullptr; }

// Which handler a function is, for the dump and for SetCallbackFunction's event: 0 none, 1 the default, 2 the scroll bar's, 3 the thumb's.
int64_t CallbackCode(InterfaceWindowCallback callback) {
    if (callback == nullptr) return 0;
    if (callback == UI_DefaultWindowCallback) return 1;
    if (callback == UI_DefaultScrollBarCallback) return 2;
    if (callback == UI_DefaultThumbtrackCallback) return 3;
    return 4;
}
}  // namespace

// ---- the window's other members (the real ones are untranslated or another unit's)
void InterfaceWindow::DrawFunction() { ev("DrawFunction", {lab(this)}); }
// (the real one copies the rectangle to the client rectangle; the constructor reaches it)
void InterfaceWindow::CalcClientArea() {
    ev("CalcClientArea", {lab(this)});
    mClientRect.mLeft = mRect.mLeft;
    mClientRect.mTop = mRect.mTop;
    mClientRect.mRight = mRect.mRight;
    mClientRect.mBottom = mRect.mBottom;
}
InterfaceWindow::~InterfaceWindow() { ev("Window.dtor", {lab(this)}); }
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() { ev("Window.ClientDraw", {lab(this)}); }
// The real one toggles attribute bit 2 (disabled) and tells the handler and the pointer; the fake toggles the bit.
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Window.Enable", {lab(this), on});
    int16_t changed = 0;
    if (on == 0) {
        if ((mAttributes & 2) == 0) {
            changed = 1;
            mAttributes |= 2;
        }
    } else if ((mAttributes & 2) != 0) {
        changed = 1;
        mAttributes &= ~2u;
    }
    return changed;
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("Window.SetValue", {lab(this), value});
    return 1;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return -1; }
InterfaceWindow* InterfaceWindow::GetNext() { return mNext; }
int32_t InterfaceWindow::DrawMe() {
    ev("DrawMe", {lab(this)});
    return 1;
}
// The rectangle less the animation offsets (when asked), as the real one gives it.
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    if (rect == nullptr) return 0;
    rect->mLeft = mRect.mLeft;
    rect->mTop = mRect.mTop;
    rect->mRight = mRect.mRight;
    rect->mBottom = mRect.mBottom;
    if (subtract != 0) {
        rect->mLeft = static_cast<int16_t>(rect->mLeft - mAnimOffset[0]);
        rect->mTop = static_cast<int16_t>(rect->mTop - mAnimOffset[1]);
        rect->mRight = static_cast<int16_t>(rect->mRight - mAnimOffset[2]);
        rect->mBottom = static_cast<int16_t>(rect->mBottom - mAnimOffset[3]);
    }
    return 1;
}
// Records the rectangle and keeps it (the real one resizes the children and tells the draw objects too).
int32_t InterfaceWindow::SetPosition(CoolRect* rect) {
    if (rect == nullptr) {
        ev("SetPosition", {lab(this), 0, 0, 0, 0, 0});
        return 1;
    }
    ev("SetPosition", {lab(this), 1, rect->mLeft, rect->mTop, rect->mRight, rect->mBottom});
    mRect.mLeft = rect->mLeft;
    mRect.mTop = rect->mTop;
    mRect.mRight = rect->mRight;
    mRect.mBottom = rect->mBottom;
    return 1;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), CallbackCode(callback)});
    if (callback != nullptr) mCallback = callback;
}
// Makes the child the queue names (a slot of the world, made already) from the request: the id and the rectangles are the request's,
// and the window becomes the parent's last child. NULL when the queue gives no slot.
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    InterfaceWindow* child = Window(ret("CreateObject", -1));
    if (child == nullptr || child->mParent != nullptr) return nullptr;   // (a window that has a parent is not handed out again)
    InterfaceWindow* last = mChild;
    while (last != nullptr && last->mNext != nullptr) last = last->mNext;
    child->mParent = this;
    child->mNext = nullptr;
    child->mPrevious = last;
    if (last != nullptr) {
        last->mNext = child;
    } else {
        mChild = child;
    }
    child->mId = id;
    for (CoolRect* rect : {&child->mRect, &child->mOriginalRect, &child->mClientRect}) {
        rect->mLeft = left;
        rect->mTop = top;
        rect->mRight = right;
        rect->mBottom = bottom;
    }
    return child;
}

// ---- the help text handler: the base class is never called; the fake records OnHideOrDelete
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::Enable() {}
void InterfaceHelpText::Disable() {}
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t, int16_t) {}
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow*) {}
void InterfaceHelpText::OnMouseMovedOut() {}
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() {}
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow* window) { ev("Help.OnHideOrDelete", {lab(window)}); }
// The constructor and the two virtuals unit w36 added to the class (src/game/ui/InterfaceHelpText.cpp; the vtable must link here too).
InterfaceHelpText::InterfaceHelpText() {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

namespace {
class FakeHelp : public InterfaceHelpText {
public:
    void OnHideOrDelete(InterfaceWindow* window) override { ev("Help.OnHideOrDelete", {lab(window)}); }
};
FakeHelp gHelp;
}  // namespace

// ---- the draw object base class: the fake constructor sets the words the real one sets (and the depth, which it leaves) to the data and
// zeros, and records; the other members are never called except through the fake draw objects below
InterfaceDrawObject::InterfaceDrawObject(void* data) {
    ev("DrawObject.ctor", {lab(this), lab(data)});
    mData = data;
    mShown = 0;
    mFrameBase = 0;
    mFrame = 0;
    mFrameCount = 0;
    mInLibrary = 0;
    mResizable = 0;
    mRect.Set(0, 0, 0, 0);
    mRect2.Set(0, 0, 0, 0);
    mF28 = 0;
    mF2A = 0;
    mF2C = 0;
    mF2E = 0;
    mF30 = 0;
    mF32 = 0;
    mDepth = 0;
}
InterfaceDrawObject::~InterfaceDrawObject() { ev("DrawObject.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

namespace {
// A draw object that records what it is told: Show, and Change with the fields its flags name (the ones the unit uses: 8, the first
// word, the thumb's place).
class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    using InterfaceDrawObject::Draw;
    void Show(int16_t shown) override { ev("Draw.Show", {lab(this), shown}); }
    void Change(uint32_t flags, DOChange* change) override {
        int32_t first = 0;
        if (change != nullptr) std::memcpy(&first, change->mF00, sizeof first);
        ev("Draw.Change", {lab(this), flags, (flags & 8) != 0 ? first : 0, change == nullptr ? 1 : 0});
    }
};
FakeDraw gDraws[kDraws];
}  // namespace

// ---- the engine's sprite surfaces
namespace {
char gPixels[16];
char gMapStorage[16];
alignas(16) char gInstanceArrays[kSprites][16];   // what each sprite list's Init hands out (room for the one entry)
char gTokens[kTokens];

InterfaceDrawObjectSprite* gSprites[kSprites];

// Objects destroyed in place (a destructor called without a delete): their storage is freed at the next reset, unrecorded.
void* gDead[16];
int gDeadCount;
void Bury(void* block) {
    if (gDeadCount < 16) gDead[gDeadCount++] = block;
}

// The slot of the sprite whose list, game data or surface this is (by its label), or -1.
int64_t SpriteSlotOf(const void* object, int base) {
    const int64_t label = lab(object);
    return label >= base && label < base + kSprites ? label - base : -1;
}
}  // namespace

SSpriteGameData::SSpriteGameData() { ev("GameData.ctor", {lab(this)}); }
void SSpriteGameData::SetUsedPosition(int32_t x, int32_t y) { ev("SetUsedPosition", {lab(this), x, y}); }
void SSpriteGameData::SetUsedSize(int32_t width, int32_t height) { ev("SetUsedSize", {lab(this), width, height}); }

// (zeroes the words, as the real constructor does)
SSpriteEngineData::SSpriteEngineData() {
    ev("Surface.ctor", {lab(this)});
    mTexture = nullptr;
    mWords.mPixels = nullptr;
    mWords.mAspect = 0.0f;
    mWords.mWidth = 0;
    mWords.mHeight = 0;
}
SSpriteEngineData::~SSpriteEngineData() { ev("Surface.dtor", {lab(this)}); }
// The surface of the size asked for, with pixels unless the queue says the allocation failed.
int32_t SSpriteEngineData::Init(int32_t width, int32_t height) {
    ev("Surface.Init", {lab(this), width, height});
    mWords.mWidth = static_cast<uint16_t>(width);
    mWords.mHeight = static_cast<uint16_t>(height);
    const bool made = ret("Surface.Init", 1) != 0;
    mWords.mPixels = made ? gPixels : nullptr;
    return made ? 1 : 0;
}
int32_t SSpriteEngineData::RefreshPartialTexture() { ev("RefreshPartialTexture", {lab(this)}); return 1; }

CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() { ev("List.ctor", {lab(this)}); }
CSpriteList::~CSpriteList() { ev("List.dtor", {lab(this)}); }
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
SSpriteGameData** CSpriteList::Init(int32_t count) {
    ev("List.Init", {lab(this), count});
    const int64_t slot = SpriteSlotOf(this, kListLabel);
    return slot >= 0 ? reinterpret_cast<SSpriteGameData**>(gInstanceArrays[slot]) : nullptr;
}

namespace {
// The label of the sprite list a link base belongs to (the link base is the list as far as the log is concerned; on the Mac it is a
// separate part of the list's object, the PowerPC side labels its address with the list's label).
int64_t LinkLabel(CMapLinkBase* link) {
    const CSpriteList* list = dynamic_cast<CSpriteList*>(link);
    return list != nullptr ? lab(list) : lab(link);
}
}  // namespace
int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    ev("MapWho.Link", {lab(this), LinkLabel(link), list});
    return 1;
}
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    ev("MapWho.Unlink", {lab(this), LinkLabel(link), list});
    return 1;
}

namespace {
int64_t Heap[4];

int64_t L(const void* object) { return lab(object); }
void Put(int64_t*& out, int64_t value) { *out++ = value; }

bool Untouched(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i)
        if (bytes[i] != 0xa5) return false;
    return true;
}

uint32_t FloatBits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

// The size of the host's block for a slot's window.
std::size_t SizeOf(int kind) { return kind == kKindBar ? sizeof(InterfaceScrollBar) : sizeof(InterfaceWindow); }

void FreeWindow(int slot) {
    if (gWindows[slot] != nullptr) ::operator delete(gWindows[slot]);
    gWindows[slot] = nullptr;
    gKinds[slot] = kKindNone;
}
}  // namespace

extern "C" {

// Resets the world for a case: no windows and no sprites, the globals as the Mac's data has them (zero), the help text the fake, the
// map and the pixels at their labels.
void tpw_uisb_reset() {
    for (int i = 0; i < gDeadCount; ++i) ::operator delete(gDead[i]);
    gDeadCount = 0;
    for (int i = 0; i < kSlots; ++i) FreeWindow(i);
    for (int i = 0; i < kSprites; ++i) {
        if (gSprites[i] != nullptr) ::operator delete(gSprites[i]);
        gSprites[i] = nullptr;
    }
    gUIHelpText = &gHelp;
    gUIAutoRedraw = 0;
    gUIRealScreenWidth = 0;
    gUIRealScreenHeight = 0;
    gei_MapWho = reinterpret_cast<CMapWho*>(gMapStorage);
    std::memset(Heap, 0, sizeof Heap);
}

// Names the world's fixed objects for the call log (windows and sprites are named when made).
void tpw_uisb_labels() {
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(kDrawLabel + i, &gDraws[i]);
    tpw_cl_label(kPixelsLabel, gPixels);
    tpw_cl_label(kMapLabel, gMapStorage);
    tpw_cl_label(kHelpLabel, &gHelp);
    for (int i = 0; i < kSprites; ++i) tpw_cl_label(kInstancesLabel + i, gInstanceArrays[i]);
    for (int i = 0; i < kTokens; ++i) tpw_cl_label(kTokenLabel + i, &gTokens[i]);
}

// The words of a sprite draw object the dump reads, as one number each (see tpw_uisb_dump_sprite).
void tpw_uisb_dump_sprite(int32_t slot, int64_t* out) {
    const InterfaceDrawObjectSprite* s = gSprites[slot];
    Put(out, L(s->mData));
    Put(out, s->mShown);
    Put(out, s->mFrameBase);
    Put(out, s->mFrame);
    Put(out, s->mFrameCount);
    Put(out, s->mInLibrary);
    Put(out, s->mResizable);
    for (const CoolRect* rect : {&s->mRect, &s->mRect2}) {
        Put(out, rect->mLeft);
        Put(out, rect->mTop);
        Put(out, rect->mRight);
        Put(out, rect->mBottom);
    }
    Put(out, s->mF28);
    Put(out, s->mF2A);
    Put(out, s->mF2C);
    Put(out, s->mF2E);
    Put(out, s->mF30);
    Put(out, s->mF32);
    Put(out, s->mDepth);
    Put(out, s->mPixelWidth);
    Put(out, s->mPixelHeight);
    Put(out, Untouched(s->mF40, sizeof s->mF40));
    Put(out, s->mCreated);
    Put(out, Untouched(s->mF46, sizeof s->mF46));
    Put(out, s->mPreDraw == nullptr ? 0 : 1);
    Put(out, s->mF50);
    // the game data
    Put(out, static_cast<int32_t>(s->mSprite.mF10));
    Put(out, FloatBits(s->mSprite.mPosition.x));
    Put(out, FloatBits(s->mSprite.mPosition.y));
    Put(out, FloatBits(s->mSprite.mPosition.z));
    Put(out, L(s->mSprite.mSprite));
    Put(out, FloatBits(s->mSprite.mF2C));
    // the instance array and the list
    Put(out, L(s->mInstances));
    Put(out, s->mInstances == nullptr ? 0 : L(s->mInstances[0]));
    Put(out, s->mList.mCount);
    // the surface
    Put(out, L(s->mEngineData.mTexture));
    Put(out, L(s->mEngineData.mWords.mPixels));
    Put(out, s->mEngineData.mWords.mWidth);
    Put(out, s->mEngineData.mWords.mHeight);
}

// One operation of the script; a[] are its arguments, out[0] is the answer and out[1..4] more. The numbers are those of the Python module.
void tpw_uisb_op(int32_t op, const int64_t* a, int64_t* out) {
    out[0] = out[1] = out[2] = out[3] = out[4] = 0;
    InterfaceWindow* w = Window(a[0]);
    InterfaceScrollBar* bar = static_cast<InterfaceScrollBar*>(w);
    InterfaceDrawObjectSprite* sprite = a[0] >= 0 && a[0] < kSprites ? gSprites[a[0]] : nullptr;
    const int64_t heapBefore[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    switch (op) {
    case 1:     // a window: slot, parent slot, attributes, id, left, top, right, bottom
    case 6: {   // a scroll bar: the same arguments
        FreeWindow(static_cast<int>(a[0]));
        const int kind = op == 1 ? kKindWindow : kKindBar;
        void* block = ::operator new(SizeOf(kind));
        std::memset(block, 0xa5, SizeOf(kind));
        const int64_t made[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};   // (the block above is the test's own)
        tpw_cl_label(kWindowLabel + static_cast<int32_t>(a[0]), block);   // (before the constructor, which records an event)
        gKinds[a[0]] = kind;
        if (kind == kKindWindow) {
            gWindows[a[0]] = new (block) InterfaceWindow(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                                                         static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]),
                                                         static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]));
        } else {
            gWindows[a[0]] = new (block) InterfaceScrollBar(Window(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
                                                            static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]),
                                                            static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]));
        }
        out[0] = L(gWindows[a[0]]);
        for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - made[i];
        return;
    }
    case 2:   // links: slot, parent, previous, next, child
        w->mParent = Window(a[1]);
        w->mPrevious = Window(a[2]);
        w->mNext = Window(a[3]);
        w->mChild = Window(a[4]);
        return;
    case 3:   // fields: slot, attributes, destroying
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mDestroying = static_cast<uint8_t>(a[2]);
        return;
    case 4: {   // draw objects: slot, the window's (-1 none, 0.. a fake draw object, kDraws.. a sprite), the client's
        for (int i = 0; i < 2; ++i) {
            const int64_t k = a[1 + i];
            InterfaceDrawObject* object = nullptr;
            if (k >= 0 && k < kDraws) object = &gDraws[k];
            if (k >= kDraws && k < kDraws + kSprites) object = gSprites[k - kDraws];
            (i == 0 ? w->mDrawObject : w->mClientDrawObject) = object;
        }
        return;
    }
    case 5:   // handler: slot, which (0 none, 1 the default, 2 the scroll bar's, 3 the thumb's)
        w->mCallback = a[1] == 0 ? nullptr : a[1] == 1 ? UI_DefaultWindowCallback : a[1] == 2 ? UI_DefaultScrollBarCallback : UI_DefaultThumbtrackCallback;
        return;
    case 7:   // a global: which, value
        switch (a[0]) {
        case 1: gUIAutoRedraw = static_cast<int32_t>(a[1]); break;
        case 2: gUIRealScreenWidth = static_cast<int16_t>(a[1]); break;
        case 3: gUIRealScreenHeight = static_cast<int16_t>(a[1]); break;
        }
        return;
    case 8:   // the bar's words: slot, max, min, value, step, page, style
        bar->mMax = static_cast<int32_t>(a[1]);
        bar->mMin = static_cast<int32_t>(a[2]);
        bar->mValue = static_cast<int32_t>(a[3]);
        bar->mStep = static_cast<int32_t>(a[4]);
        bar->mPage = static_cast<int32_t>(a[5]);
        bar->mStyle = static_cast<int16_t>(a[6]);
        return;
    case 9: {   // a sprite draw object: slot, data token (-1 NULL)
        if (gSprites[a[0]] != nullptr) ::operator delete(gSprites[a[0]]);
        gSprites[a[0]] = nullptr;
        void* block = ::operator new(sizeof(InterfaceDrawObjectSprite));
        std::memset(block, 0xa5, sizeof(InterfaceDrawObjectSprite));
        const int64_t made[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
        const int32_t s = static_cast<int32_t>(a[0]);
        auto* p = static_cast<InterfaceDrawObjectSprite*>(block);
        tpw_cl_label(kSpriteLabel + s, block);   // (the parts are named before the constructor makes them)
        tpw_cl_label(kGameDataLabel + s, &p->mSprite);
        tpw_cl_label(kSurfaceLabel + s, &p->mEngineData);
        tpw_cl_label(kListLabel + s, &p->mList);
        gSprites[a[0]] = new (block) InterfaceDrawObjectSprite(a[1] >= 0 ? &gTokens[a[1]] : nullptr);
        out[0] = L(gSprites[a[0]]);
        for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - made[i];
        return;
    }
    case 10:   // a sprite's words: slot, created, pixel width, pixel height, shown, rectangle (4), depth, word 0x2c, surface width, surface height, pixels (0 none, 1 the buffer, 2 other)
        sprite->mCreated = static_cast<int16_t>(a[1]);
        sprite->mPixelWidth = static_cast<int16_t>(a[2]);
        sprite->mPixelHeight = static_cast<int16_t>(a[3]);
        sprite->mShown = static_cast<int16_t>(a[4]);
        sprite->mRect.mLeft = static_cast<int16_t>(a[5]);
        sprite->mRect.mTop = static_cast<int16_t>(a[6]);
        sprite->mRect.mRight = static_cast<int16_t>(a[7]);
        sprite->mRect.mBottom = static_cast<int16_t>(a[8]);
        sprite->mDepth = static_cast<int16_t>(a[9]);
        sprite->mF2C = static_cast<int16_t>(a[10]);
        sprite->mEngineData.mWords.mWidth = static_cast<uint16_t>(a[11]);
        sprite->mEngineData.mWords.mHeight = static_cast<uint16_t>(a[12]);
        sprite->mEngineData.mWords.mPixels =
            a[13] == 0 ? nullptr : a[13] == 1 ? static_cast<void*>(gPixels) : reinterpret_cast<void*>(static_cast<intptr_t>(0x12345678));
        return;
    case 11: {   // a window's rectangles: slot, which (0 the rectangle, 1 the client rectangle, 2 the animation offsets), the four
        if (a[1] == 2) {
            for (int i = 0; i < 4; ++i) w->mAnimOffset[i] = static_cast<int16_t>(a[2 + i]);
            return;
        }
        CoolRect& rect = a[1] == 0 ? w->mRect : w->mClientRect;
        rect.mLeft = static_cast<int16_t>(a[2]);
        rect.mTop = static_cast<int16_t>(a[3]);
        rect.mRight = static_cast<int16_t>(a[4]);
        rect.mBottom = static_cast<int16_t>(a[5]);
        return;
    }
    default:
        break;
    }
    // ---- the functions under test (op, slot in a[0])
    switch (op) {
    case 20: out[0] = w->Show(static_cast<int16_t>(a[1])); break;
    case 21: w->ShowAllDRAWOBJS(static_cast<int16_t>(a[1])); break;
    case 22: out[0] = L(w->GetWindow(static_cast<int32_t>(a[1]))); break;
    case 23: {   // SetPosition(short*, short*): slot, x, y, x null, y null
        int16_t x = static_cast<int16_t>(a[1]);
        int16_t y = static_cast<int16_t>(a[2]);
        out[0] = w->SetPosition(a[3] != 0 ? nullptr : &x, a[4] != 0 ? nullptr : &y);
        out[1] = x;
        out[2] = y;
        break;
    }
    case 24: out[0] = w->SetPosition(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4])); break;
    case 30: bar->CalcClientArea(); break;
    case 31: out[0] = bar->Enable(static_cast<int16_t>(a[1])); break;
    case 32: bar->ReCalcValue(); break;
    case 33: out[0] = bar->ReCalcThumbPos(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 34: bar->SetMinMax(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 35: out[0] = bar->SetValue(static_cast<int32_t>(a[1])); break;
    case 36: bar->SetPageValue(static_cast<int32_t>(a[1])); break;
    case 37: out[0] = bar->GetValue(); break;
    case 38: bar->AddIncButton(); break;
    case 39: bar->AddDecButton(); break;
    case 40: bar->AddThumbTrack(); break;
    case 41: bar->ClientClick(static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2])); break;
    case 42: out[0] = UI_DefaultScrollBarCallback(w, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])); break;
    case 43: out[0] = bar->WhatAmI(); break;
    case 44: bar->DrawFunction(); break;
    case 45: bar->ClientDraw(); break;
    case 46:   // the destructor: slot, flag (a positive short deletes the window; the window is gone either way)
        if (static_cast<int16_t>(a[1]) > 0) {
            delete bar;
            gWindows[a[0]] = nullptr;
            gKinds[a[0]] = kKindNone;
        } else {
            bar->~InterfaceScrollBar();
            Bury(bar);
            gWindows[a[0]] = nullptr;
            gKinds[a[0]] = kKindNone;
        }
        break;
    case 50: {   // Change: slot, flags, depth, word 0x2c, the rectangle (4), record null, rectangle null
        CoolRect rect(static_cast<int16_t>(a[4]), static_cast<int16_t>(a[5]), static_cast<int16_t>(a[6]), static_cast<int16_t>(a[7]));
        DOChange change;
        std::memset(&change, 0xa5, sizeof change);
        change.mDepth = static_cast<int16_t>(a[2]);
        change.mF08 = static_cast<int16_t>(a[3]);
        change.mRect = a[9] != 0 ? nullptr : &rect;
        sprite->Change(static_cast<uint32_t>(a[1]), a[8] != 0 ? nullptr : &change);
        break;
    }
    case 51: sprite->Show(static_cast<int16_t>(a[1])); break;
    case 52: {
        CoolRect rect(1, 2, 3, 4);
        sprite->Draw(static_cast<int16_t>(a[1]), &rect, static_cast<int16_t>(a[2]));
        break;
    }
    case 53: sprite->Refresh(); break;
    case 54: sprite->UpdateTextures(nullptr); break;
    case 55: sprite->CreateTextures(nullptr); break;
    case 56: {   // GetWritableDataArea: slot, first null, second null. out: the answer (the pixels' label when it is their address), the two sizes
        int32_t first = 0x5a5a;
        int32_t second = 0x5a5b;
        const int32_t answer = sprite->GetWritableDataArea(a[1] != 0 ? nullptr : &first, a[2] != 0 ? nullptr : &second);
        out[0] = answer == static_cast<int32_t>(reinterpret_cast<intptr_t>(gPixels)) && answer != 0 ? kPixelsLabel : answer;
        out[1] = first;
        out[2] = second;
        break;
    }
    case 57: sprite->SetPreDrawCallback(a[1] == 0 ? nullptr : reinterpret_cast<int32_t (*)(TbRenderArea&)>(static_cast<intptr_t>(a[1]))); break;
    case 58:   // the destructor: slot, flag (a positive short deletes the object; it is gone either way)
        if (static_cast<int16_t>(a[1]) > 0) {
            delete sprite;
            gSprites[a[0]] = nullptr;
            // (the parts' labels go too: a new object may be given the same memory)
            tpw_cl_label(kGameDataLabel + static_cast<int32_t>(a[0]), nullptr);
            tpw_cl_label(kSurfaceLabel + static_cast<int32_t>(a[0]), nullptr);
            tpw_cl_label(kListLabel + static_cast<int32_t>(a[0]), nullptr);
        } else {
            sprite->~InterfaceDrawObjectSprite();
            Bury(sprite);
            gSprites[a[0]] = nullptr;
        }
        break;
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - heapBefore[i];
}

// A window's state in the order of the Python module's dump_window: 68 words, and for a scroll bar 6 more.
void tpw_uisb_dump_window(int32_t slot, int64_t* out) {
    const InterfaceWindow* w = gWindows[slot];
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
    bool extrasZero = true;
    for (void* extra : w->mExtraParams) extrasZero = extrasZero && extra == nullptr;
    Put(out, extrasZero);
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
    Put(out, CallbackCode(w->mCallback));
    Put(out, CallbackCode(w->mDefaultCallback));
    Put(out, static_cast<int32_t>(w->mButtonsHeld));
    for (int i = 0; i < 4; ++i) Put(out, L(w->mAnimTimers[i]));
    Put(out, static_cast<int32_t>(w->mF130));
    Put(out, w->mF134);
    Put(out, w->mAnimating);
    Put(out, w->mSliding);
    if (gKinds[slot] == kKindBar) {
        const auto* bar = static_cast<const InterfaceScrollBar*>(w);
        Put(out, bar->mMax);
        Put(out, bar->mMin);
        Put(out, bar->mValue);
        Put(out, bar->mStep);
        Put(out, bar->mPage);
        Put(out, bar->mStyle);
    }
}

// The globals: the auto-redraw word, the screen's size, the map pointer's label, the help text's label.
void tpw_uisb_dump_globals(int64_t* out) {
    Put(out, gUIAutoRedraw);
    Put(out, gUIRealScreenWidth);
    Put(out, gUIRealScreenHeight);
    Put(out, L(gei_MapWho));
    Put(out, L(gUIHelpText));
}

void tpw_uisb_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) Put(out, Heap[i]);
}

}  // extern "C"
