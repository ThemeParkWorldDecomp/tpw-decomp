// InterfaceHelpText.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceHelpText code, named after it.

#include "UI_Base.h"

// The interface's help text handler (main app, UI toolkit 0x101cf874..0x101cfdb0, unit w36): the base class of the object that shows a
// window's help text (the tooltip), its default instance, UI_SetHelpTextHandler, and (it is in the same part of the toolkit) the
// constructor of the timers' table entries.
//
// The base class does nothing visible: it keeps the pointer position and the window the pointer is in, and tells itself to hide
// (HideHelpText, a virtual member that the game's own handler class overrides) when the pointer leaves, a button goes down or the
// window goes. Show, Hide, Update, OnFrame, Init and DeInit are empty. The default instance (gUIDefaultHelpText, src/UI_init.cpp) is
// made by the constructor, which leaves it ENABLED; before this unit the host's was zero-initialised, i.e. disabled.
//
// The Mac's object is 0x18 bytes: +0 enabled (short), +2 wanted (short), +4 and +6 the pointer position, +8 a short that the
// constructor clears and the base class only tests, +0xc the tick the pointer entered a window at, +0x10 the vtable pointer, +0x14 the
// window. The virtual members are called through the vtable (0x102afa88): the slot at 0xc, HideHelpText, from Disable, OnMouseMovedOut,
// OnMouseDown and OnHideOrDelete.

void InterfaceHelpText::ShowHelpText() { mWanted = 1; }   // 0x101cf874
void InterfaceHelpText::HideHelpText() { mWanted = 0; }   // 0x101cf8b8

// 0x101cf8fc (40 bytes). The position (+4, +6) is not set.
InterfaceHelpText::InterfaceHelpText() {
    mEnabled = 1;
    mWanted = 0;
    mWindow = nullptr;
    mMovedInTicks = 0;
    mShowing = 0;
}

// 0x101cf954 (76 bytes): puts the base class's vtable back and, for the deleting form, frees the object (the compiler's here).
InterfaceHelpText::~InterfaceHelpText() {}

// 0x101cf9d0. (The original then reads the wanted flag and compares it with 0, and does nothing with the result.)
void InterfaceHelpText::Enable() { mEnabled = 1; }

// 0x101cfa14: when the text is wanted it is hidden (the virtual HideHelpText).
void InterfaceHelpText::Disable() {
    mEnabled = 0;
    if (mWanted != 0) HideHelpText();
}

void InterfaceHelpText::Show() {}     // 0x101cfa88
void InterfaceHelpText::Hide() {}     // 0x101cfabc
void InterfaceHelpText::Update() {}   // 0x101cfaf0

// 0x101cfb24
void InterfaceHelpText::OnMouseMoved(int16_t x, int16_t y) {
    mMouseX = x;
    mMouseY = y;
}

// 0x101cfb68: the pointer entered a window. When the handler is enabled and the flag at +8 is set, the tick is kept.
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow* window) {
    mWindow = window;
    if (mEnabled != 0 && mShowing != 0) mMovedInTicks = IveGotTicks();
}

// 0x101cfc04 and 0x101cfc8c: the pointer left the window, or a button went down: the window is forgotten and the text hidden (when
// the handler is enabled and the flag at +8 is set).
void InterfaceHelpText::OnMouseMovedOut() {
    mWindow = nullptr;
    if (mEnabled != 0 && mShowing != 0) HideHelpText();
}

void InterfaceHelpText::OnMouseDown() {
    mWindow = nullptr;
    if (mEnabled != 0 && mShowing != 0) HideHelpText();
}

void InterfaceHelpText::OnFrame() {}   // 0x101cfd10
void InterfaceHelpText::Init() {}      // 0x101cfd48
void InterfaceHelpText::DeInit() {}    // 0x101cfd7c

// 0x101cfdb0: the window the pointer was in is hidden or deleted: forgotten, and the text hidden, whatever the handler's flags.
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow* window) {
    if (mWindow == window) {
        mWindow = nullptr;
        HideHelpText();
    }
}
