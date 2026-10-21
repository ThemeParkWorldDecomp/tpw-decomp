// Fakes and the driver for the highestZ unit (src/game/ui/UI_Base.cpp, InterfaceListControl2::GetZOfEntries in
// src/game/ui/CoolRegion.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp). Real: highestZ, GetZOfEntries, InterfaceWindow's constructor, GetPosition, the rectangles. Fakes: the
// window's other members and the three tree walkers (copies of UI_pump.cpp's, tested in test_UIPump), the list control's
// constructor and virtuals (WhatAmI answers 7 as the real one does), the shapes' virtuals, and the UI calls the constructor makes.
//
// The test builds a forest of up to 12 windows (plain windows and list controls) from a spec, links them by the order of the specs,
// and calls highestZ on one of them with the answer word and the root set by the case.
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "calllog.h"
#include <cstring>
#include <new>
#include "fake_arena_on_heap.h"
using namespace calllog;

// ---- what the unit's files need and the test does not look at
InterfaceWindow* gUIRootWindow = nullptr;
InterfaceWindow* gUIFocusWindow = nullptr;
InterfaceHelpText* gUIHelpText = nullptr;
uint32_t IveGotTicks() { return 0; }
void UI_SetCursor(int16_t) {}
int32_t UI_PostMessage(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_SendMessage(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_UpdatePointerIn() { return 1; }
int32_t InterfaceWindow::Show(int16_t) { return 0; }
InterfaceWindow* InterfaceWindow::GetChild(int32_t) { return mChild; }
InterfaceWindow* InterfaceWindow::GetNext() { return mNext; }
uint32_t InterfaceWindow::GetAttr() { return mAttributes; }

void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 1; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return mHelpText; }

InterfaceListControl2::InterfaceListControl2(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                             int16_t right, int16_t bottom)
    : InterfaceWindow(parent, attributes, id, left, top, right, bottom) {}
void InterfaceListControl2::CalcClientArea() {}
InterfaceListControl2::~InterfaceListControl2() {}
int32_t InterfaceListControl2::WhatAmI() { return kUIKindListControl; }
void InterfaceListControl2::ClientDraw() {}

bool CoolCircle::PointIn(int16_t, int16_t) { return false; }
void CoolCircle::Shift(int16_t, int16_t) {}
void CoolCircle::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolCircle::~CoolCircle() {}
bool CoolEllipse::PointIn(int16_t, int16_t) { return false; }
void CoolEllipse::Shift(int16_t, int16_t) {}
void CoolEllipse::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolEllipse::~CoolEllipse() {}
bool CoolPoly::PointIn(int16_t, int16_t) { return false; }
void CoolPoly::Shift(int16_t, int16_t) {}
void CoolPoly::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolPoly::~CoolPoly() {}

// ---- the world
namespace {
constexpr int kSlots = 12;
constexpr int kSpecWords = 13;   // parent, kind, attributes, z, rect (4), animation offsets (4), item slot
alignas(16) char gStore[kSlots][1024];
InterfaceWindow* gWindows[kSlots];
InterfaceWindow* gItemArrays[kSlots][2];   // [0] is what mItemWindows[0] holds
int32_t gBuilt = 0;

InterfaceWindow* Window(int32_t slot) { return slot >= 0 && slot < gBuilt ? gWindows[slot] : nullptr; }
}  // namespace

// Builds the forest. Per window: parent slot (-1 none), kind (0 plain, 1 list control), attributes, z, left, top, right, bottom,
// the four animation offsets, and for a list control the item slot: -2 no array, -1 an array whose first pointer is NULL, k the
// window k as the first item. Children are linked in the order of the specs.
extern "C" void tpw_hz_build(int32_t count, const int32_t* spec) {
    gBuilt = count;
    for (int i = 0; i < count; ++i) {
        const int32_t* w = spec + kSpecWords * i;
        std::memset(gStore[i], 0xa5, sizeof gStore[i]);
        InterfaceWindow* parent = nullptr;
        if (w[1] == 0) {
            gWindows[i] = new (gStore[i]) InterfaceWindow(parent, 0, i, static_cast<int16_t>(w[4]), static_cast<int16_t>(w[5]),
                                                          static_cast<int16_t>(w[6]), static_cast<int16_t>(w[7]));
        } else {
            gWindows[i] = new (gStore[i]) InterfaceListControl2(parent, 0, i, static_cast<int16_t>(w[4]), static_cast<int16_t>(w[5]),
                                                                static_cast<int16_t>(w[6]), static_cast<int16_t>(w[7]));
        }
        InterfaceWindow* window = gWindows[i];
        window->mAttributes = static_cast<uint32_t>(w[2]);
        window->mZ = static_cast<int16_t>(w[3]);
        for (int k = 0; k < 4; ++k) window->mAnimOffset[k] = static_cast<int16_t>(w[8 + k]);
        window->mNext = nullptr;
        window->mChild = nullptr;
        window->mPrevious = nullptr;
        window->mParent = nullptr;
    }
    for (int i = 0; i < count; ++i) {
        const int32_t* w = spec + kSpecWords * i;
        if (w[0] >= 0 && w[0] < count) {
            InterfaceWindow* parent = gWindows[w[0]];
            InterfaceWindow* window = gWindows[i];
            window->mParent = parent;
            InterfaceWindow** link = &parent->mChild;
            InterfaceWindow* last = nullptr;
            while (*link != nullptr) {
                last = *link;
                link = &(*link)->mNext;
            }
            *link = window;
            window->mPrevious = last;
        }
        if (w[1] == 1 && w[12] != -2) {
            gItemArrays[i][0] = w[12] >= 0 ? Window(w[12]) : nullptr;
            gItemArrays[i][1] = nullptr;
            static_cast<InterfaceListControl2*>(gWindows[i])->mItemWindows = gItemArrays[i];
        }
    }
}

extern "C" void tpw_hz_set_root(int32_t slot) { gUIRootWindow = Window(slot); }

// The three static words: answer, attribute, child's Z.
extern "C" void tpw_hz_set_words(int32_t answer, int32_t attribute, int32_t childZ) {
    gUIHighestZ = answer;
    gUIHighestZAttr = attribute;
    gUIHighestZChildZ = childZ;
}

extern "C" void tpw_hz_get_words(int32_t* out) {
    out[0] = gUIHighestZ;
    out[1] = gUIHighestZAttr;
    out[2] = gUIHighestZChildZ;
}

// highestZ(window, rect): rect is NULL when hasRect is 0.
extern "C" int32_t tpw_hz_highest(int32_t slot, int32_t hasRect, const int32_t* rect) {
    CoolRect probe(static_cast<int16_t>(rect[0]), static_cast<int16_t>(rect[1]), static_cast<int16_t>(rect[2]), static_cast<int16_t>(rect[3]));
    return highestZ(Window(slot), hasRect ? &probe : nullptr);
}

extern "C" int32_t tpw_hz_zofentries(int32_t slot) {
    return static_cast<InterfaceListControl2*>(Window(slot))->GetZOfEntries();
}
