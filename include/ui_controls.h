// The interface's controls (main app, the Interface*.cpp files of the UI toolkit): the InterfaceWindow subclasses that
// InterfaceWindow::CreateObject (0x101dc750, src/ui_controls_create.cpp) makes by kind number. Each class has the layout
// of the Mac's object (the offsets are in the comments; the Mac's sizes are the operator new arguments in CreateObject) and
// declares the virtual members its vtable overrides, in the order of the base class's 12 slots (the Mac vtables, one per
// class, are at the addresses below). The host's objects are larger than the Mac's (native pointers, the vtable pointer
// first); the Mac offsets are comments only.
//
// PARTIAL: the constructors (and, for the button, the whole class) and every class's WhatAmI (src/ui_whatami.cpp) are translated;
// the other virtual members are declared with their addresses and untranslated. The class's first declared virtual member that is
// not inline is the key function of its vtable: the launcher defines it in launcher/pending_manual.cpp until its unit lands.
// PARTIAL: the constructors (and, for the button, the whole class) are translated, and so are the window handlers of the
// button, the check buttons, the knob, the text box and the vary box (src/ui_handlers_small.cpp, unit w37a) with the knob's and
// the vary box's own members; the other virtual members are declared with their addresses and untranslated. The class's first
// declared virtual member that is not inline is the key function of its vtable: the launcher defines it in
// launcher/pending_manual.cpp until its unit lands.
//
// The unknown words are named mFxxx by their Mac offset; the names of the others come from the small accessors and from the
// members of the other classes' neighbours that use them (guessed names say so).
#pragma once
#include <cstdint>

#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"

// unit w37b: native pointers for the list records built by the toolkit.
struct tagLBEntry;
class InterfaceDrawObject;

// The kinds of window: what WhatAmI answers (src/ui_whatami.cpp, src/InterfaceButton.cpp, src/InterfaceScrollBar.cpp) and the kind
// numbers InterfaceWindow::CreateObject and the window scripts' record 0 take; one number per class, the same table. The Mac's
// names are known for the five that its assertion strings give (UIOBJ_CREATE_SCROLLBAR, _LISTCONTROL, _KNOB, _VARYBOX and
// _LIGHTPANEL); the other names are invented.
enum UIObjectKind : int32_t {
    kUIKindWindow = 1,         // InterfaceWindow (0x101d9eb0)
    kUIKindButton = 2,         // InterfaceButton (0x101cb010)
    kUIKindScrollBar = 3,      // InterfaceScrollBar (0x101d6550), UIOBJ_CREATE_SCROLLBAR
    kUIKindListBox = 4,        // InterfaceListBox (0x101d0e78)
    kUIKindEditBox = 5,        // InterfaceEditBox (0x101cd7a8)
    kUIKindImage = 6,          // InterfaceImage (0x101cff50)
    kUIKindListControl = 7,    // InterfaceListControl2 (0x101d1918), UIOBJ_CREATE_LISTCONTROL
    kUIKindCheckButtons = 8,   // InterfaceCheckButtons (0x101cc0c8)
    kUIKindBar = 9,            // InterfaceBar (0x101c8848)
    kUIKindTextBox = 10,       // InterfaceTextBox (0x101d7d74)
    kUIKindKnob = 11,          // InterfaceKnob (0x101d0228), UIOBJ_CREATE_KNOB
    kUIKindVaryBox = 12,       // InterfaceVaryBox (0x101d8e0c), UIOBJ_CREATE_VARYBOX
    kUIKindLightPanel = 13,    // InterfaceLightPanel (0x101d0a14), UIOBJ_CREATE_LIGHTPANEL
};

// A colour as the button keeps it: four bytes, in the order of the Mac's __RGBA (passed by value in one register). The ctor's
// colours are white 0xffffffff for the up text and 0, 0, 0, 0xff for the down text, which fixes the alpha as the last byte.
struct UI_RGBA {
    uint8_t mR;
    uint8_t mG;
    uint8_t mB;
    uint8_t mA;
};

// ---- the controls' window handlers (the constructors store them as the window's message function). The button's, the check
// buttons', the knob's, the text box's and the vary box's are in src/ui_handlers_small.cpp (unit w37a); the others are untranslated.
// The button's: pointer in (0x10001), out (0x10002) and moved (0x10003), the left button down (0x10005) and up (0x10004), and the
// on-frame message (0x1e) for the auto-repeat. It answers 0 for the pointer messages and the left button's press and release,
// which it does not pass on, and what UI_DefaultWindowCallback answers for everything else (the on-frame message too).
int32_t UI_DefaultButtonCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);        // 0x101cb5f0 (2,316 bytes)
// The check buttons': the creation message (0x15) selects the first button of a group with attribute 0x10 and no selection; a
// button's message 0x100 (its id, its new state) selects or deselects it, tells the parent 0x101 (the id, the state), and goes on.
int32_t UI_DefaultCheckButtonsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);  // 0x101cc27c (984)
int32_t UI_DefaultEditBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101cf1ec (1,604)
// The knob's: the left button's press (0x10005, takes the focus) and release (0x10004) answer 0 without the default handler; a
// move (0x10003) turns the knob while the button is held.
int32_t UI_DefaultKnobCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);          // 0x101d07ac (256)
int32_t UI_DefaultListBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101d1210 (760)
int32_t UI_DefaultListControl2Callback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);  // 0x101d51f0 (1,576)
// The text box's does nothing of its own: the window's default handler.
int32_t UI_DefaultTextBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101d8134 (36)
// The vary box's: its two buttons (children with the ids 1 and 2) send 0x100 (a click), 0x10009 (held) and 0x10008 (repeating); the
// wheel (0x1000d) adds to the value; the creation message (0x15) sets the buttons' auto-repeat to 200 and 50.
int32_t UI_DefaultVaryBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101d9674 (452)

// ---- the bar. Mac 0x154 bytes, vtable 0x102a7c78.
class InterfaceBar : public InterfaceWindow {
public:
    InterfaceBar(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                 int16_t bottom);                                  // 0x101c86c4
    void DrawFunction() override;                                  // 0x1018a8c4 (untranslated)
    ~InterfaceBar() override;                                      // 0x101c87a4 (src/ui_controls_dtors.cpp, unit w54)
    int32_t WhatAmI() override;                                    // 0x101c8848 (src/ui_whatami.cpp)
    int32_t operator-(InterfaceWindow& other) override;            // 0x101c89a0 (untranslated)
    int32_t SetValue(int32_t value) override;                      // 0x101c887c (untranslated)
    int32_t GetValue() override;                                   // 0x101c8968 (untranslated): mValue

    int32_t mF138;       // 0x138  0
    int32_t mF13C;       // 0x13c  0
    int32_t mValue;      // 0x140  0 (GetValue)
    uint32_t mF144;      // 0x144  not set by the constructor
    CoolRect mBarRect;   // 0x148  made empty, then the client rectangle (which the constructor makes the window's rectangle)
};

// ---- the button: the menu of the lobby is made of these. Mac 0x15c bytes, vtable 0x102af974.
class InterfaceButton : public InterfaceWindow {
public:
    InterfaceButton(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                    int16_t bottom);                               // 0x101cadc0 (src/InterfaceButton.cpp)
    ~InterfaceButton() override;                                   // 0x101caea0: gives the text back to the string heap
    int32_t WhatAmI() override;                                    // 0x101cb010: 2
    int16_t Enable(int16_t on) override;                           // 0x101caf48
    int32_t SetText(char16_t* text) override;                      // 0x101cb3c4
    char16_t* GetText() override;                                  // 0x101cb440
    int32_t GetHelpText() override;                                // 0x101cbf40

    // Which picture of the draw objects the button shows: 1 disabled (attribute 2), else with the highlight 2 up or 3 down,
    // else by the state bits 4 (bit 1), 5 (bit 0) or 0.
    int16_t CalcFrame();                                           // 0x101cad20
    int8_t GetState();                                             // 0x101cb048
    // Tells the client draw object the text style of the frame: offsets and the up or down colour.
    void DoTextColour();                                           // 0x101cb084
    // Both answer 1 when the value changed, 0 when it was the same. `redraw` posts the window a draw message (1) when the
    // UI's auto redraw (gUIAutoRedraw) is on.
    int32_t SetState(int8_t state, int8_t redraw);                 // 0x101cb1d0
    int32_t HiLight(int8_t on, int8_t redraw);                     // 0x101cb2b8
    // 0 when the held-timer byte was 2, else 1; the byte is cleared either way.
    int32_t StopHeldTimer();                                       // 0x101cb478
    void SetAutoRepeat(int32_t delay, int32_t interval);           // 0x101cb4d4
    void SetTextUpColour(UI_RGBA colour);                          // 0x101cb518
    void SetTextDownColour(UI_RGBA colour);                        // 0x101cb584

    int8_t mState;            // 0x138  0: bit 0 down, bit 1 down and moved off (GetState)
    int8_t mHilight;          // 0x139  0: the pointer is over the button
    uint8_t mF13A[2];         // 0x13a  not set by the constructor
    int32_t mRepeatDelay;     // 0x13c  500 (SetAutoRepeat; names guessed from the values 500 and 125)
    int32_t mRepeatInterval;  // 0x140  125
    uint32_t mHeldTick;       // 0x144  not set by the constructor: the tick the auto-repeat last stepped at (the handler sets it)
    char16_t* mText;          // 0x148  NULL; a string of the string heap (GetText, the destructor releases it)
    UI_RGBA mTextUpColour;    // 0x14c  white
    uint32_t mF150;           // 0x150  not set by the constructor
    UI_RGBA mTextDownColour;  // 0x154  black
    int8_t mTextShiftX;       // 0x158  1: how far the text moves when the button is down (DoTextColour)
    int8_t mTextShiftY;       // 0x159  1
    uint8_t mHeldState;       // 0x15a  0 (StopHeldTimer: 2 is a held repeat); the handler sets 1 when an auto-repeat button goes down
    uint8_t mF15B;            // 0x15b  not set by the constructor
};

// ---- the check buttons. Mac 0x13c bytes, vtable 0x102af9c8.
class InterfaceCheckButtons : public InterfaceWindow {
public:
    InterfaceCheckButtons(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                          int16_t bottom);                         // 0x101cbf90
    ~InterfaceCheckButtons() override;                             // 0x101cc02c (src/InterfaceCheckButtons_dtor.cpp, unit w91): the base's
    int32_t WhatAmI() override;                                    // 0x101cc0c8 (src/ui_whatami.cpp)

    int32_t mCurrent;   // 0x138  -1 (GetCurrent at 0x101cc108)

    // unit w43b (src/InterfaceCheckButtons_current.cpp)
    // Makes the button with this id the current one: the old current button is set to state 0 and the new one to state 1 (SetState with
    // redraw 1), and the old id answered as a signed short. Answers -1 and changes nothing when the id is the current one or when no child
    // has it; -1 as the id clears the current one (the old button is set to 0) and answers -1 too.
    int32_t SetCurrent(int32_t id);                          // 0x101cc148

    // unit w47 (src/InterfaceCheckButtons_current.cpp): the current button's id (-1 for none); the word at 0x138.
    int32_t GetCurrent();                                    // 0x101cc108
};

// ---- the edit box. Mac 0x168 bytes, vtable 0x102a7cc4.
class InterfaceEditBox : public InterfaceWindow {
public:
    InterfaceEditBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                     int16_t bottom);                              // 0x101cd4c4
    void DrawFunction() override;                                  // 0x1018a850 (untranslated)
    void CalcClientArea() override;                                // 0x101cd468 (untranslated): the client rectangle is the rectangle
    ~InterfaceEditBox() override;                                  // 0x101cd65c (untranslated)
    int32_t WhatAmI() override;                                    // 0x101cd7a8 (src/ui_whatami.cpp)
    void ClientDraw() override;                                    // 0x1018a88c (untranslated)
    int32_t SetText(char16_t* text) override;                      // 0x101cda6c (untranslated)

    char16_t* mBuffer;     // 0x138  the text: 256 characters, made with new[] when attribute 0x40 is set, else NULL
    int32_t mMaxLength;    // 0x13c  0xff with a buffer, else 0 (name guessed)
    union { int32_t mF140; char16_t* mAcceptedChars = nullptr; }; // unit w37c, 0x140
    union { int32_t mF144; char16_t* mUnAcceptedChars = nullptr; }; // unit w37c, 0x144
    int16_t mF148;         // 0x148  0
    int16_t mF14A;         // 0x14a  -1
    int16_t mF14C;         // 0x14c  -1
    int16_t mF14E;         // 0x14e  0
    int16_t mF150;         // 0x150  0
    int16_t mF152;         // 0x152  0
    int16_t mF154;         // 0x154  0
    int16_t mF156;         // 0x156  0
    int16_t mF158;         // 0x158  0
    uint8_t mF15A[2];      // 0x15a  not set by the constructor
    union { int32_t mF15C; InterfaceDrawObject* mCursorDrawObject = nullptr; }; // unit w37c, 0x15c
    uint8_t mF160;         // 0x160  1
    uint8_t mF161;         // 0x161  0
    uint8_t mRightToLeft;  // 0x162  1 when the UI was started for a right to left language (gUIHebrew)
    uint8_t mF163;         // 0x163  not set by the constructor
    InterfaceTimer* mTimer;   // 0x164  UI_AddTimer(window, 750, 0x75e88): the cursor's blink (guessed)
    // unit w37c
    int32_t _SetText(char16_t* text, bool notify);                // 0x101cd858
    void SetCursorDrawObject(InterfaceDrawObject* object);         // 0x101cdac4
    void MoveCursor(int16_t position, int16_t show, int16_t selection); // 0x101cdd8c
    void SelectAll();                                            // 0x101ce8a4
    void SetUnAcceptedChars(char16_t* text);                      // 0x101ce94c
    void Edit(int16_t character);                                // 0x101ce990
    void Keydown(int16_t key, int32_t modifiers);                 // 0x101cebc4
    int32_t SetStaticTextArea(char16_t* text, int32_t capacity);   // 0x101cd7e0
    int32_t GetCurrentLen();                                     // 0x101cf138
    void LockOutMouse(bool lock);                                // 0x101cf1ac
};

// ---- the image. Mac 0x140 bytes, vtable 0x102afaec.
class InterfaceImage : public InterfaceWindow {
public:
    InterfaceImage(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                   int16_t bottom);                                // 0x101cfe3c
    ~InterfaceImage() override;                                    // 0x101cfebc (src/ui_image_dtor.cpp)
    int32_t WhatAmI() override;                                    // 0x101cff50 (src/ui_whatami.cpp)
    int32_t SetValue(int32_t value) override;                      // 0x101cff88 (untranslated)
    int32_t GetValue() override;                                   // 0x101d0054 (untranslated): mValue

    uint32_t mF138;   // 0x138  not set by the constructor
    int32_t mValue;   // 0x13c  not set by the constructor
};

// ---- the knob. Mac 0x168 bytes, vtable 0x102afb40. The names come from SetMinMaxAngles and ChangeValue (untranslated):
// the value runs from mMinValue to mMaxValue and is shown as an angle from mMinAngle to mMaxAngle.
class InterfaceKnob : public InterfaceWindow {
public:
    InterfaceKnob(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                  int16_t bottom);                                 // 0x101d008c
    ~InterfaceKnob() override;                                     // 0x101d0194 (untranslated)
    int32_t WhatAmI() override;                                    // 0x101d0228 (src/ui_whatami.cpp): 0xb
    int32_t operator-(InterfaceWindow& other) override;            // 0x101d038c (untranslated)
    int32_t SetValue(int32_t value) override;                      // 0x101d02e8 (untranslated)
    int32_t GetValue() override;                                   // 0x101d0354 (untranslated): mValue
    // Sets the smallest and the largest angle from the words the pointers point at (a NULL pointer leaves that one). When the
    // smallest is then above the largest, both become the largest. The range is the largest less the smallest.
    void SetMinMaxAngles(int32_t* minimum, int32_t* maximum);      // 0x101d0260 (src/ui_controls_small.cpp, unit w31a)
    // Turns the knob to this drag angle (cut to the angle range): when it differs from mDragAngle the value follows (the angle's share
    // of the value range, the ends mapped to the ends), and when that changes the value too, the displayed angle is worked out from the
    // value (the client draw object is told it, change flag 4) and the parent is posted 0x800 (the knob's id, the value).
    void ChangeAngle(int32_t angle);                               // 0x101d0574 (src/InterfaceKnob.cpp, unit w37a)
    // A pointer move while the knob is held (mDragging): when the pointer's y (the low 16 bits of `y`) differs from mLastY, the knob
    // turns by 8 angle units for each unit the pointer went up (ChangeAngle).
    void DoKnobMove(int32_t y);                                    // 0x101d0718 (src/InterfaceKnob.cpp, unit w37a)
    // Unit v5264. Sets the value (cut to the value range, the largest first); when it differs from mValue the old one is kept, the
    // displayed angle is worked out from the value (the ends mapped to the ends), the client draw object (if any) is told it (change
    // flag 4) and the parent hears 0x800 with the knob's id and the value.
    void ChangeValue(int32_t value);                               // 0x101d0448 (src/InterfaceKnob_InterfaceKnob_ChangeValue.cpp)

    int32_t mMinAngle;     // 0x138  0
    int32_t mMaxAngle;     // 0x13c  0xfff
    int32_t mAngleRange;   // 0x140  mMaxAngle - mMinAngle
    int32_t mDragAngle;    // 0x144  0: the angle the pointer has turned the knob to (ChangeAngle); mAngle follows from the value
    int32_t mAngle;        // 0x148  0
    int32_t mMinValue;     // 0x14c  0
    int32_t mMaxValue;     // 0x150  1
    int32_t mValueRange;   // 0x154  mMaxValue - mMinValue
    int32_t mValue;        // 0x158  0
    int32_t mOldValue;     // 0x15c  0
    int16_t mLastY;        // 0x160  not set by the constructor: the pointer's y at the last move (the press sets it)
    int16_t mPreviousY;    // 0x162  not set by the constructor: the one before
    int16_t mDragging;     // 0x164  0: 1 from the left button's press in the knob to its release
    uint8_t mF166[2];      // 0x166  padding
};

// ---- the light panel. Mac 0x13c bytes, vtable 0x102afb94.
class InterfaceLightPanel : public InterfaceWindow {
public:
    InterfaceLightPanel(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                        int16_t bottom);                           // 0x101d08f0
    ~InterfaceLightPanel() override;                               // 0x101d097c (untranslated)
    int32_t WhatAmI() override;                                    // 0x101d0a14 (src/ui_whatami.cpp)
    int32_t SetValue(int32_t value) override;                      // 0x101d0a50 (untranslated)
    int32_t GetValue() override;                                   // 0x101d0b5c (untranslated): mValue

    int32_t mValue;   // 0x138  0
};

// ---- the list box. Mac 0x158 bytes, vtable 0x102a7d10. With attribute 0x1000 it makes a scroll bar child.
class InterfaceListBox : public InterfaceWindow {
public:
    InterfaceListBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                     int16_t bottom);                              // 0x101d0c50
    void DrawFunction() override;                                  // 0x1018a3a0 (untranslated)
    void CalcClientArea() override;                                // 0x101d0b98 (untranslated): the client rectangle less 0x30 on the right, and the list rectangle
    ~InterfaceListBox() override;                                  // 0x101d0dc4 (untranslated)
    int32_t WhatAmI() override;                                    // 0x101d0e78 (src/ui_whatami.cpp)
    void ClientDraw() override;                                    // 0x1018a3dc (untranslated)
    void SetListRect(int16_t left, int16_t top, int16_t right, int16_t bottom);   // 0x101d11b0 (src/ui_controls_small.cpp, unit w31a)

    CoolRect mListRect;   // 0x138  the part the entries are drawn in (CalcClientArea; SetListRect)
    union {
        int32_t mF144;    // 0x144  0 (legacy word view)
        tagLBEntry* mEntries = nullptr; // unit w37b: root of the entry tree
    };
    int32_t mF148;        // 0x148  -1
    int32_t mF14C;        // 0x14c  0
    int32_t mF150;        // 0x150  0
    int16_t mF154;        // 0x154  0
    int16_t mF156;        // 0x156  0

    // unit w37b
    tagLBEntry* GetIndexedEntry(int32_t index);       // 0x101d0eb0
    void RemoveAllItems();                           // 0x101d10c8
};

// ---- the list control's items (unit w93)
// The data type of a column (SetColumnType, GetColumnType): the Mac stores it in the 16-bit word at +10 of the column's 0xc-byte
// rectangle record, which is the bottom of the CoolRect (SetColumns and SetColumnPos write 1 there, so a fresh column is a number).
enum LCDataType : int32_t {
    kLCText = 0,     // the cell holds a string (a new std::basic_string<wchar_t>)
    kLCNumber = 1,   // the cell holds a number (a long)
};

// std::basic_string<wchar_t> of the Metrowerks library (16-bit characters): one pointer to the shared representation. The
// string library is not translated: the constructor and assign are the instantiations at 0x10146a14 and 0x101b2510, standing in
// as recording boundaries in the test and as interim definitions in launcher/pending_manual.cpp.
struct LCStringRep {
    int32_t mLength;         // +0x00  the characters in use
    int32_t mCapacity;       // +0x04  3 for a new empty string
    int32_t mRefCount;       // +0x08  1
    char16_t* mData;         // +0x0c  the characters (a pointer on the Mac)
};
struct LCString {
    LCStringRep* mRep;       // +0x00
    explicit LCString(const char16_t* text);                       // 0x10146a14 (untranslated; the allocator argument is dropped)
    void assign(const char16_t* text, uint32_t length);            // 0x101b2510 (untranslated)
};

// One word of the item block and of an item to add. A word is 4 bytes on the Mac; the host's is 8 so that a string cell can hold
// a native pointer. The block is cleared through mRaw, the links and numbers live in mInt (the low half).
union LCWord {
    int64_t mRaw;
    int32_t mInt;            // a link, the entry's data word, or a number cell
    const char16_t* mText;   // a string cell of an item to add (UI_LCAdd): the text
    LCString* mString;       // a string cell of the block: the string made from that text
};
typedef LCWord UI_LCAdd;          // an item to add: one word per column (text pointer or number)
typedef LCWord tagLCDataEntry;    // the cells of one entry of the block: one word per column

// The words of an entry in the item block (entries are mColumnCount + 4 words apart; an entry is named by the index of its first
// word). The entries are chained through kLCNext in display order, used ones first and free ones after them; kLCChild and
// kLCParent make the hierarchy (the indentation of UpdateDisplay).
enum LCEntryWord : int32_t {
    kLCNext = 0,     // the next entry in the chain, -1 at the end
    kLCData = 1,     // the long the caller gave AddItem (GetData)
    kLCChild = 2,    // the first child, -1 for none
    kLCParent = 3,   // the parent, -1 for none, -2 for a free entry
    kLCCells = 4,    // the first cell; there are mColumnCount of them
};

// ---- the list control. Mac 0x178 bytes, vtable 0x102afbfc. With attribute 0x1000 it makes a scroll bar child.
class InterfaceListControl2 : public InterfaceWindow {
public:
    InterfaceListControl2(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                          int16_t bottom);                         // 0x101d15cc
    void CalcClientArea() override;                                // 0x101d154c (src/InterfaceListControl2_draw.cpp): the list rectangle is the client rectangle
    ~InterfaceListControl2() override;                             // 0x101d1768 (src/InterfaceListControl2_draw.cpp)
    int32_t WhatAmI() override;                                    // 0x101d1918 (src/ui_whatami.cpp)
    void ClientDraw() override;                                    // 0x1018a934 (src/InterfaceListControl2_draw.cpp)
    void SetListRect(int16_t left, int16_t top, int16_t right, int16_t bottom);   // 0x101d47f0 (src/ui_controls_small.cpp, unit w31a)
    // Makes `count` columns when there are none (mColumnCount 0): the rectangle of column i starts as 32 i, 0, 32 (i + 1), 1.
    // A count of 0 makes the (empty) array and leaves mColumnCount at 0, so the next call makes another.
    void SetColumns(int16_t count);                                               // 0x101d2488 (src/ui_controls_small.cpp, unit w31a)
    // Puts column `column` at left and right (top 0, bottom 1) when the column is below mColumnCount and the columns exist; a
    // negative column is not tested (the original writes before the array).
    void SetColumnPos(int16_t column, int16_t left, int16_t right);               // 0x101d2584 (src/ui_controls_small.cpp, unit w31a)
    // 0x101d518c (src/ui_controls_small.cpp, unit w50): the Z of the window the first item pointer names (mItemWindows[0]); 0 when there is
    // no array or the pointer is NULL. highestZ uses it as the list's own Z.
    int16_t GetZOfEntries();

    // ---- unit w93: the item block (src/InterfaceListControl2_items.cpp)
    // Takes `object` as the selection bar's draw object (the old one loses a user, is told Show(0) and is deleted when it was the last
    // one), sets it to one user, places it over the first row (Change flags 0x17) and shows it when the list is visible and has a
    // current entry.
    void SetSelectionObject(InterfaceDrawObject* object);                         // 0x101d1eb4
    // Adds `count` entries to the block (a new one, the old words copied, the first new entry linked after the last used one).
    // Answers the new block, NULL for a count of 0 or no columns.
    void* AllocNewBlock(int32_t count);                                           // 0x101d207c
    // Gives every visible row's window (and the cells under it) the text or number of the entry shown there, hides the rows past the end.
    void UpdateDisplay();                                                         // 0x101d2220
    void SetColumnType(int16_t column, LCDataType type);                          // 0x101d2614: the low half of `type`; nothing out of range
    int32_t GetColumnType(int16_t column);                                        // 0x101d268c: not range checked
    // Makes `window` row `row` (when it, the arrays and the columns exist and the row is below mVisibleRows): puts it and the windows
    // under it (its child, then that child's siblings) in the rows' columns and sets their Z.
    void SetColumnFormat(int32_t row, InterfaceWindow* window);                   // 0x101d26e0
    // Takes the row height from the first window `makeRow` answers, works out how many rows fit, makes the rest, hands each to
    // SetColumnFormat (last row first, row 0 with the first window) and to `attach`, and updates the display.
    void SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*));   // 0x101d2864
    // The entry after `index` in display order (child, else next, else the next of the nearest ancestor that has one; -2 when
    // there is none and the entry has no parent), and in *change how many levels deeper it is (1, 0, or minus the levels climbed).
    int32_t AdvanceIndex(int32_t index, int32_t* change);                         // 0x101d2a84
    // The entry shown at `position`: mFirstEntry for a position out of range (or no block), -1 past the end of the chain.
    int32_t GetIndexedEntry(int32_t position);                                    // 0x101d2b64
    // The first free entry (-1 when the chain has none) and the used one before it (-1 when the first entry is free).
    void FindFreeEntry(int32_t& freeEntry, int32_t& previous);                    // 0x101d2d6c
    // Whether the new item sorts before the entry whose cells are `entry`, by the sort column and direction.
    int32_t ShouldIInsertEntryHere(UI_LCAdd* item, tagLCDataEntry* entry);        // 0x101d2e8c
    // Adds an item (one word per column; strings are copied into new strings) with `data`, as the first child of the entry at
    // `position` (a position out of range, or -1, means the end). 0, or -1 for a NULL item. A list without attribute 0x10 never
    // links an entry added to a non-empty chain (QUIRK, as the original).
    int32_t AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update);   // 0x101d3000
    // Replaces the cells of the entry at `position` named by the bits of `columns`.
    void ChangeItem(int32_t position, UI_LCAdd* item, int32_t columns, int32_t update, int32_t unused);   // 0x101d3660
    // The data word of the entry at `position`: 1 and *data (when given), or 0 when there is no such entry.
    int32_t GetData(int32_t position, int32_t* data);                             // 0x101d41f0
    // Unit v5239: 0x101d3fcc (unit v5955 translated it): puts `data` in the data word of the entry at `position`; true, or false when there is none.
    void SetData(int32_t position, int32_t data);
    // Makes row `row` (0 is the first visible one) the current entry, -1 for a negative row (and for one past the visible rows, only
    // when attribute 0x100 is set; otherwise nothing happens); tells the parent (0x401) and moves the selection bar.
    void ChangeCurEntry(int32_t row);                                             // 0x101d4854

    CoolRect mListRect;   // 0x138  the part the entries are drawn in
    union {
        int32_t mF144;    // 0x144  0 (legacy word view)
        uint8_t* mItemStorage = nullptr; // unit w37b: new[] storage (mBlockWords LCWords, see Words)
    };
    union {
        int32_t mF148;    // 0x148  0 (legacy word view)
        InterfaceWindow** mItemWindows = nullptr; // unit w37b: new[] window pointers
    };
    int32_t mCurrentEntry; // 0x14c  -1: the current entry (an entry index), -1 for none
    int32_t mItemCount;    // 0x150  0: the entries in use
    int32_t mTopEntry;     // 0x154  0: the position of the first visible row (what the scroll bar's value is)
    int32_t mBlockWords;   // 0x158  0: the words in the item block
    uint32_t mF15C;        // 0x15c  not set by the constructor, not used by the item functions
    int32_t mFirstEntry;   // 0x160  -1: the first entry of the chain
    int16_t mColumnCount;  // 0x164  0: the number of columns (SetColumns)
    int16_t mF166;         // 0x166  0
    int16_t mSortColumn;   // 0x168  0: the column ShouldIInsertEntryHere compares
    int16_t mSortDescending; // 0x16a  0: nonzero to sort the other way
    CoolRect* mColumns;   // 0x16c  0: mColumnCount rectangles, an array made with new[] (a pointer on the Mac; the host's is wider
                          //        than the word, so the members after it are not at the Mac offsets). The Mac's are 0xc bytes
                          //        each; the bottom of a record doubles as the column's data type.
    int16_t mRowHeight;   // 0x170  0: from the first row window (SetColumnFormatsAll)
    int16_t mVisibleRows; // 0x172  0: how many rows fit in the list rectangle
    union {
        int32_t mF174;    // 0x174  0 (legacy word view)
        InterfaceDrawObject* mItemDrawObject = nullptr; // unit w37b: shared item draw object (the selection bar)
    };

    // unit w37b
    void RemoveAllItems();                           // 0x101d3cc8, untranslated
    // Unit v5660: copies the cells of the entry at `position` (the cells named by `column`) into `out` (stand-in).
    // (unit v5955 translated it: true when the entry and the column exist, false otherwise)
    void GetItem(int32_t position, int32_t column, UI_LCAdd& out);   // 0x101d40bc

    // Unit v5002 (untranslated callees of the all-features panel)
    int32_t FindData(int32_t data);                  // 0x101d4384 (252 bytes): the position of the entry with that data word, negative for none
    void SetSort(int32_t column, bool descending);   // 0x101d4db8 (436 bytes)
    void SetCurrent(int32_t position);               // 0x101d4c64 (284 bytes)

    // Unit v5095 (src/InterfaceListControl2_items_InterfaceListControl2_RemoveAllItems.cpp; RemoveAllItems above is translated there too)
    // Takes the entry at `position` out of the list; 1, or 0 for a position out of range (or no block).
    int32_t RemoveItem(int32_t position);            // 0x101d39f0 (672 bytes)
    // Frees entry `entry` (and its children, and with `removeNext` its following siblings) into the free chain.
    void RemoveMe(int32_t position, int32_t entry, int32_t previous, int32_t removeNext);   // 0x101d3818 (412 bytes)

    // The item block as words.
    LCWord* Words() const { return reinterpret_cast<LCWord*>(mItemStorage); }

    // Unit v5203 (src/InterfaceListControl2_items_InterfaceListControl2_Sort.cpp)
    // Sorts the chain by column mSortColumn (the argument is not used); mSortDescending 0 or 1, anything else moves nothing.
    void Sort(int32_t unused);                                   // 0x101d1958 (1320 bytes)
    // The position of the first entry whose text cell in column `column` equals `text`, -1 for none.
    int32_t FindString(char16_t* text, int32_t column);          // 0x101d44b8 (348 bytes)
    // The entry at `position` ignoring the hierarchy (untranslated stand-in; the second argument is -1 in Sort).
    int32_t GetNonHierIndexedEntry(int32_t position, int32_t parent);   // 0x101d2c30

    // Unit v5908 (src/InterfaceListControl2_items_InterfaceListControl2_SetSort.cpp; FindData, SetCurrent and SetSort above are translated there too)
    // Sends the message to every cell of the visible rows (message 0x1e: row << 16 | column, and the entry's data word).
    void PropagateMessage(int32_t message, int32_t a, int32_t b);   // 0x101d4650 (352 bytes)
    // A click at (x, y): makes the row under it current, posts the column (0x404) and, with attribute 0x80, the current entry (0x400).
    void SelectClick(int16_t x, int16_t y);                         // 0x101d4a50 (472 bytes)
    // A click on the column button `id` (column id - 0x10): changes the sort column or direction, sorts and updates.
    void ColumnButtonClick(int32_t id);                             // 0x101d4fa4 (424 bytes)

    // Unit v5955 (src/InterfaceListControl2_items_InterfaceListControl2_GetColumnPos.cpp)
    // Column `column`'s left and right edges through the two pointers (neither may be NULL); false when there are no columns, a pointer
    // is NULL or the column is not below mColumnCount (a negative column is not tested).
    bool GetColumnPos(int32_t column, int16_t* left, int16_t* right);   // 0x101d42ec
};
// Unit v5955: the keys UI_DefaultListControl2Callback asks CaptureKeys for (0x102afbcc; the Mac's first word is 0x23000000; the rest is a guess).
extern captureKeyList gListControl2Keys[];

// Unit v5074 (src/FillUpgradeBox.cpp): the ride status panel's upgrade list.
class CObject;
#include "ui_main.h"   // RideStatusPanel (Upgrade, UpdateStatusWindow, Remove, UpgradePanelByeBye) lives there
extern RideStatusPanel gRideStatusPanel;        // 0x103959a4, original name unknown
void FillUpgradeBox(int32_t kind, InterfaceListControl2* list, int32_t clear, CObject* object);   // 0x101ba128
int32_t UpgradePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);          // 0x101ba4e0
// 0x101b9fb8: the LCPostFormat of ui_onlineParkListPanel.cpp (the staff room's, src/LCPostFormat.cpp, has the same name at 0x101c346c): gives the row a text
// draw object and the font, and its first child a cash text object, the font and right/middle justification.
// Lead 2026-10-20 (v5914): was OnlineParkListLCPostFormat, the name src/PublishPanelMsg_LCFormat.cpp gives 0x101ebef4; both in one link = duplicate symbol.
void OnlineParkListPanelLCPostFormat(InterfaceWindow* window);
extern uint32_t gParkListFontIndex;   // 0x102ad5dc (name invented): the font table index of that list, 7 at start
// MSL's wcstok (an import, glue 0x1023a078); the original passes a stale third register, so the state argument is never meaningful.
char16_t* wcstok(char16_t* string, const char16_t* delimiters, char16_t** state);

// ---- the text box. Mac 0x150 bytes, vtable 0x102afd14.
class InterfaceTextBox : public InterfaceWindow {
public:
    InterfaceTextBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                     int16_t bottom);                              // 0x101d7bf0
    void DrawFunction() override;                                  // 0x101d80f8 (untranslated)
    ~InterfaceTextBox() override;                                  // 0x101d7cc8 (src/ui_controls_dtors.cpp, unit w54): frees the text block
    int32_t WhatAmI() override;                                    // 0x101d7d74 (src/ui_whatami.cpp): 0xa
    void ClientDraw() override;                                    // 0x1018a8fc (untranslated)
    int32_t operator-(InterfaceWindow& other) override;            // 0x101d8034 (untranslated)
    int32_t SetText(char16_t* text) override;                      // 0x101d7f50 (untranslated)

    char16_t* mText;        // 0x138  the text, a new[] block of mTextLength + 1 characters (SetText, CalcTextPos)
    int32_t mHorizontal;    // 0x13c  0: how CalcTextPos places the text across (1 and 2 are the right and the middle)
    int32_t mVertical;      // 0x140  0: and down
    int32_t mF144;          // 0x144  0
    int32_t mTextLength;    // 0x148  0: the size of the block in characters
    int16_t mTextX;         // 0x14c  0: where CalcTextPos puts the text
    int16_t mTextY;         // 0x14e  0
    // Unit v5975 (src/ui_whatami_InterfaceTextBox_CalcTextPos_2.cpp): 0x101d7dac (368): places the text in the client rectangle by
    // mHorizontal / mVertical (1 right or bottom, 2 middle, else left or top) using the font's width and height; nothing without a font or text.
    void CalcTextPos();
};
// Unit v5975: the runtime's operator new[] (0x10238668) and operator delete (0x10238218) as SetText calls them (weak defaults in
// src/ui_whatami_InterfaceTextBox_CalcTextPos_2.cpp; recording fakes in the test library).
namespace textbox_callee {
void* NewArray(uint32_t size);
void Delete(void* block);
}  // namespace textbox_callee

// ---- the vary box. Mac 0x158 bytes, vtable 0x102afd68. The names come from SetMinMaxValues, SetInc and ChangeValue
// (untranslated).
class InterfaceVaryBox : public InterfaceWindow {
public:
    InterfaceVaryBox(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                     int16_t bottom);                              // 0x101d8c94
    ~InterfaceVaryBox() override;                                  // 0x101d8d74 (untranslated)
    int32_t WhatAmI() override;                                    // 0x101d8e0c (src/ui_whatami.cpp)
    int32_t operator-(InterfaceWindow& other) override;            // 0x101d8fc0 (untranslated)
    int32_t SetValue(int32_t value) override;                      // 0x101d8f2c (untranslated)
    int32_t GetValue() override;                                   // 0x101d8f84 (untranslated): mValue

    // Handles one of the two buttons' messages (the handler maps them): 0 and 1 take one increment down and up, 2 and 3 are the start of
    // a hold (3 also takes one increment up), 4 and 5 a repeat step down and up. Any other number does nothing. A new value is cut to the
    // ends (the maximum first), taken when it differs from the value or the update is forced, and then told to the draw objects (the
    // window's own SetValue, not this class's) and posted to the parent as 0x800 (the box's id, the value).
    void DoVaryBoxButton(int16_t which);                           // 0x101d915c (src/InterfaceVaryBox.cpp, unit w37a)

    int32_t mMinValue;       // 0x138  0
    int32_t mMaxValue;       // 0x13c  1
    int32_t mValue;          // 0x140  0x9708cd: out of range, so the first ChangeValue always takes
    int32_t mOldValue;       // 0x144  0
    int32_t mIncrement;      // 0x148  1 (SetInc)
    int32_t mRepeatCountdown;   // 0x14c  not set by the constructor: the steps of a held button until the next change (the handler sets it)
    int32_t mRepeatStep;        // 0x150  not set by the constructor: the amount a held button changes the value by (the increment 1 only)
    uint8_t mForceUpdate;    // 0x154  1: ChangeValue takes the same value once, then clears it
    uint8_t mF155[3];        // 0x155  padding
    // Unit v5085 (stand-ins, untranslated): the range and the step, each handed over by address.
    void SetMinMaxValues(int32_t* minimum, int32_t* maximum);   // 0x101d8e44 (104 bytes)
    void SetInc(int32_t* increment);                            // 0x101d8ee8 (20 bytes)
    // Unit v5266: cuts the value to the range (the maximum first), takes it when it differs or the update is forced, then told to
    // the window's SetValue and posted to the parent as 0x800 (id, value). 1 when taken, else 0.
    int32_t ChangeValue(int32_t value);                         // 0x101d9080 (168 bytes, src/ui_whatami_InterfaceVaryBox_ChangeValue.cpp)
};

// unit w37b: tagLBEntry, Mac 0x50 bytes. Preorder traversal is child, sibling, then parent.
struct tagLBEntry {
    char16_t mText[32];       // 0x00, sprintf's %ls argument
    int32_t mData;            // 0x40, not read by this unit
    tagLBEntry* mNext;        // 0x44
    tagLBEntry* mChild;       // 0x48
    tagLBEntry* mParent;      // 0x4c
};
int32_t recurseDelete(tagLBEntry* entry); // 0x101d0f50

// unit w37c
int32_t NearestCharToPos(InterfaceDrawObject* object, int32_t x, int32_t y, int32_t length); // 0x101cdc94
