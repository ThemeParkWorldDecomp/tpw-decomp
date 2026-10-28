// InterfaceWindow (main app, 0x101d987c..0x101dff2c in the UI toolkit, plus a few small
// members the inventory files under ui_utils.cpp: 0x1018a270..0x1018aa0c): a window of the
// interface. A window is 0x138 bytes on the Mac; the host's is larger (native pointers) and
// has the vtable pointer first. The Mac offsets are in the comments.
//
// PARTIAL: the data members are complete as far as the constructor and the small members
// show them (the members after the gaps are named from how the accessors use them); the
// members translated so far are the constructor, PointIn and MakeRectAbsolute (src/
// InterfaceWindow.cpp, unit w21) and the five that other units call (below). The virtual
// members are declared in the order of the Mac's vtable (0x102a7d94); none is translated.
#pragma once
#include <cstdint>

#include "UI_Base.h"
#include "platform/arena.h"

class InterfaceWindow;
class InterfaceFont;
class InterfaceDrawObject;
// One frame of a window's animation (mAnim), 8 bytes on the Mac and on the host. Ordinarily the amounts added to the
// window's rectangle (left, top, right, bottom); when mLeft holds one of the four values below the frame is a command
// instead and mTop is its argument (doAnimFrame).
struct UI_RectDelta {
    int16_t mLeft;
    int16_t mTop;
    int16_t mRight;
    int16_t mBottom;
};
constexpr int16_t kAnimEnd = -9999;        // 0xd8f1: stops the animation and puts the window back where it began
constexpr int16_t kAnimZRot = -9998;       // 0xd8f2: mTop is the z rotation to set (setZRot)
constexpr int16_t kAnimGoto = -9997;       // 0xd8f3: mTop is the frame to go on at, at once
constexpr int16_t kAnimFrame = -9996;      // 0xd8f4: mTop is the frame the draw objects are set to
class InterfaceTimer;
// A window's message function.
using InterfaceWindowCallback = int32_t (*)(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

// 0x101de3f8 (src/game/ui/InterfaceWindow.cpp, 1,852 bytes): the handler every window starts with.
int32_t UI_DefaultWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

struct captureKeyList;   // UI_Base.h
class InterfaceWindow {
public:
    // 0x101d98d8. `parent` is NULL for the root window. `attributes` (the Mac's int): bit 1
    // asks for UI_UpdatePointerIn at the end, 0x400 makes the rectangle absolute (adds the
    // positions of all the parents), 0xf0000000 are kinds of window that PointIn answers
    // for, 2 is "disabled" (Enable). `id` is the Mac's long.
    InterfaceWindow(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                    int16_t bottom);

    // The windows (and every control, which derive from this class) come from the arena: the status panels keep theirs in the
    // Mac's 32-bit fields (UIStatusPanelFields, Ptr32), as the Mac kept every window's address in 32 bits. A window built in
    // memory from elsewhere (placement new over ::operator new, the list controls' row formats) is given back to where it came from.
    static void* operator new(size_t size) {
        void* block = tpw::ArenaAlloc(size);
        if (block == nullptr) throw std::bad_alloc();
        return block;
    }
    static void* operator new(size_t size, const std::nothrow_t&) noexcept { return tpw::ArenaAlloc(size); }
    static void* operator new(size_t, void* place) noexcept { return place; }
    static void operator delete(void* block) {
        if (tpw::InArena(block)) tpw::ArenaFree(block);
        else ::operator delete(block);
    }
    static void operator delete(void* block, const std::nothrow_t&) noexcept { operator delete(block); }
    static void operator delete(void*, void*) noexcept {}

    // ---- the virtual members, in vtable order
    virtual void DrawFunction();                      // 0x1018a270 (src/game/ui/InterfaceDrawEngine.cpp, unit w54): nothing
    virtual void CalcClientArea();                    // 0x101d987c (src/game/ui/InterfaceWindow.cpp): copies the rectangle to the client rectangle
    virtual ~InterfaceWindow();                       // 0x101d9ba4 (src/game/ui/InterfaceWindow.cpp, unit w51): see there
    virtual int32_t WhatAmI();                        // 0x101d9eb0 (src/game/ui/InterfaceWindow.cpp): 1 (kUIKindWindow, include/ui_controls.h)
    virtual void ClientDraw();                        // 0x1018a2a8
    virtual int16_t Enable(int16_t on);               // 0x101dc320 (same file): bit 2 of the attributes; 1 when it changed, then the handler hears 0x17
    virtual int32_t operator-(InterfaceWindow& other);   // 0x101dcf24 (same file): WhatAmI of this one and of the other (slot +0x14), whatever they answer; 0
    virtual int32_t SetValue(int32_t value);          // 0x101dfe84
    virtual int32_t GetValue();                       // 0x101dff2c (same file): 0
    virtual int32_t SetText(char16_t* text);          // 0x101de248 (src/game/ui/InterfaceWindow.cpp): the text for both draw objects (flag 0x100), the client one first; 1
    virtual char16_t* GetText();                      // 0x1018a9d4 (same file): NULL
    virtual int32_t GetHelpText();                    // 0x1018aa0c (same file): mHelpText

    // ---- translated
    // 0x101d9ee8 (src/game/ui/InterfaceWindow.cpp, unit w51): sends message 0x14 to the window and, depth first, to its descendants, marks
    // each as destroying and takes it out of the key capture, the keyboard window and the focus window.
    void PropagateDestruction();
    // 0x101dcebc (same file): clears the key capture when this window holds it; 1, or 0 when it did not.
    int32_t UnCaptureKeys();
    InterfaceWindow* PointIn(int16_t x, int16_t y);   // 0x101dc3e8: the deepest window under the point, or NULL
    void MakeRectAbsolute(CoolRect* rect);            // 0x101dab88: shifts it by the positions of all the parents

    // ---- the pump's members (src/game/ui/UI_Base.cpp, src/game/ui/InterfaceWindow.cpp)
    // Calls the window's handler (the function at +0x110) with the window and the three arguments and answers what it answers;
    // -1 when it has none.
    int32_t CallWindowHandler(int32_t message, int32_t a, int32_t b);   // 0x101de384
    int8_t AmIGettingDestroyed();                     // 0x101c9be8: the byte at +0 (UI_Base.cpp's range)
    InterfaceWindow* GetChild(int32_t unused);        // 0x101dbac4: the first child; the argument is never read
    InterfaceWindow* GetNext();                       // 0x101dbb38: the next sibling
    uint32_t GetAttr();                               // 0x101dbb70: the attributes

    // ---- other members the translated code calls
    void setCursor(int16_t cursor);                        // 0x101dd7f0
    void unlink();                                        // 0x101de084
    InterfaceWindow* Build(int16_t* description, InterfaceWindowCallback callback);   // 0x101deb78 (src/game/ui/InterfaceWindow.cpp, unit w31a): see ProcessScript
    uint8_t* GetClientObjectSurface(int32_t* width, int32_t* height); // 0x101dbc98 (src/game/ui/InterfaceWindow.cpp, unit v5L16): the client draw object's GetWritableDataArea
    void FinishWithClientObjectSurface();                          // 0x101dbd1c (src/game/ui/InterfaceWindow.cpp, unit v5L06): the client draw object's Refresh
    // Gives this window the keyboard (gUIKeyboardWindow): when another window had it, that one is posted message 0x20 (`on`, 0);
    // then this one is posted 0x1f (`on`, 0). Does nothing when it has it already. (The Mac answers 1 when it took the keyboard
    // and 0 when it had it; no caller reads that, so the declaration stays void.)
    void CaptureKb(int32_t on);                              // 0x101dccec (src/game/ui/InterfaceWindow.cpp)
    InterfaceWindow* GetWindow(int32_t id);                  // 0x101dbe40: the first child with that id, or NULL (src/game/ui/InterfaceWindow.cpp)
    // Sets the window's message function unless `callback` is NULL. (The Mac answers 1; no caller reads it.)
    void SetCallbackFunction(InterfaceWindowCallback callback);   // 0x101dcba8 (src/game/ui/InterfaceWindow.cpp)
    // 0x101dc750 (src/game/ui/InterfaceWindow.cpp, unit w31b): makes the control of this kind number (1 a plain window, 2 a button,
    // 3 a scroll bar, 4 a list box, 5 an edit box, 6 an image, 7 a list control, 8 check buttons, 9 a bar, 10 a text box, 11 a
    // knob, 12 a vary box, 13 a light panel) with this window as its parent, links it as the LAST of this window's children and
    // answers it. Any other kind number (0, 14 and up, negative) makes nothing and answers NULL.
    InterfaceWindow* CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                  int16_t bottom);

    // 0x101de318 (src/game/ui/InterfaceWindow.cpp, unit w36): calls the window's default handler (the function at +0x114) with the same
    // message and arguments and answers what it answers (no test for NULL).
    int32_t CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b);

    // ---- the positions, the drawing and the animation (src/game/ui/InterfaceWindow.cpp, unit w30)
    // Draws the window and all its visible descendants, each parent before its children: for a window whose attribute bit 1 is
    // set, DrawFunction and then message 3 unless one of the kinds in 0xf0000000 is set, then the children in order. 1.
    int32_t DrawMe();                                  // 0x101da564
    // The same walk the other way round: the children last to first, each window's message 3 and then DrawFunction. 1.
    int32_t DrawReverseOrder();                        // 0x101da794
    // Moves the window, its client rectangle, region and pivot, and all its descendants. 0 when both amounts are 0.
    int32_t ShiftPosition(int16_t dx, int16_t dy);     // 0x101da9d0
    // Tells the children the window's edges moved by these amounts; the children's attribute bits 0x10000 to 0x80000 say
    // which edges of theirs follow (left, right, top, bottom).
    void HandleScale(int16_t left, int16_t top, int16_t right, int16_t bottom);   // 0x101dac2c
    // Puts the window in `rect`: a shift when the size is the same, a resize (with HandleScale) otherwise. 1.
    int32_t SetPosition(CoolRect* rect);               // 0x101dae98
    // Writes the window's rectangle, less the animation's offsets when `subtract` is not 0, to `rect`. 1, or 0 for NULL.
    int32_t GetPosition(CoolRect* rect, int16_t subtract);   // 0x101db884
    // 0x101ddd34 (src/game/ui/InterfaceWindow.cpp, unit w91, 440 bytes): sets the pivot (mPivotX, mPivotY) the animations scale and rotate about,
    // in the window and in every descendant: each gets the pair and tells its draw object and then its client draw object (Change, flag
    // 0x40; none when it has none), the parent first, then each child with its subtree in turn. The pause window's message function passes
    // the halves of its right and bottom edges.
    void setPivot(int16_t x, int16_t y);
    // 0x101dd08c (src/game/ui/InterfaceWindow.cpp, unit w91, 284 bytes): starts the frames `anim` (UI_RectDelta, ended by a command frame) with
    // `ticks`: a running animation is undone first (the window goes back to its rectangle less the offsets, through SetPosition, and the
    // offsets are cleared), then frame 0 starts at the UI clock. NULL starts nothing. Answers whether `anim` was not NULL (the Mac's bool).
    int32_t startAnim(UI_RectDelta* anim, int32_t ticks);
    // 0x101dd6b8 (src/game/ui/InterfaceWindow.cpp, unit w91, 100 bytes): starts animation timer 0 to 3, given as the index or as the timer id
    // 0x80080 to 0x80083. No bounds check. Answers whether the timer exists.
    int32_t startAnimTimer(int32_t which);
    // 0x101dd5e0 (160 bytes, untranslated; unit w131 calls it from HappyBarMsg with (0x80083, 2000)): makes animation timer `id` (0x80080 to 0x80083) with
    // `ms` milliseconds; answers a short, nonzero when it was made.
    int16_t setAnimTimer(int32_t id, int32_t ms);
    // Writes the rectangle the window was made with (mOriginalRect, at +0x14 to +0x1a) to `rect` and answers 1; NULL: nothing is written and
    // the answer is 0. (Unit w56: MoveHelpText puts the tooltip window back where it began.)
    int32_t GetOriginalPosition(CoolRect* rect);       // 0x101db9d0
    // 0x101db284 (src/game/ui/InterfaceWindow.cpp, unit w118, 48 bytes): copies the four shorts of `rect` to mOriginalRect; a NULL rectangle writes
    // nothing. Answers 1 either way (GetOriginalPosition answers 0 for NULL).
    int32_t SetOriginalPosition(CoolRect* rect);       // 0x101db284
    // Moves the window to the end of its parent's list of children (the one drawn last). Answers the window that was last,
    // which is told message 0x19 (the window itself is told 0x18), or NULL when this one was last already.
    InterfaceWindow* BringToTop();                     // 0x101dc4e0
    // One step of the animation when its time has come; answers the frame it is at, or -1 when it did not step.
    int32_t doAnimFrame();                             // 0x101dd304
    // Sets the z rotation of the window and all its descendants and tells their draw objects.
    void setZRot(int16_t rotation);                    // 0x101ddb04
    // ---- the window's show and position leaves (src/game/ui/InterfaceWindow.cpp, unit w30b)
    // Shows (any nonzero value) or hides the window by attribute bit 1: nothing happens when it is in that state already. The draw
    // objects, the handler (message 0x11) and the children follow through ShowAllDRAWOBJS, except that showing a window under a
    // hidden parent changes only its own bit. Always ends in UI_UpdatePointerIn, and a window that was hidden is told to the help
    // text handler (OnHideOrDelete). 1.
    int32_t Show(int16_t on);                          // 0x101dc1ec
    // For this window and its subtree: tells the draw objects Show(shown) and the handler message 0x11 (shown, 0), but only for
    // windows whose attribute bit 1 is set (all of them when `shown` is 0; a hidden window hides its whole subtree from the walk).
    void ShowAllDRAWOBJS(int16_t shown);               // 0x101dbf88
    // Puts the window at (*x, *y), either may be NULL to keep that coordinate, with its size; then SetPosition(CoolRect*). 1.
    int32_t SetPosition(int16_t* x, int16_t* y);       // 0x101db1b0
    // The rectangle of these four coordinates (each cut to 16 bits), through SetPosition(CoolRect*). 1.
    int32_t SetPosition(int32_t left, int32_t top, int32_t right, int32_t bottom);   // 0x101db360

    // ---- the window script and the draw objects (src/game/ui/InterfaceWindow.cpp, unit w31a)
    // Runs the records of a window script (the format is described at ProcessScript's definition) from the word at gUIScriptPosition
    // on, making the windows they describe under this one, and answers the last window a record of this level made (NULL when none).
    // A record 0 makes a window with CreateObject and runs the records after it under the new window until a record 5 ends them;
    // `Build` starts the position at 0 and gives the first window made the callback.
    InterfaceWindow* ProcessScript(int16_t* script);   // 0x101dec14
    // Gives the window a draw object (replacing the one it has): the old one is told Show(0) and loses a user, and is deleted when
    // it is not in the library and has no user left; the new one is told its frame, depth (the window's z), z rotation, rectangle
    // (the window's), pivot and the font's id (Change, flags 0x57 and 0x10000 with a font) and Show(1) when the window is visible.
    void SetDrawObject(InterfaceDrawObject* object);          // 0x101db474
    // The same for the client draw object (+0x98) and the client rectangle, with the depth z + 1. QUIRK: what it deletes when the
    // old object has no user left is the window's (main) draw object, not the old client one, and the window keeps the pointer.
    void SetClientDrawObject(InterfaceDrawObject* object);    // 0x101db624
    // 1 when this window and all its parents have attribute bit 1, else 0.
    int16_t GetVisibility();                                  // 0x101dbd9c

    // ---- the leaves the controls' handlers call (src/game/ui/InterfaceWindow.cpp, unit w37a)
    // Takes the keyboard focus (gUIFocusWindow) when this window does not have it: the window that had it is posted message 0x1b, the
    // focus is set and this window is posted 0x1a. (UI_DefaultWindowCallback does the same inline for a press, message 0x10005.)
    void CheckFocus();                                       // 0x101dc6ac
    // Takes the pointer (gUICaptureDevice) when nobody has it: 1. Otherwise 0, whoever has it.
    int32_t CaptureDevice();                                 // 0x101dcc10
    // Gives the pointer back when this window has it (and works out the window the pointer is in again): 1. Otherwise 0.
    int32_t UnCaptureDevice();                               // 0x101dcc6c
    void UnCaptureKb(int32_t on);                            // 0x101dcdb0 (104 bytes; unit v5119): posts 0x20 (on, 0) to the keyboard window when it is this one, clears it; whether it was
    // Shows the window's cursor (UI_SetCursor with the window's cursor number, -1 for none).
    void cursorChange();                                     // 0x101dd860
    int32_t GetID();                                         // 0x101dbe08: the id the constructor was given
    InterfaceWindow* GetParent();                            // 0x101dbafc

    // ---- the members the island lobby's controller calls (unit w32)
    // 0x101db7dc (src/game/ui/InterfaceWindow.cpp, 120 bytes): sets the frame of the window's draw object and of its client draw
    // object (each only when there is one) to the number's low 16 bits.
    void SetFrame(int32_t frame);
    // The window's font and text setters (src/game/ui/InterfaceWindow.cpp, unit w39). Each one hands a DOChange to the window's draw object
    // (mDrawObject, +0x94) and then to its client draw object (+0x98), whichever it has: the flag says which field of the change the
    // draw object should read. The bytes the draw objects get are the low bytes of the arguments.
    // Keeps the font (nothing else happens when it is the one the window has) and tells the draw objects its id (the low byte of
    // mID, 0xff for no font): flag 0x10000.
    void SetFont(InterfaceFont* font);                       // 0x101da118
    // Tells the CLIENT draw object the text (flag 0x100); nothing when there is none.
    void SetClientDrawText(char16_t* text);                  // 0x101de130
    // 0x101de1bc (80 bytes, untranslated; unit w101 declares it): when the window has a draw object (mDrawObject), calls its virtual at +0x14 of
    // the table the object's word +0x38 points at, with 0x100 and the text; nothing otherwise.
    void SetWindowDrawText(char16_t* text);
    // Flag 0xc00: the horizontal and vertical justification (0 left or top, 1 centre, 2 right or bottom); nothing at all when the window
    // has neither draw object.
    void SetFontJust(int32_t horizontal, int32_t vertical);  // 0x101da204 (the two justification codes)
    // Flag 0x40000: the text colour (the low byte of each argument); nothing when the window has neither draw object.
    void SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha);   // 0x101da3ac
    // Keeps `value` as extra parameter number `index` and answers 1; answers 0 and keeps nothing for an index of 16 or more. The index is
    // a SIGNED short that is compared as a signed number, so a negative index writes before the array (the Mac's word at 0x9c + 4 *
    // index: -1 is the client draw object, -2 the draw object).
    int32_t setExtraParam(int16_t index, void* value);       // 0x101dcfd0

    // unit w43
    void* getExtraParam(int16_t index);   // 0x101dd030: signed index, NULL for index >= 16
    void SetFontWrap(int32_t horizontal, int32_t vertical);   // 0x101da2d8: flag 0x3000

    // ---- data
    uint8_t mDestroying;             // 0x00  nonzero once the destructor has begun (AmIGettingDestroyed): PointIn and UI_PostMessage skip such a window
    CoolRect mRect;                  // 0x04  the window's rectangle (absolute when attribute 0x400 is set)
    CoolRect mOriginalRect;          // 0x10  as the constructor was given it (made absolute) (Get/SetOriginalPosition)
    CoolRect mPreviousRect;          // 0x1c  (UpdatePreviousPosition, GetPreviousPosition)
    CoolRect mClientRect;            // 0x28  (CalcClientArea, Get/SetClientRect)
    CoolRect mRect34;                // 0x34  nothing reads it so far
    CoolRegion* mRegion;             // 0x40  what PointIn asks: &mRect at first (the destructor deletes any other)
    uint32_t mAttributes;            // 0x44  the constructor's `attributes` (GetAttr)
    InterfaceFont* mFont;            // 0x48  (SetFont)
    int16_t mCursor;                 // 0x4c  the UI_SetCursor number while the pointer is in this window; -1 none
    InterfaceWindow* mNext;          // 0x50  next sibling (GetNext)
    InterfaceWindow* mPrevious;      // 0x54  previous sibling
    InterfaceWindow* mChild;         // 0x58  first child (GetChild)
    InterfaceWindow* mParent;        // 0x5c  (GetParent)
    union {
        uint32_t mF60;               // 0x60  0
        InterfaceWindow* mOwner;     // 0x60  a pointer on the Mac (the host's is wider than the word): CreateObject stores the creating window
                                     //       here for a plain window made with attribute 0x100 (name guessed)
    };
    uint32_t mF64;                   // 0x64  0
    uint8_t mF68[0x88 - 0x68];       // 0x68  not set by the constructor
    int32_t mId;                     // 0x88  the constructor's `id` (GetID)
    int32_t mHelpText;               // 0x8c  -1 (GetHelpText)
    int32_t mF90;                    // 0x90  -1
    InterfaceDrawObject* mDrawObject;         // 0x94  (SetDrawObject)
    InterfaceDrawObject* mClientDrawObject;   // 0x98  (SetClientDrawObject, ClientDraw)
    void* mExtraParams[16];          // 0x9c  setExtraParam / getExtraParam (0x40 bytes on the Mac)
    int16_t mZ;                      // 0xdc  1 for the root, the parent's plus 2 otherwise, at most 0x100
    int16_t mZRot;                   // 0xde  0 (setZRot)
    int16_t mPivotX;                 // 0xe0  the middle of the rectangle (setPivot)
    int16_t mPivotY;                 // 0xe2
    UI_RectDelta* mAnim;             // 0xe4  the animation's frames (startAnim); NULL
    int16_t mAnimFrame;              // 0xe8  0
    int32_t mAnimTicks;              // 0xec  not set by the constructor (startAnim)
    int32_t mAnimStart;              // 0xf0  not set by the constructor
    int16_t mAnimOffset[4];          // 0xf4  left, top, right, bottom: 0 (GetPosition subtracts them)
    InterfaceTimer* mSlideTimer;     // 0xfc  NULL; the timer that paces the slide (message 0x10 with 0x80085)
    int16_t mSlideStep[4];           // 0x100 not set by the constructor: added to the rectangle each step (left, top, right, bottom)
    int16_t mSlideSteps;             // 0x108 not set by the constructor: steps left
    uint8_t mF10A[0x110 - 0x10a];    // 0x10a not set by the constructor
    InterfaceWindowCallback mCallback;          // 0x110  the window's message function: UI_DefaultWindowCallback at first
    InterfaceWindowCallback mDefaultCallback;   // 0x114  what CallDefaultWindowHandler calls: the same
    // (the Mac's vtable pointer is at 0x118)
    uint32_t mButtonsHeld;           // 0x11c  0: bit n set while mouse button n went down in this window and has not come up
    InterfaceTimer* mAnimTimers[4];  // 0x120  NULL (setAnimTimer makes them; the destructor removes any left: UI_RemoveTimer)
    uint32_t mF130;                  // 0x130  0
    uint8_t mF134;                   // 0x134  0
    uint8_t mAnimating;              // 0x135  0; startAnim sets it and stopAnim clears it
    int8_t mSliding;                 // 0x136  not set by the constructor: nonzero while the slide runs (message 0x10 with 0x80085)

    // unit w37b
    int32_t UpdatePreviousPosition();                // 0x101db2f8
    int32_t GetPreviousPosition(CoolRect* rect);      // 0x101db954
    // unit w37c
    void CheckClientMouse(int32_t message, int32_t button, int32_t point); // 0x101dc5d0
    // unit w93 (src/game/ui/InterfaceWindow.cpp)
    // Tells the draw object and the client draw object (whichever exist) the text offset (Change, flags 0xc000: x, then y, each cut to
    // 16 bits). The change record the original passes has only its text-style pointer set.
    void SetTextOffset(int32_t x, int32_t y);                 // 0x101da484
    // The size of the rectangle through *width and *height (either may be NULL); with a nonzero `adjust` (a short) less the size of
    // the animation offsets (right minus left, bottom minus top of mAnimOffset), each cut to 16 bits. 1.
    int32_t GetDims(int16_t* width, int16_t* height, int16_t adjust);   // 0x101dbeb0
    // unit w43b (src/game/ui/InterfaceWindow.cpp)
    // Sets the depth of the window (cut to 0x100 above, not below) and tells its draw object that depth and its client draw object one
    // more (Change, flag 2); then the same for the children with the argument plus 2, for their children with plus 4 and, through setZ
    // itself, for all deeper windows with plus 6 (so the children of a window three levels down start again from their parent's depth
    // plus 2). QUIRK: the children are given the argument as it came, not the depth the window kept after the cut.
    void setZ(int16_t z);                                   // 0x101dd8bc
    // Copies the client rectangle (mClientRect) to `rect` and answers 1; NULL: nothing is written and the answer is 0.
    int32_t GetClientRect(CoolRect* rect);                  // 0x101dba4c
    // unit v5L14: the other way, `rect` becomes the client rectangle (1); NULL: nothing changes and the answer is 0.
    int32_t SetClientRect(CoolRect* rect);                  // 0x101db3fc
    // unit v5076: 0x101ddf20 (292 bytes, stand-in): makes `child` a child of this window (the two flags are 1, 1 for the staff location panel)
    // (unit v5981: translated in src/game/ui/InterfaceWindow.cpp, src/game/ui/ui_islandLobbyController.cpp; answers 1, or 0 when `child` is NULL or already linked)
    void link(InterfaceWindow* child, char a, char b);
    // unit v5981: the same, answering what the Mac's code leaves in r3 (1, or 0); link calls it.
    int32_t linkChecked(InterfaceWindow* child, char a, char b);

    // unit v5119 (src/game/ui/InterfaceWindow.cpp)
    // 0x101dbba8 (164 bytes): writes the client draw object's second rectangle (+0x1c) to `rect`; (0, 0, 0, 0) when there is none.
    void GetSizeOfClientDrawObject(CoolRect* rect);
    // 0x101db0e8 (140 bytes): SetPosition of the window's rectangle adjusted by the four amounts.
    void AdjustPosition(int16_t left, int16_t top, int16_t right, int16_t bottom);
    // 0x101dd1e8 (236 bytes): when animating, clears the animation and puts the window back less the offsets (SetPosition), clears the offsets; the Mac's bool.
    bool stopAnim();

    // unit v5565: 0x101dd754 (100 bytes, stand-in): stops the animation timer with that id (0x80080, 0x80081 for the tags)
    // (unit v51111: translated; answers 1 when the timer exists, else 0, like startAnimTimer)
    int32_t stopAnimTimer(int32_t id);

    // unit v5955: 0x101dce4c (44 bytes, stand-in): the window asks for the keys of the list
    // (unit v51111: translated; 0 when the window holds the keyboard already, else 1 after taking the keys)
    int32_t CaptureKeys(captureKeyList* keys);
};

// The position in the window script (the word index), shared by the nested ProcessScript calls; Build sets it to 0.
extern int32_t gUIScriptPosition;   // 0x10395c5c, original name unknown

// Unit v5193 (src/game/ui/ui_worldLobbyController.cpp)
namespace bfl { namespace draw { struct CColour; } }
// The satellite panel's parent window (the word at 0x10396fec; NULL until the lobby makes it). Original name unknown.
extern InterfaceWindow* gUISatelliteParent;
// 0x1022b8d0 (312 bytes): with the parent set, takes its child 0x16007 and that one's child 0x16008 + which (which 0 to 4; any other
// number does nothing more) and calls the virtual at vtable (+0x118) offset 0x24 (SetValue, slot 7) on it with `value`.
void UpdateSatellite(int32_t which, int32_t value);

// Unit v5226 (src/game/ui/ui_worldLobbyController.cpp): the lobby's chat satellite and the world lobby window's messages.
struct ArgumentVector;
struct MslCharString;
class Argument;
extern int32_t gChatSatelliteFinished;       // 0x10396fb4, original names unknown: 1 once the answers are in or the connection broke
extern int32_t gChatSatelliteAnswers;        // 0x10396fb8, the answers counted
extern int32_t gChatSatelliteValues[5];      // 0x10396fbc
extern int32_t gChatSatelliteParkIDs[5];     // 0x10396fd0
extern void* gChatSatelliteChat;             // 0x10396fe4 (the Mac's Chat*)
extern int32_t gChatSatelliteWanted;         // 0x10396fe8
void ChatServerCallbackFn(int32_t id, void* unused, ArgumentVector& arguments);   // 0x1022a44c (440)
void ProcessGetChatSatellitePlayers();                                            // 0x1022a668 (776)
int32_t WorldLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1022aa20 (972)
void FindParkOnlineParkInfoPanelCancelCallback();   // 0x1022a9a8 (untranslated; the Mac passes the address of its TVector)
void CreateOnlineParkFindPanel(void (*onCancel)());   // 0x101e9ff8 (src/game/ui/ui_onlineParkFindPanel.cpp)
// Unit v5661: what CreateOnlineParkFindPanel and ShowFindListSearching need.
class InterfaceDrawObject;
class CFSOnlineSystem;
extern int16_t gOnlineParkFindScript[];       // 0x102b0c8c: the find panel's window script
extern const char16_t gOnlineParkFindWide[];   // 0x1024f01e: wide strings of the pool; the list cells use +0 and +2 (bytes)
InterfaceWindow* OnlineParkFindLCFormat();             // 0x101e96b0 (the find list's row maker; weak stand-in in the callees file)
void OnlineParkFindLCPostFormat(InterfaceWindow*);     // 0x101e9874 (the find list's attach function; weak stand-in)
// Callees the host cannot call as the Mac does (new plus constructor, the virtual at vtable +0x2c of the table at window +0x118):
// weak definitions in src/OnlineParkInfoPanelMsg_CreateOnlineParkFindPanel_callees.cpp, faked by the test library.
namespace findpanel_callee {
InterfaceDrawObject* NewCursorSprite();                           // operator new(0x19c), then 0x1018f1d0 (NULL)
CFSOnlineSystem* NewOnlineSystem();                               // operator new(0x404), then 0x1016f7d0
const char16_t* LoginCustomerName(CFSOnlineSystem* system);       // 0x10171834
void WindowSetText(InterfaceWindow* window, char16_t* text);      // the virtual at slot 0x2c of the table at +0x118
}  // namespace findpanel_callee
namespace chat_satellite_callee {
typedef void (*ChatCallback)(int32_t id, void* unused, ArgumentVector& arguments);
void ArgumentStrVal(MslCharString* result, const Argument* argument);   // 0x10217684 Argument::StrVal (the string by value)
int32_t ChatConnect(void* chat, const char* server, uint16_t port, ChatCallback callback, char* extra);   // 0x1023aa08
void ChatRoomInfo(void* chat, const MslCharString& room, void* context);   // 0x1023aa38
}  // namespace chat_satellite_callee
// 0x1022bb44 (800 bytes): the colour `percent` (cut to 0..100) of the way from `a` through `b` to `c`: below 50 from a to b, otherwise
// from b to c, in 1/65536 steps; red, green, blue blended, alpha 0xff, the cache words those of the start colour.
bfl::draw::CColour UI2DMReturnScaleColour(int32_t percent, const bfl::draw::CColour& a, const bfl::draw::CColour& b,
                                          const bfl::draw::CColour& c);

// Unit v5930 (src/game/ui/ui_onlineParkFindPanel.cpp): the find list's row maker and attach function (OnlineParkFindLCFormat and
// OnlineParkFindLCPostFormat, declared above) and the function that fills the list.
template<class T> class TbPtrList;
class CFSOnlinePark;
class CFSGlobalMasterSave;
class InterfaceFont;
extern uint8_t gOnlineParkFindRowInit;     // 0x1039609e: LCFormat's guard byte (the row height is worked out again after it is cleared)
extern int32_t gOnlineParkFindRowHeight;   // 0x103960a0: the row height LCFormat works out once (0 until then)
void FillOnlineParkFindList(TbPtrList<CFSOnlinePark>* parks);   // 0x101e9bf0 (748)
// Callees the host cannot call as the Mac does: weak definitions in src/OnlineParkFindPanelMsg_LCPostFormat_7_callees.cpp, faked by the test library.
namespace onlineparkfind_callee {
int16_t FontHeight(InterfaceFont* font);        // the virtual at vtable +0xc of the object at font +0x10 (through __ptr_glue), answers a short
CFSGlobalMasterSave* NewMasterSave();           // operator new(0x4c), then 0x101694dc
void WStrCtor(void* mem);                       // 0x10238740 TbDynamicStringTemplate<wchar_t>()
void WStrDtor(void* mem);                       // 0x10238710
void CStrCtor(void* mem, const char* text);     // 0x102382d8 TbDynamicStringTemplate<char>(const char*)
void CStrDtor(void* mem);                       // 0x102382f0
}  // namespace onlineparkfind_callee

// Unit v5948 (src/game/ui/ui_worldLobbyController.cpp): the world lobby window and its satellite panel.
// The world lobby window's script (0x102b20f8).
extern int16_t gWorldLobbyScript[];
// 0x1022b038 (1600): builds the world lobby window on the lobby's window (once; the parent word 0x10396fec says it exists), sets its
// extra parameter 0 to `publish`, and either hides the panel 0x16007 (publish) or shows it with five number windows 0x16008..0x1600c
// (callback SatelliteMsg, extra parameter 1..5); then the four arrow buttons 0x16002, 0x16004, 0x16005, 0x16001 get auto repeat (0, 10).
void CreateWorldLobbyController(bool publish);
// 0x1022ae30 (464): message 0x10006 enters the chat satellite number (extra parameter 0) minus one once the four lists are in; 0x1e asks
// for the satellite players; then the default handler answers.
int32_t SatelliteMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// Callee the host cannot call as the Mac does (operator new 0x19c plus InterfaceNumberTextObject 0x1018d930 with NULL): weak definition in
// src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_worldLobbyController.cpp, faked by the test library.
namespace worldlobby_callee {
InterfaceDrawObject* NewNumberText();
}  // namespace worldlobby_callee
