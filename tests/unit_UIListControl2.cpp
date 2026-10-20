// Unit w93: InterfaceListControl2's item management and InterfaceWindow::SetTextOffset and GetDims.
//
// Real: the unit's 17 functions and CoolRect. Fakes (each records through calllog, the same events as the PowerPC stubs in
// test_UIListControl2.py): the window's Show, SetPosition, setZ, SetValue, SetText, GetVisibility, the scroll bar, the draw object's
// Change and Show, the message pump, operator new, the string library (LCString), the row factory and the attach callback.
//
// The world is 15 windows: [0] the parent, [1] the list control, [2] its scroll bar (id 1), [3..6] four row windows and [7..14]
// two cell windows under each row (a row's cell windows are its child and that child's next sibling). Four draw objects.
//
// Sizes: a word of the item block, a pointer in an array and a string object are twice as wide here as on the Mac, so the
// allocation observer records half the bytes it is asked for; the PowerPC side records the bytes.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "InterfaceScrollBar.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

namespace {
void ObserveAllocation(std::size_t size, bool array, void* p);
}
#define TPW_ALLOCATION_OBSERVER(size, array, p) ObserveAllocation(size, array, p)
#include "calllog.cpp"

using namespace calllog;

namespace {
void ObserveAllocation(std::size_t size, bool array, void*) { ev(array ? "new[]" : "new", {static_cast<int64_t>(size / 2)}); }
}  // namespace

int32_t gUIAutoRedraw;

// ---- the string library (a recording boundary): a new string gets a representation of its own, assign replaces the characters
static char16_t* CopyText(const char16_t* text, uint32_t length) {
    char16_t* copy = static_cast<char16_t*>(std::malloc((length + 1) * sizeof(char16_t)));
    for (uint32_t i = 0; i < length; ++i) copy[i] = text[i];
    copy[length] = 0;
    return copy;
}
LCString::LCString(const char16_t* text) {
    ev("Str.new", {newlab(this)}, {narrow(text).c_str()});
    const uint32_t length = wcslen(text);
    mRep = static_cast<LCStringRep*>(std::calloc(1, sizeof(LCStringRep)));
    mRep->mLength = static_cast<int32_t>(length);
    mRep->mCapacity = static_cast<int32_t>(length);
    mRep->mRefCount = 1;
    mRep->mData = CopyText(text, length);
}
void LCString::assign(const char16_t* text, uint32_t length) {
    ev("Str.assign", {lab(this), static_cast<int64_t>(length)}, {narrow(text).c_str()});
    mRep->mLength = static_cast<int32_t>(length);
    mRep->mData = CopyText(text, length);
}

// ---- the MSL wide-string functions: faithful and unrecorded (16-bit characters, compared as unsigned)
uint32_t wcslen(const char16_t* text) {
    uint32_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}
int32_t wcsncmp(const char16_t* a, const char16_t* b, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        if (a[i] == 0) break;
    }
    return 0;
}

// ---- the message pump. The 0x403 message carries an address inside the item block: it is recorded as a word offset.
static int32_t BlockLow();
int32_t UI_PostMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Post", {lab(w), m, a, b});
    return static_cast<int32_t>(ret("Post"));
}
int32_t UI_SendMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    const int32_t shown = m == 0x403 ? (a - BlockLow()) / static_cast<int32_t>(sizeof(LCWord)) : a;
    ev("Send", {lab(w), m, shown, b});
    return static_cast<int32_t>(ret("Send"));
}

// ---- the window. Only what the unit calls records; the object keeps its layout and vtable.
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
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 1;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this)}, {narrow(text).c_str()});
    return 1;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }
InterfaceWindow* InterfaceWindow::GetChild(int32_t) { return mChild; }
InterfaceWindow* InterfaceWindow::GetNext() { return mNext; }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        if (child->mId == id) return child;
    }
    return nullptr;
}
int16_t InterfaceWindow::GetVisibility() {
    ev("Visibility", {lab(this)});
    return static_cast<int16_t>(ret("Visibility", 1));
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 1;
}
int32_t InterfaceWindow::SetPosition(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    ev("SetPosition", {lab(this), left, top, right, bottom});
    return 1;
}
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }

// The list control's constructor and virtuals only furnish the vtable.
InterfaceListControl2::InterfaceListControl2(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
InterfaceListControl2::~InterfaceListControl2() {}
void InterfaceListControl2::CalcClientArea() {}
void InterfaceListControl2::ClientDraw() {}
int32_t InterfaceListControl2::WhatAmI() { return 7; }

// The scroll bar: SetMinMax, SetPageValue and SetValue record.
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
void InterfaceScrollBar::SetMinMax(int32_t a, int32_t b) { ev("SetMinMax", {lab(this), a, b}); }
void InterfaceScrollBar::SetPageValue(int32_t page) { ev("SetPage", {lab(this), page}); }

// ---- the draw object: Change records the part of the change record its flags name; Show can change the user counts (as a
// virtual callee may), which is how the deletion in SetSelectionObject is reached.
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() { ev("Draw.dtor", {lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t flags, DOChange* change) {
    if (flags == 0x17) {
        ev("Change", {lab(this), flags, change->mFrame, change->mDepth, change->mF08, change->mRect->mLeft, change->mRect->mTop,
                      change->mRect->mRight, change->mRect->mBottom});
    } else if (flags == 0x10) {
        ev("Change", {lab(this), flags, change->mRect->mLeft, change->mRect->mTop, change->mRect->mRight, change->mRect->mBottom});
    } else if (flags == 0xc000) {
        int16_t x;
        int16_t y;
        std::memcpy(&x, &change->mText->mF0A[0], sizeof x);
        std::memcpy(&y, &change->mText->mF0A[2], sizeof y);
        ev("Change", {lab(this), flags, x, y});
    } else {
        ev("Change", {lab(this), flags});
    }
}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
namespace {
InterfaceListControl2* control();
}
void InterfaceDrawObject::Show(int16_t a) {
    ev("Draw.Show", {lab(this), a, mResizable, mInLibrary});
    if (ret("ShowMutate") != 0) {
        mResizable = static_cast<int16_t>(ret("ShowRefs"));
        mInLibrary = static_cast<int16_t>(ret("ShowLibrary"));
    }
    // the queue "ShowClear" (a 1) takes the selection object away from the control, as a Show that destroyed it would
    if (ret("ShowClear") != 0) control()->mItemDrawObject = nullptr;
}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

// ---- the row factory and the attach callback of SetColumnFormatsAll: windows come from the "Row" queue (an index, -1 for NULL)
namespace {
constexpr int kWindows = 15;
constexpr int kDraws = 4;
InterfaceWindow* windows[kWindows];
InterfaceDrawObject* draws[kDraws];
char16_t texts[100][8];
LCString* setupStrings[64];   // the strings the setup made: labelled 300 + k, as the PowerPC side does
int setupStringCount;

template <class T> T* make() {
    void* memory = ::operator new(sizeof(T));
    std::memset(memory, 0, sizeof(T));
    return new (memory) T(nullptr, 0, 0, 0, 0, 0, 0);
}
InterfaceListControl2* control() { return static_cast<InterfaceListControl2*>(windows[1]); }
}  // namespace
static int32_t BlockLow() { return static_cast<int32_t>(reinterpret_cast<intptr_t>(control()->mItemStorage)); }
namespace {

InterfaceWindow* MakeRow() {
    const int64_t which = ret("Row", -1);
    ev("MakeRow", {which});
    return which < 0 ? nullptr : windows[which];
}
void Attach(InterfaceWindow* w) { ev("Attach", {lab(w)}); }

// The text of a string cell: "t" and two digits.
const char16_t* Text(int64_t v) {
    char16_t* t = texts[v % 100];
    t[0] = u't';
    t[1] = static_cast<char16_t>(u'0' + (v % 100) / 10);
    t[2] = static_cast<char16_t>(u'0' + v % 10);
    t[3] = 0;
    return t;
}
// The cell kinds of the specs: 0 a number, 1 a string, 2 NULL.
void Cell(LCWord& word, int64_t kind, int64_t v) {
    word.mRaw = 0;
    if (kind == 0) word.mInt = static_cast<int32_t>(v);
    else if (kind == 1) word.mText = Text(v);
}
int32_t Stride() { return control()->mColumnCount + 4; }
}  // namespace

extern "C" {
// Setup, outside the observed heap interval. The blocks and arrays the code replaced are freed here; abandoned windows are not
// destroyed.
void tpw_lc_reset() {
    tpw_cl_on = 0;
    if (windows[1] != nullptr) {
        delete[] control()->mItemStorage;
        delete[] control()->mItemWindows;
    }
    for (auto*& w : windows) {
        if (w != nullptr) ::operator delete(w);
        w = nullptr;
    }
    for (auto*& d : draws) {
        if (d != nullptr) ::operator delete(d);
        d = nullptr;
    }
    windows[0] = make<InterfaceWindow>();
    windows[1] = make<InterfaceListControl2>();
    windows[2] = make<InterfaceScrollBar>();
    for (int i = 3; i < kWindows; ++i) windows[i] = make<InterfaceWindow>();
    for (auto*& d : draws) {
        void* block = ::operator new(sizeof(InterfaceDrawObject));
        std::memset(block, 0, sizeof(InterfaceDrawObject));
        d = new (block) InterfaceDrawObject(nullptr);
    }
    gUIAutoRedraw = 0;
    setupStringCount = 0;
}
// Fixed labels: windows 10..24, draw objects 60..63, the item window array 70, the columns 71, the setup's strings 300 and up.
void tpw_lc_labels() {
    for (int i = 0; i < kWindows; ++i) tpw_cl_label(10 + i, windows[i]);
    for (int i = 0; i < kDraws; ++i) tpw_cl_label(60 + i, draws[i]);
    tpw_cl_label(70, control()->mItemWindows);
    tpw_cl_label(71, control()->mColumns);
    for (int i = 0; i < setupStringCount; ++i) tpw_cl_label(300 + i, setupStrings[i]);
}
void tpw_lc_setup(int32_t op, const int64_t* a) {
    InterfaceListControl2* c = control();
    if (op == 0) {
        InterfaceWindow* w = windows[a[0]];
        w->mAttributes = static_cast<uint32_t>(a[1]);
        w->mId = static_cast<int32_t>(a[2]);
        w->mParent = a[3] < 0 ? nullptr : windows[a[3]];
        w->mChild = a[4] < 0 ? nullptr : windows[a[4]];
        w->mNext = a[5] < 0 ? nullptr : windows[a[5]];
    } else if (op == 1) {
        InterfaceWindow* w = windows[a[0]];
        CoolRect& r = a[1] == 0 ? w->mRect : static_cast<InterfaceListControl2*>(w)->mListRect;
        r.mLeft = static_cast<int16_t>(a[2]);
        r.mTop = static_cast<int16_t>(a[3]);
        r.mRight = static_cast<int16_t>(a[4]);
        r.mBottom = static_cast<int16_t>(a[5]);
    } else if (op == 2) {
        c->mCurrentEntry = static_cast<int32_t>(a[0]);
        c->mItemCount = static_cast<int32_t>(a[1]);
        c->mTopEntry = static_cast<int32_t>(a[2]);
        c->mFirstEntry = static_cast<int32_t>(a[3]);
        c->mRowHeight = static_cast<int16_t>(a[4]);
        c->mVisibleRows = static_cast<int16_t>(a[5]);
        c->mSortColumn = static_cast<int16_t>(a[6]);
        c->mSortDescending = static_cast<int16_t>(a[7]);
        c->mZ = static_cast<int16_t>(a[8]);
        c->mZRot = static_cast<int16_t>(a[9]);
        c->mAttributes = static_cast<uint32_t>(a[10]);
        c->mId = static_cast<int32_t>(a[11]);
    } else if (op == 3) {
        c->mColumnCount = static_cast<int16_t>(a[0]);
        c->mColumns = a[0] > 0 ? new CoolRect[a[0]] : nullptr;
        for (int i = 0; i < a[0]; ++i) c->mColumns[i].Set(static_cast<int16_t>(32 * i), 0, static_cast<int16_t>(32 * i + 32), static_cast<int16_t>(a[1 + i]));
    } else if (op == 4) {
        c->mItemWindows = new InterfaceWindow*[a[0]]();
    } else if (op == 5) {
        c->mItemWindows[a[0]] = a[1] < 0 ? nullptr : windows[a[1]];
    } else if (op == 6) {
        c->mItemDrawObject = a[0] < 0 ? nullptr : draws[a[0]];
    } else if (op == 7) {
        draws[a[0]]->mResizable = static_cast<int16_t>(a[1]);
        draws[a[0]]->mInLibrary = static_cast<int16_t>(a[2]);
    } else if (op == 8) {
        windows[a[0]]->mDrawObject = a[1] < 0 ? nullptr : draws[a[1]];
        windows[a[0]]->mClientDrawObject = a[2] < 0 ? nullptr : draws[a[2]];
    } else if (op == 9) {
        for (int i = 0; i < 4; ++i) windows[a[0]]->mAnimOffset[i] = static_cast<int16_t>(a[1 + i]);
    } else if (op == 10) {
        const int32_t words = static_cast<int32_t>(a[0]) * Stride();
        c->mItemStorage = a[0] > 0 ? new uint8_t[static_cast<size_t>(words) * sizeof(LCWord)]() : nullptr;
        c->mBlockWords = words;
    } else if (op == 11) {
        const int32_t stride = Stride();
        auto link = [&](int64_t v) { return v < 0 ? static_cast<int32_t>(v) : static_cast<int32_t>(v) * stride; };
        LCWord* e = c->Words() + a[0] * stride;
        e[kLCNext].mInt = link(a[1]);
        e[kLCData].mInt = static_cast<int32_t>(a[2]);
        e[kLCChild].mInt = link(a[3]);
        e[kLCParent].mInt = link(a[4]);
    } else if (op == 12) {
        LCWord* cell = c->Words() + a[0] * Stride() + kLCCells + a[1];
        if (a[2] == 1) {
            cell->mString = new LCString(Text(a[3]));
            setupStrings[setupStringCount++] = cell->mString;
        } else {
            Cell(*cell, a[2], a[3]);
        }
    } else if (op == 13) {
        gUIAutoRedraw = static_cast<int32_t>(a[0]);
    } else if (op == 14) {
        c->mColumns[a[0]].mLeft = static_cast<int16_t>(a[1]);
        c->mColumns[a[0]].mRight = static_cast<int16_t>(a[2]);
    }
}

// The function under test. out[0] is the answer; out[1] and out[2] are out parameters.
void tpw_lc_op(int32_t op, const int64_t* a, int64_t* out) {
    InterfaceListControl2* c = control();
    out[0] = out[1] = out[2] = 0;
    UI_LCAdd item[3];
    switch (op) {
    case 20: c->SetSelectionObject(a[0] < 0 ? nullptr : draws[a[0]]); break;
    case 21: out[0] = c->AllocNewBlock(static_cast<int32_t>(a[0])) != nullptr; break;
    case 22: c->UpdateDisplay(); break;
    case 23: c->SetColumnType(static_cast<int16_t>(a[0]), static_cast<LCDataType>(a[1])); break;
    case 24: out[0] = c->GetColumnType(static_cast<int16_t>(a[0])); break;
    case 25: c->SetColumnFormat(static_cast<int32_t>(a[0]), a[1] < 0 ? nullptr : windows[a[1]]); break;
    case 26: c->SetColumnFormatsAll(a[0] != 0 ? MakeRow : nullptr, a[1] != 0 ? Attach : nullptr); break;
    case 27: {
        int32_t change = -77;
        out[0] = c->AdvanceIndex(static_cast<int32_t>(a[0]), a[1] != 0 ? &change : nullptr);
        out[1] = change;
        break;
    }
    case 28: out[0] = c->GetIndexedEntry(static_cast<int32_t>(a[0])); break;
    case 29: {
        int32_t freeEntry = -77;
        int32_t previous = -77;
        c->FindFreeEntry(freeEntry, previous);
        out[1] = freeEntry;
        out[2] = previous;
        break;
    }
    case 30: {
        for (int i = 0; i < 3; ++i) Cell(item[i], a[1 + 2 * i], a[2 + 2 * i]);
        out[0] = c->ShouldIInsertEntryHere(item, c->Words() + a[0] * Stride() + kLCCells);
        break;
    }
    case 31: {
        for (int i = 0; i < 3; ++i) Cell(item[i], a[4 + 2 * i], a[5 + 2 * i]);
        out[0] = c->AddItem(a[3] == 2 ? nullptr : item, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
        break;
    }
    case 32: {
        for (int i = 0; i < 3; ++i) Cell(item[i], a[4 + 2 * i], a[5 + 2 * i]);
        c->ChangeItem(static_cast<int32_t>(a[0]), a[3] == 2 ? nullptr : item, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), 0);
        break;
    }
    case 37: {   // ChangeItem over many columns: a[3] pairs
        UI_LCAdd wide[128];
        for (int i = 0; i < a[3]; ++i) Cell(wide[i], a[4 + 2 * i], a[5 + 2 * i]);
        c->ChangeItem(static_cast<int32_t>(a[0]), wide, static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), 0);
        break;
    }
    case 33: {
        int32_t data = -77;
        out[0] = c->GetData(static_cast<int32_t>(a[0]), a[1] != 0 ? &data : nullptr);
        out[1] = data;
        break;
    }
    case 34: c->ChangeCurEntry(static_cast<int32_t>(a[0])); break;
    case 35: windows[a[0]]->SetTextOffset(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])); break;
    case 36: {
        int16_t w = 0x7abc;
        int16_t h = 0x7abd;
        out[0] = windows[a[0]]->GetDims(a[2] != 0 ? nullptr : &w, a[3] != 0 ? nullptr : &h, static_cast<int16_t>(a[1]));
        out[1] = w;
        out[2] = h;
        break;
    }
    }
    for (int i = 0; i < kDraws; ++i) {
        if (draws[i] != nullptr && lab(draws[i]) == -1) draws[i] = nullptr;
    }
}

// The control's state and the block, normalising every native pointer to its fixed label.
int32_t tpw_lc_dump(int64_t* out) {
    int64_t* start = out;
    auto put = [&](int64_t v) { *out++ = v; };
    InterfaceListControl2* c = control();
    put(c->mListRect.mLeft);
    put(c->mListRect.mTop);
    put(c->mListRect.mRight);
    put(c->mListRect.mBottom);
    put(c->mCurrentEntry);
    put(c->mItemCount);
    put(c->mTopEntry);
    put(c->mBlockWords);
    put(c->mFirstEntry);
    put(c->mColumnCount);
    put(c->mSortColumn);
    put(c->mSortDescending);
    put(c->mRowHeight);
    put(c->mVisibleRows);
    put(lab(c->mItemWindows));
    put(lab(c->mColumns));
    put(lab(c->mItemDrawObject));
    put(c->mItemStorage != nullptr);
    put(gUIAutoRedraw);
    for (int i = 0; i < c->mColumnCount && c->mColumns != nullptr; ++i) {
        put(c->mColumns[i].mLeft);
        put(c->mColumns[i].mTop);
        put(c->mColumns[i].mRight);
        put(c->mColumns[i].mBottom);
    }
    if (c->mItemWindows != nullptr) {
        for (int i = 0; i < 4 && i < c->mVisibleRows; ++i) put(lab(c->mItemWindows[i]));
    }
    const int32_t stride = c->mColumnCount + 4;
    for (int32_t i = 0; c->mItemStorage != nullptr && i < c->mBlockWords; ++i) {
        const int32_t column = i % stride - kLCCells;
        const LCWord& word = c->Words()[i];
        if (column >= 0 && c->mColumns != nullptr && c->mColumns[column].mBottom == kLCText) put(lab(word.mString));
        else put(word.mInt);
    }
    for (int i = 0; i < kDraws; ++i) {
        put(draws[i] != nullptr);
        if (draws[i] == nullptr) continue;
        put(draws[i]->mResizable);
        put(draws[i]->mInLibrary);
    }
    return static_cast<int32_t>(out - start);
}
}
