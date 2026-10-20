// InterfaceScrollBar (main app, UI toolkit 0x101d6280..0x101d72fc, DrawFunction and ClientDraw 0x1018a328/0x1018a364 in the
// ui_utils range): the interface's scroll bar, an InterfaceWindow with a value between two ends and a thumb. Mac 0x15c bytes (what
// InterfaceWindow::CreateObject allocates; the members below end at 0x14e), vtable 0x102a7d5c. src/InterfaceScrollBar.cpp
// (unit w30b) has all 18 functions below; the buttons and the thumb it makes through InterfaceWindow::CreateObject (unit w31b) are
// children with the ids 1 (the increment button), 2 (the decrement button) and 3 (the thumb, whose handler is
// UI_DefaultThumbtrackCallback).
//
// The bar is vertical or horizontal (mStyle). The client area is what is left of the rectangle after the two square buttons; the
// thumb moves along it. The value runs from the minimum to the maximum; for a vertical bar the maximum is at the top, for a
// horizontal bar at the right. A maximum below the minimum turns the step negative (a bar that counts the other way).
#pragma once
#include "InterfaceWindow.h"

class InterfaceScrollBar : public InterfaceWindow {
public:
    // `attributes` (besides the window's): 0x10 a vertical bar (else horizontal), 0x40 no buttons (the whole rectangle is the
    // client area), 0x80 makes the buttons (unless 0x40) and the thumb.
    InterfaceScrollBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                       int16_t bottom);                                  // 0x101d636c

    // ---- the virtual members this class replaces, in the vtable's order (0x102a7d5c); the others are InterfaceWindow's
    void DrawFunction() override;                  // 0x1018a328: nothing
    void CalcClientArea() override;                // 0x101d6280: the rectangle less a square at each end, or all of it
    ~InterfaceScrollBar() override;                // 0x101d64b8
    int32_t WhatAmI() override;                    // 0x101d6550: 3
    void ClientDraw() override;                    // 0x1018a364: nothing
    // A request for the state the window is in (the disabled bit, attribute 2) answers 0 and does nothing. Otherwise the window's own
    // Enable (not virtual here), then every child's Enable, then, when the global auto-redraw is on, a draw message posted to the
    // window. 1.
    int16_t Enable(int16_t on) override;           // 0x101d658c
    int32_t SetValue(int32_t value) override;      // 0x101d6bd4: ReCalcThumbPos(-1, value) (as a short: 1 when the value changed)
    int32_t GetValue() override;                   // 0x101d6b08

    // ---- the rest
    // Works the value out from where the thumb is (the opposite of ReCalcThumbPos) and posts the parent message 0x800 (the bar's
    // id, the value) when it changed. Needs the thumb child (id 3).
    void ReCalcValue();                            // 0x101d66a0
    // Moves the value and the thumb. `mode`: 0 the value less a step, 1 plus a step, 2 plus a page, 3 less a page, -1 `value`,
    // -2 the value as it is (the thumb is put in place and the draw objects told even if nothing changed). The new value is cut to
    // the ends. 0 when the value stayed (except for mode -2); else 1 after putting the thumb child (id 3) in its place (SetPosition),
    // telling both draw objects the position (change flag 8: the first word of the record, 0 to 0x400), the window's SetValue
    // (not this class's) and, when the value changed, sending the parent message 0x800 (the bar's id, the value). Needs the thumb child;
    // another mode leaves the new value as the caller's register had it in the original (here: the old value).
    int16_t ReCalcThumbPos(int32_t mode, int32_t value);   // 0x101d67dc
    // The ends. Nothing when both are as they are; the step's sign follows the order of the ends (negative when the maximum is
    // below the minimum). Does not move the value or the thumb.
    void SetMinMax(int32_t min, int32_t max);      // 0x101d6b44
    void SetPageValue(int32_t page);               // 0x101d6c34
    // Each makes its child (CreateObject kind 2, no attributes, id 1, 2 and 3: the increment button at the top or right end, the
    // decrement button at the other end, the thumb 0x40 long at the minimum end) and shows it; the thumb also gets
    // UI_DefaultThumbtrackCallback as its handler.
    void AddIncButton();                           // 0x101d6c74
    void AddDecButton();                           // 0x101d6d20
    void AddThumbTrack();                          // 0x101d6dcc
    // A click on the track (the point in the window's own coordinates): a page toward the point, judged against the thumb's far edge.
    void ClientClick(int16_t x, int16_t y);        // 0x101d6fc0

    // ---- data (the Mac offsets; the base window ends at 0x138)
    int32_t mMax;                                  // 0x138  0
    int32_t mMin;                                  // 0x13c  0
    int32_t mValue;                                // 0x140  0
    int32_t mStep;                                 // 0x144  1; negative when the maximum is below the minimum
    int32_t mPage;                                 // 0x148  1
    int16_t mStyle;                                // 0x14c  1 vertical, 2 horizontal (the constructor's attribute 0x10 or not), | 4 no buttons
};

// The scroll bar's message function (0x101d70ec, src/InterfaceScrollBar.cpp): 3 draws the client and the thumb (and answers 0
// without the default handler); -1 (when the global auto-redraw is on) posts the thumb a draw message and draws the client; the
// button messages 0x100, 0x10008 and 0x10009 with 2 or 1 in the first argument step the value down or up; 0x1000d (the wheel)
// subtracts its first argument from the value; 0x11006 (a click) is ClientClick with the point; and everything goes on to
// UI_DefaultWindowCallback.
int32_t UI_DefaultScrollBarCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

// The thumb's message function (0x101d7348, not translated).
int32_t UI_DefaultThumbtrackCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

// unit w37b: 0x101d6e9c. Shifts one axis, then clamps both ends in order.
void CalcThumbtrack(CoolRect* thumb, CoolRect track, int32_t dx, int32_t dy, int32_t horizontal);
