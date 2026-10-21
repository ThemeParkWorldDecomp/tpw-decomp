// UI_Base.cpp range (main app): the UI system's start-up, message queues, pointer and
// cursors (src/game/ui/UI_Base.cpp, src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp), its rectangles and its name hash, and the message pump
// (src/game/ui/UI_Base.cpp, src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/ui_ErrorDisplay.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_main.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UIInterface.cpp). PARTIAL: the device input (buttons, keys, wheel) is untranslated.
// The UI works in a 2048 x 1536 virtual screen (ThemeParkWorld::GetUIMousePos).
#pragma once
#include <cstdint>

// A region of the UI's screen: the base answers "nothing" to everything. Mac: the vtable
// pointer alone (4 bytes), vtable 0x102afcd8.
class CoolRegion {
public:
    virtual bool PointIn(int16_t x, int16_t y);                                         // 0x101d5a3c
    virtual void Middle(int16_t* x, int16_t* y);                                        // 0x101d5a70
    virtual void Shift(int16_t dx, int16_t dy);                                         // 0x101d5aa0
    virtual void Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom);      // 0x101d5ad0
    virtual ~CoolRegion();                                                              // 0x101d5b00
    virtual int32_t operator==(CoolRegion& other);                                      // 0x101d5b74
};

// A rectangle: left and top inside, right and bottom outside. Mac 0xc bytes: the four
// coordinates at 4, 6, 8, 0xa; vtable 0x102af8fc.
class CoolRect : public CoolRegion {
public:
    CoolRect();                                                             // 0x101c7448
    CoolRect(int16_t left, int16_t top, int16_t right, int16_t bottom);     // 0x101c7494
    ~CoolRect() override;                                                   // 0x101c74e0
    void Set(int16_t left, int16_t top, int16_t right, int16_t bottom);     // 0x101c756c
    void Get(int16_t* left, int16_t* top, int16_t* right, int16_t* bottom); // 0x101c75a8
    void GetDims(int16_t* width, int16_t* height);                          // 0x101c7618
    bool PointIn(int16_t x, int16_t y) override;                            // 0x101c7678
    void Middle(int16_t* x, int16_t* y) override;                           // 0x101c7710
    uint32_t RectIntersects(CoolRect* other);                               // 0x101c7788
    void Shift(int16_t dx, int16_t dy) override;                            // 0x101c7a1c
    void Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom) override;   // 0x101c7a78

    int16_t mLeft;     // 0x04
    int16_t mTop;      // 0x06
    int16_t mRight;    // 0x08
    int16_t mBottom;   // 0x0a
};

// The other shapes a window can have as its region (window script record 4). Each stores the CoolRegion's vtable pointer and then
// its own, so on the Mac they are CoolRegion's 6 virtual members with these overrides: PointIn, Shift, Adjust and the destructor
// (Middle and operator== stay CoolRegion's). The names of the data members are from how PointIn uses them.

// A circle: centre and radius. Mac 0xc bytes, vtable 0x102afcb8.
class CoolCircle : public CoolRegion {
public:
    CoolCircle(int16_t x, int16_t y, int16_t radius);                       // 0x101d5bb0
    bool PointIn(int16_t x, int16_t y) override;                            // 0x101d5bfc
    void Shift(int16_t dx, int16_t dy) override;                            // 0x101d5c6c
    void Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom) override;   // 0x101d5cb4: nothing
    ~CoolCircle() override;                                                 // 0x101d6200

    int16_t mX;        // 0x04
    int16_t mY;        // 0x06
    int16_t mRadius;   // 0x08
};

// An ellipse: centre and the two radii. Mac 0xc bytes, vtable 0x102afc84.
class CoolEllipse : public CoolRegion {
public:
    CoolEllipse(int16_t x, int16_t y, int16_t radiusX, int16_t radiusY);    // 0x101d5ce4
    bool PointIn(int16_t x, int16_t y) override;                            // 0x101d5d34
    void Shift(int16_t dx, int16_t dy) override;                            // 0x101d5db0
    void Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom) override;   // 0x101d5df8: nothing
    ~CoolEllipse() override;                                                // 0x101d6180

    int16_t mX;         // 0x04
    int16_t mY;         // 0x06
    int16_t mRadiusX;   // 0x08
    int16_t mRadiusY;   // 0x0a
};

// A polygon: a list of points (x and y shorts), copied from the caller's array. Mac 0xc bytes, vtable 0x102afc50.
class CoolPoly : public CoolRegion {
public:
    CoolPoly(int16_t count, const int16_t* points);                         // 0x101d5e2c
    bool PointIn(int16_t x, int16_t y) override;                            // 0x101d5fac
    void Shift(int16_t dx, int16_t dy) override;                            // 0x101d60cc
    void Adjust(int16_t left, int16_t top, int16_t right, int16_t bottom) override;   // 0x101d6150: nothing
    ~CoolPoly() override;                                                   // 0x101d5f04

    int16_t mCount;      // 0x04  the number of points
    int16_t* mPoints;    // 0x08  an array of 2 * mCount shorts made with new[]
};

uint32_t UI_hash_cakes(const char* text, uint32_t multiplier);   // 0x101c8d40

// The UI's clock: an object it asks the time of. The base answers 0 (0x10187994); the one
// the game installs answers the bullfrog library's clock (0x10185b08, in ui_main.cpp).
class clockTickFunction {
public:
    virtual uint32_t tickyticky();
};
class LbTimeClockTickFunction : public clockTickFunction {
public:
    uint32_t tickyticky() override;
};
// The third one, 0x101cab58 (src/game/ui/UI_Base.cpp): the platform timer's microseconds / 1000
// (timeGetTime on Windows). Nothing translated installs it yet.
class timeGetTimeClockTickFunction : public clockTickFunction {
public:
    uint32_t tickyticky() override;
};

// The start of the interface (src/game/ui/UI_Base.cpp), called by InitGeneralUI. The Mac's
// UI_InitSystem answers 1 when everything was made and 0 when an allocation failed, and
// UI_DevicePointerMove answers 1 when the pointer was handled; no caller reads either, so
// the declarations (older than the translation) stay void.
class CursorTable;
class InterfaceWindow;
void UI_InitSystem(int32_t unused, int32_t queueSize, bool hebrew);                         // 0x101c8a5c: `unused` is never read
// 0x101c8c2c (236 bytes, src/game/ui/UI_Base.cpp): the end of the UI system UI_InitSystem started; DeInitGeneralUI calls it. 0 when there is no
// root window (nothing is done), else 1.
int32_t UI_CloseSystem();
// The cursors the UI keeps in a list of its own (the Mac's namespace-scoped CursorLibrary).
class InterfaceCursor;
class CursorLibrary {
public:
    // A node of the list of cursors: the cursor, 0xc bytes this code does not read, and the next node at +0x10.
    struct Node {
        InterfaceCursor* mCursor;   // 0x00
        uint8_t mF04[0xc];          // 0x04
        Node* mNext;                // 0x10
    };
    // 0x101cc8ec (120 bytes, src/game/ui/InterfaceCursor.cpp): walks the list, deleting each cursor (through its virtual destructor with the
    // deleting flag) and freeing the node. The head is left as it was, pointing at the freed first node. UI_CloseSystem calls it.
    static void RemoveAll();
};
extern CursorLibrary::Node* gCursorLibrary;   // 0x10395c2c, original name unknown: the head of the list of cursors
void UI_SetClockTickFunction(clockTickFunction* clock);                                      // 0x101cabe0
void UI_SetPeekCallBack(int32_t (*callback)(InterfaceWindow*, int32_t, int32_t, int32_t));   // 0x101c9f18
void UI_UseCursorTable(CursorTable* table);                                                  // 0x101ca170
void UI_SetStandardCursor(uint16_t cursor);                                                  // 0x101ca078
void UI_SetCursor(int16_t cursor);                                                           // 0x101c9f68
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101c9518 (src/game/ui/UI_Base.cpp, src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/ui_ErrorDisplay.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_main.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UIInterface.cpp)
class InterfaceStringHeap {
public:
    // 0x101d7808 (312 bytes, src/game/ui/InterfaceStringHeap.cpp): makes the heap's two tables: a rows of ... see AllocateResult.
    // The original answers 1 when it made them and 0 when it could not (a size is 0, 4 < c < a does not hold, or an
    // allocation failed); the declaration stays void because the unit libraries of the callers fake it as void.
    static void Allocate(int32_t a, int32_t b, int32_t c, int32_t d);
    // 0x101d7978 (60 bytes, src/game/ui/InterfaceStringHeap.cpp): gives the heap's tables back, table A then table B (delete[]); the pointers and the
    // sizes are left as they were. DeInitGeneralUI calls it (no `this`, nothing read).
    static void DeAllocate();
    // 0x101d7808, the translation (what it answers is described above): table A is b rows of a 16-bit entries, table B
    // d rows of c; the first word of each row is set to -1 (the second table's loop runs b times).
    static int32_t AllocateResult(int32_t a, int32_t b, int32_t c, int32_t d);
    // 0x101d7b10 (src/game/ui/InterfaceStringHeap.cpp, unit w54): gives a string of the heap back (the button's destructor does, for its text):
    // its first two characters become 0xffff, which is how the heap tells a free string.
    static void ReleaseString(char16_t* text);
    // Unit v5975 (src/game/ui/InterfaceStringHeap.cpp): 0x101d79ec (236): a free string of at least `length` characters
    // (first word -1) from table B then table A, its first character set to 0; NULL when none or no tables.
    static char16_t* GetString(int32_t length);
    // 0x101d7b60 (88): GetString(wcslen(text)) and a copy of the text into it; the string or NULL.
    static char16_t* GetAndCopy(const char16_t* text);
};

// unit w50 (src/game/ui/UI_Base.cpp): each posts one key message to the key capture (0x1000a down, 0x1000b up, 0x1000c typed) while the UI
// is active, and answers whether it was (the Mac's r3: gUIActive != 0).
bool UI_Keydown(int16_t key, int32_t flags);          // 0x101c929c
bool UI_Keyup(int16_t key, int32_t flags);            // 0x101c9314
bool UI_KeyChar(int16_t character, int32_t flags);    // 0x101c9388
void UI_DevicePointerMove(int16_t x, int16_t y);      // 0x101c8d94
void UI_DeviceButtonDown(int16_t button, int16_t x, int16_t y);   // 0x101c9034
void UI_DeviceButtonUp(int16_t button, int16_t x, int16_t y);     // 0x101c9100
void UI_DeviceMouseWheel(int32_t delta, int16_t x, int16_t y, int16_t modifiers);  // 0x101c91cc
void UI_Activate();                                   // 0x101ca5e0
void UI_DeActivate();                                 // 0x101ca658
void activateUI(InterfaceWindow* window, int32_t active); // 0x101ca30c (src/game/ui/UI_Base.cpp): posts 0x21 (active) or 0x22 to the window and its descendants
void UI_SetRealScreenRez(int16_t width, int16_t height);   // 0x101c9ed8
void UI_HideCursor();                                 // 0x101ca0b0: hides the current cursor, if there is one
void UI_ShowCursor();                                 // 0x101ca110
int16_t UI_GetButtonFlags();                          // 0x101ca944, nonzero while a mouse button is down

extern int16_t gUIRealScreenWidth;    // 0x10395bbc, original name unknown
extern int16_t gUIRealScreenHeight;   // 0x10395bba
extern InterfaceWindow* gUIRootWindow; // 0x10395c24
extern int32_t gUIActive;              // 0x102af924, starts at 1
extern int16_t gUIButtonFlags;         // 0x10395bbe, cleared on deactivation

// ---- the default window handler's state (src/game/ui/InterfaceWindow.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceTimer.cpp unless said otherwise; original names unknown)

// The state of one mouse button as the handler follows it. Mac 0xc bytes (the table is three of them at 0x10395bd0).
struct UIButtonState {
    int16_t mState;   // 0x00  0 up, 1 pressed and not moved far, 2 pressed and moved more than 6 or already a double click
    int16_t mX;       // 0x02  where it went down
    int16_t mY;       // 0x04
    int16_t mF06;     // 0x06  not used
    int32_t mTime;    // 0x08  the tick it went down at (0 after a release that was not a click)
};
extern UIButtonState gUIButtons[3];      // 0x10395bd0, left, right and middle
extern int32_t gUIDoubleClickTime;       // 0x102af91c, 500: the ticks inside which a second press or release makes a double click
extern int32_t gUIAutoRedraw;            // 0x10395c28, set elsewhere: when nonzero, a change to a window posts it a draw message (1)
extern int32_t gUIHelpTextResult;        // 0x10395c60, message 0xc stores the window's help text number here and answers the address
                                         // (the word after it, 0x10395c64, is the help text's string, filled in by others)
// The word after gUIHelpTextResult on the Mac (0x10395c64): the help text's string, NULL unless some code sets it. CUIHelpWindow::ShowHelpText
// (src/game/ui/ui_tooltip.cpp, unit w56) reads it as the second word of message 0xc's answer. Defined in src/game/ui/ui_tooltip.cpp.
extern char16_t* gUIHelpTextString;      // 0x10395c64
extern InterfaceWindow* gUIFocusWindow;  // 0x10395c20, the window that has the focus (CheckFocus); NULL (src/game/ui/InterfaceMessageQueue.cpp, unit w29)
uint32_t IveGotTicks();                  // 0x101cac3c (src/game/ui/UI_Base.cpp, unit w29): the UI clock's tick

// ---- message queues (src/game/ui/InterfaceMessageQueue.cpp; the range is UI_Base.cpp's 0x101c7ad8..0x101c85f0)

// A queued message. Mac 0x10 bytes; the window is 0 in a free slot.
struct InterfaceMessage {
    InterfaceWindow* mWindow;   // 0x00
    int32_t mMessage;           // 0x04
    int32_t mA;                 // 0x08
    int32_t mB;                 // 0x0c
};

// A ring of messages. Mac 0x14 bytes. Add writes at mTail and walks it down (wrapping from 0 to
// count - 1); Post reads at mHead and walks it down the same way.
class InterfaceMessageQueue {
public:
    InterfaceMessageQueue();                                                           // 0x101c7ad8
    ~InterfaceMessageQueue();                                                          // 0x101c7b24 (src/game/ui/InterfaceMessageQueue.cpp): frees and clears mEntries
    // Makes `count` slots, all free. 1 when it did, 0 when it already had slots or the allocation failed
    // (a `short` on the Mac).
    int16_t Initialise(int32_t count);                                                 // 0x101c7bc4
    // Queues a message in the slot at mTail when that slot is free and the window is not NULL; 1 when it did.
    int16_t Add(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101c7c98
    // Hands the message at mHead to its window's handler (src/game/ui/InterfaceMessageQueue.cpp; the key messages 0x1000a to 0x1000c go to the
    // key capture's, the keyboard or the focus window instead) and frees the slot; 1 when there was one, 0 when the slot
    // at mHead is free. A message with 0x80808080 in any of its three words (what InvalidateMsgs leaves) is dropped
    // without a call.
    int16_t Post();                                                                    // 0x101c7d7c
    // The message at mHead, or NULL when there is none (no slots, a free slot or an invalidated message).
    InterfaceMessage* Peek();                                                          // 0x101c7fa8
    // -1 when there are no slots; else the slots of this window get 0x80808080 in the message and both arguments, and the answer is how
    // many (as a short). (src/game/ui/InterfaceMessageQueue.cpp, unit w51)
    int32_t InvalidateMsgs(InterfaceWindow* window);                                   // 0x101c8038
    void Flush();                                                                      // 0x101c80fc: all slots free again
    int32_t GetNumQd();                                                                // 0x101c8190: mQueued

    InterfaceMessage* mEntries;   // 0x00  mCount slots, an array made with new[]
    int32_t mCount;               // 0x04
    int32_t mHead;                // 0x08  count - 1 when empty
    int32_t mTail;                // 0x0c  count - 1 when empty
    int32_t mQueued;              // 0x10
};

// A message with a time to be delivered at. Mac 0x14 bytes; a slot is free when its time is 0.
struct InterfaceTimedMessage {
    InterfaceWindow* mWindow;   // 0x00
    int32_t mMessage;           // 0x04
    int32_t mA;                 // 0x08
    int32_t mB;                 // 0x0c
    uint32_t mTime;             // 0x10
};

// Mac 8 bytes.
class InterfaceTimedMessages {
public:
    InterfaceTimedMessages();                                                          // 0x101c81d0
    ~InterfaceTimedMessages();                                                         // 0x101c8214 (src/game/ui/InterfaceMessageQueue.cpp): frees mEntries (not cleared)
    int16_t Add(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time);   // 0x101c82ac (untranslated)
    // Delivers every message whose time has come (the clock is IveGotTicks) to its window's handler and frees its slot;
    // answers how many it delivered. (Declared void before the pump was translated: the Mac's answer is a `short`.)
    int16_t Process();                                                                 // 0x101c83d4
    // -1 when there are no slots; else the slots of this window get time 0, and the answer is how many (as a short). (src/game/ui/InterfaceMessageQueue.cpp)
    int32_t InvalidateMsgs(InterfaceWindow* window);                                   // 0x101c84d8
    void Flush();                                                                      // 0x101c8580: all times 0
    // Makes `count` slots (fewer than 0x200) with a time of 0, and leaves the rest of each as the allocation was.
    int16_t Initialise(int32_t count);                                                 // 0x101c85f0

    int32_t mCount;                  // 0x00
    InterfaceTimedMessage* mEntries; // 0x04  an array made with new[]
};

// One of the UI's timers (the table UI_InitTimerSystem makes). Mac 0x20 bytes. The members other than the destructor and FindTimer are
// untranslated.
class InterfaceTimer {
public:
    InterfaceTimer();                                           // 0x101d819c
    // 0x101d830c (src/game/ui/InterfaceTimer.cpp, unit w54): takes the timer out of its list (its own links are left). The class has no vtable.
    ~InterfaceTimer();
    // The tail of the same Mac function, which only runs for the deleting form: the timers are in the table, so nothing is freed; the
    // slot is checked to lie in the table (an assertion that goes on) and marked free. `delete timer` is then the Mac's deleting call.
    static void operator delete(void* object);                  // 0x101d830c, the deleting flag's half
    // Unit v5975 (src/game/ui/InterfaceTextBox.cpp): 0x101d822c (180), the class's own operator delete (name
    // given here to keep it apart): asserts the slot lies in the table (msl_assertion_failed, goes on), then marks it free (+0x14).
    static void OperatorDelete(void* object);
    void Stop();                                                // 0x101d8428
    void Start();                                               // 0x101d8460
    void SetSlice(int32_t slice);                               // 0x101d84c8 (src/game/ui/InterfaceTimer.cpp, unit v5L06): mSlice = slice
    // The LAST timer of the list from this one on with that window and id, or NULL (src/game/ui/InterfaceTimer.cpp, unit w51).
    InterfaceTimer* FindTimer(InterfaceWindow* window, int32_t id);   // 0x101d8500

    InterfaceWindow* mWindow;   // 0x00
    uint32_t mStart;            // 0x04  the tick it started at
    int32_t mSlice;             // 0x08
    int32_t mId;                // 0x0c
    int32_t mRunning;           // 0x10
    int32_t mFree;              // 0x14  1 when the slot is not in use
    InterfaceTimer* mNext;      // 0x18
    InterfaceTimer* mPrevious;  // 0x1c
};
void UI_InitTimerSystem(int32_t count);   // 0x101d86c4: a table of `count` timers, none in use, none running
// 0x101d8c2c (60 bytes, src/game/ui/InterfaceTimer.cpp): destroys the table (each timer's destructor unlinks it, last timer first, then the array is
// freed) and forgets the list of timers in use; the table's pointer and count are left as they were.
void UI_CloseTimerSystem();

// The messages (src/game/ui/UI_Base.cpp). Messages 1 to 3 go to the draw queue and every other to the general queue, except
// that 0x1000a to 0x1000c go there without any test (NULL window or queue included) and answer 0. For the rest the
// answer is 1 when the queue exists and the window is neither NULL nor being destroyed (even if the queue was full
// and dropped the message), else 0.
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c9400
void UI_FlushAllMessages();                                       // 0x101c9e48
int32_t UI_UpdatePointerIn();                                     // 0x101c8f18: 1 when there is a root window and nobody has captured the device

// ---- pointer, cursors and help text (src/game/ui/UI_Base.cpp, src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp)
class InterfaceCursor;
class InterfaceHelpText {
public:
    InterfaceHelpText();            // 0x101cf8fc (src/game/ui/InterfaceHelpText.cpp, unit w36)
    virtual void ShowHelpText();    // 0x101cf874
    virtual void HideHelpText();    // 0x101cf8b8
    virtual ~InterfaceHelpText();   // 0x101cf954
    virtual void Enable();          // 0x101cf9d0
    virtual void Disable();         // 0x101cfa14
    virtual void Show();            // 0x101cfa88
    virtual void Hide();            // 0x101cfabc
    virtual void Update();          // 0x101cfaf0
    virtual void OnMouseMoved(int16_t x, int16_t y);          // 0x101cfb24 (the base keeps the position)
    virtual void OnMouseMovedIn(InterfaceWindow* window);     // 0x101cfb68 (the base keeps the window)
    virtual void OnMouseMovedOut();                           // 0x101cfc04
    virtual void OnMouseDown();                               // 0x101cfc8c
    virtual void OnFrame();                                   // 0x101cfd10 (the base does nothing)
    // Told by InterfaceWindow::Show when a window is hidden (src/game/ui/InterfaceWindow.cpp, unit w30b).
    virtual void OnHideOrDelete(InterfaceWindow* window);     // 0x101cfdb0
    virtual void Init();                                      // 0x101cfd48 (the base does nothing)
    virtual void DeInit();                                    // 0x101cfd7c (the base does nothing)
    // All of the class is translated in src/game/ui/InterfaceHelpText.cpp (unit w36). The Mac's vtable is 0x102afa88, its slots in the order of
    // the declarations above (8 bytes of header, then 4 bytes a slot; HideHelpText is slot 0xc). The Mac's object is 0x18 bytes
    // with the vtable pointer at +0x10; the data members below are named from the base class's members (original names unknown).
    int16_t mEnabled;           // 0x00  1 after the constructor (Enable and Disable)
    int16_t mWanted;            // 0x02  ShowHelpText and HideHelpText
    int16_t mMouseX;            // 0x04  OnMouseMoved
    int16_t mMouseY;            // 0x06
    int16_t mShowing;           // 0x08  0 after the constructor
    uint32_t mMovedInTicks;     // 0x0c  the tick OnMouseMovedIn ran at
    InterfaceWindow* mWindow;   // 0x14  the window the pointer is in (OnMouseMovedIn), NULL after OnMouseMovedOut or OnMouseDown
};
// Makes `handler` the help text handler, or the default one for NULL (0x101ca978, src/game/ui/UI_Base.cpp, unit w36).
void UI_SetHelpTextHandler(InterfaceHelpText* handler);
void UI_DisableHelpText();   // 0x101caa4c: the handler's Disable
void UI_EnableHelpText();    // 0x101caaac: the handler's Enable

// Unit v5295 (src/game/ui/UI_Base.cpp)
// Calls the handler's Disable when its mEnabled is not 0, else its Enable (vtable offsets 0x18 and 0x14 on the Mac).
void UI_ToggleHelpText();    // 0x101ca9d0
// UI_Base.cpp's static initialiser: constructs gUIDefaultHelpText, registers it for destruction (destructor TVector 0x10268238,
// node gUIDefaultHelpTextNode) and stores the vtable of gUIDefaultClock (Mac: 0x102a7ae4 then 0x102af94c, the second stays).
void __sinit_UI_Base_cpp();  // 0x101cac94
extern char gUIDefaultHelpTextNode[16];   // 0x10395ba4, the registration node (original name unknown)
void UI_DestroyDefaultHelpText(InterfaceHelpText* object, int16_t deleteFlag);   // the destructor TVector at 0x10268238

// Words of the keyboard capture (InterfaceWindow::CaptureKeys): the list of keys the window wants and the window.
struct captureKeyList;
struct UIKeyCapture {
    const captureKeyList* mKeys;   // 0x00
    InterfaceWindow* mWindow;      // 0x04
};

// The UI's state (original names unknown). Defined in src/game/ui/UI_Base.cpp unless said otherwise.
extern InterfaceWindow* gUIPointerIn;            // 0x10395c1c, the window the pointer is in (set by UI_InitSystem to the root)
extern InterfaceWindow* gUICaptureDevice;        // 0x10395c14, the window that has captured the pointer (CaptureDevice); NULL
extern InterfaceWindow* gUILastWindow;           // 0x10395c18, the window the last constructor made (src/game/ui/InterfaceWindow.cpp)
extern UIKeyCapture gUIKeyCapture;               // 0x10395c08
extern InterfaceMessageQueue* gUIDrawQueue;      // 0x10395c00, messages 1 to 3
extern InterfaceMessageQueue* gUIMessageQueue;   // 0x10395c04, the others
extern InterfaceTimedMessages* gUITimedMessages; // 0x10395bfc
// Unit v51048: 0x101c95f8 (120 bytes): adds the message to the timed messages when they exist, the window is not NULL and the message is
// not 1 to 3 (those go to the draw queue); 1 when added, else 0.
int32_t UI_PutDeferredMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time);
extern int32_t (*gUIPeekCallBack)(InterfaceWindow*, int32_t, int32_t, int32_t);   // 0x10395bf8, UI_SetPeekCallBack
extern int32_t gUIStarted;                       // 0x10395bf4, 1 once UI_InitSystem has made the root window
extern bool gUIHebrew;                           // 0x10395b88, UI_InitSystem's third argument
extern int16_t gUIPointerX;                      // 0x10395bc2, the last pointer position (UI_DevicePointerMove)
extern int16_t gUIPointerY;                      // 0x10395bc0
extern InterfaceCursor* gUICursor;               // 0x10395bb0, the cursor in use; NULL
extern CursorTable* gUICursorTable;              // 0x10395bb4, UI_UseCursorTable
extern uint16_t gUIStandardCursor;               // 0x10395bb8, UI_SetStandardCursor: what UI_SetCursor(-1) and a missing cursor become
extern int16_t gUICursorNumber;                  // 0x102af920, the number of the cursor in use; -1 at first
extern InterfaceHelpText* gUIHelpText;           // 0x102af928, the help text handler (UI_SetHelpTextHandler, src/game/ui/InterfaceHelpText.cpp); starts as gUIDefaultHelpText
extern InterfaceHelpText gUIDefaultHelpText;     // 0x10395b8c, the handler before one is set (made by InterfaceHelpText's constructor, 0x101cf8fc: enabled)
extern clockTickFunction* gUIClock;              // 0x102af92c, UI_SetClockTickFunction; starts as gUIDefaultClock
extern clockTickFunction gUIDefaultClock;        // 0x10395b74, what UI_SetClockTickFunction(NULL) installs
extern InterfaceTimer* gUITimers;                // 0x10395c54, the table (UI_InitTimerSystem)
extern int32_t gUITimerCount;                    // 0x10395c50
extern InterfaceTimer* gUITimerList;             // 0x10395c58, the timers in use, linked through mNext
extern int32_t gUITimersPaused;                  // 0x10395c4c


// ---- the message pump (src/game/ui/UIInterface.cpp): what UIInterface::Process does once a pass

// A key a window asked for (InterfaceWindow::CaptureKeys): the list ends with the entry whose modifier is -1.
struct captureKeyList {
    int16_t mKey;         // 0x00
    int16_t mModifiers;   // 0x02  compared with bits 16 to 23 of the message's second argument
};

// The pump's state (original names unknown). Defined in src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/UI_Base.cpp.
extern InterfaceWindow* gUIKeyboardWindow;   // 0x10395c10, the window that has captured the keyboard (CaptureKb); NULL
extern int32_t gUIOnFrameCount;              // 0x10395b84, the windows onframe has visited this pass

// The queues. Dispatch hands messages of the general queue to their windows, up to `limit` of them, and answers how many it
// counted (the peek callback, if there is one, is told of each message first). With a peek callback the limit is exact. Without
// one the limit is tested after each message, so a queue longer than the limit sends one message more than it answers, and one
// goes out whatever the limit is (even 0 or less) if the queue has any. The draw queue (messages 1 to 3) always counts that way. The deferred ones
// are the timed messages that are due: -1 when there is no timed queue, else how many.
int32_t UI_DispatchMessages(int32_t limit);        // 0x101c96b4
int32_t UI_DispatchDrawMessages(int32_t limit);    // 0x101c97d8
int32_t UI_DispatchDeferredMessages();             // 0x101c987c
// Sends the on-frame message (0x1e) to the window and, in tree order, all its descendants that are not being destroyed;
// counts every window it visits, destroyed or not, in gUIOnFrameCount. (The Mac unrolls the first five levels of the tree.)
void onframe(InterfaceWindow* window);             // 0x101c98f0
// Zeroes the count, sends the on-frame messages from the root window if there is one, and answers the count.
int32_t UI_SendOnFrameMessages();                  // 0x101c9c2c
// The UI's clock (gUIClock).
// The depth highestZ works out for a window (the root when NULL) and a rectangle (none when NULL); -1 when there is no window.
int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect);   // 0x101ca880
// 0x101ca6dc (unit w50, src/game/ui/UI_Base.cpp): the Z one above the highest visible descendant of the window that the rectangle
// touches (any, when the rectangle is NULL). The running answer lives in a static word that only the root resets: a window
// that is not the root starts from what the last call left (see the file).
int32_t highestZ(InterfaceWindow* window, CoolRect* rect);
extern int32_t gUIHighestZ;        // 0x10395b80  highestZ's running answer (names unknown for the three; src/game/ui/UI_Base.cpp)
extern int32_t gUIHighestZAttr;    // 0x10395b78  the attribute word of the child highestZ looked at last
extern int32_t gUIHighestZChildZ;  // 0x10395b7c  the Z of that child (or of its first item, for a list control)

// The timers (the table of UI_InitTimerSystem). AddTimer takes the first free timer of the table, resets it (not running, its
// start tick the clock's now) and links it at the end of the list of timers in use; NULL when the table is full. It asserts
// that the table exists. Service does nothing while the timers are paused; otherwise, for each timer in use whose slice has gone
// by, it posts message 0x10 (the timer's id, how many it has posted in this call) to the timer's window, once for each slice that
// fits in the time elapsed, when the timer is running, has a window and the UI is active. A timer whose slice is 0 or less
// never lets that loop finish (the original hangs, so does this).
InterfaceTimer* UI_AddTimer(InterfaceWindow* window, int32_t slice, int32_t id);   // 0x101d878c
void UI_ServiceAllTimers();                                                       // 0x101d88e4

// ---- ui_utils.cpp / ui_main.cpp: drawing text (src/game/ui/ui_utils.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp, unit w35b). UI text is 16-bit (the Mac wchar_t).
struct TbRect;
struct TbColour;
struct TbRenderArea;

// The render area the UI's text is drawn on (a TbRenderArea made by __sinit_ui_utils_cpp) and the word behind it that says whether
// something has set it up (UI_DrawPostcardText sets it to 1 around its UI_DrawText, with the area made over the postcard's pixels).
extern TbRenderArea gUIRenderArea;           // 0x1038cd00 (name invented)
extern int32_t gUIRenderAreaInUse;           // 0x1038cd28 (name invented)
// The multibyte text UIMain_ConvertString converts into: 0x4000 bytes (the converter is told 0x3fff), no other size is known.
extern uint8_t gUIMainMBBuffer[0x4000];      // 0x10388c48 (name invented; defined in src/game/ui/ui_main.cpp)

// 0x1018b578: draws `text` in `rect` with the font number `font` (0 to 12 of the current resolution group), `colour`, and the
// justifications (horizontal 0 left, 1 centre, 2 right; vertical 0 top, 1 centre, 2 bottom; the Hebrew languages swap left
// and right). With `wrap` 0 the text is not wrapped: the box is 0x4000 wide and starts where the justification puts it. When
// `onCurrent` is 0 it is drawn on gUIRenderArea, and not at all unless gUIRenderAreaInUse is set; otherwise on the Bullfrog
// library's current area. `extent` (if not NULL) gets the rectangle the formatter reports. The clip rectangle is the box
// during the draw and released afterwards.
void UI_DrawText(const char16_t* text, const TbRect& rect, const TbColour& colour, int8_t font, int8_t horizontal,
                 int8_t vertical, int32_t wrap, TbRect* extent, int32_t onCurrent);
// 0x1018b360: makes `area` describe `width` x `height` pixels at `pixels` in the engine's texture format (the masks, depth
// and palette flag of gei_TextureSettings' pixel format; the stride is the bytes of a pixel times the width). NULL: nothing.
void MakeRenderArea(uint8_t* pixels, TbRenderArea* area, int32_t width, int32_t height);
// Unit v51012 (src/game/ui/ui_utils.cpp): raw pixel data. The mangled name passes the type as an enum word.
enum RAWSurfaceType : int32_t {kRAWSurface24 = 0, kRAWSurface32A = 2, kRAWSurface32B = 3 };
void MakeRenderAreaFromRAW(void* pixels, int32_t width, int32_t height, RAWSurfaceType type, TbRenderArea* area);   // 0x1018b434
extern uint8_t gUIUtilsColour[5];   // 0x1038cd2c, original name unknown: opaque black (alpha 0xff, 0, 0, 0) and its palette index
void __sinit_ui_utils_cpp();        // 0x1018bda8
// 0x1018b2c8: in a Hebrew language 0 (left) becomes 2 (right) and 2 becomes 0.
void AdjustJustificationForHebrew(int32_t& justification);
// 0x10181a7c: the text as multibyte in gUIMainMBBuffer, converted only when the language has a Unicode to multibyte
// converter (otherwise the buffer is left as it was); answers the buffer either way.
// The original has ONE such function, taking a char16_t* (declared in ui_DrawObject.h, defined in src/game/ui/ui_main.cpp); this const overload
// forwards to it (src/game/ui/ui_utils.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp).
uint8_t* UIMain_ConvertString(char16_t* text);                 // 0x10181a7c (src/game/ui/ui_main.cpp, unit w44; also declared in ui_DrawObject.h)
uint8_t* UIMain_ConvertString(const char16_t* text);

// unit w37b: Bullfrog drawing calls used by the list box.
struct TbRect;
struct TbColour;
void LbDraw_RectangleFilled(const TbRect* rect, TbColour colour); // bullfrog 0x1002af44
void LbDraw_Text(int32_t x, int32_t y, const char* text, TbColour colour); // bullfrog 0x1001b6a8

// unit w37c
// Takes the window's timer with that id out of the list of timers in use and frees it (src/game/ui/InterfaceTimer.cpp, unit w51). The Mac
// answers 1 when it did and 0 when there was none; the declaration stays void because the callers' unit libraries fake it as void, and
// UI_RemoveTimerResult is the translation that answers.
void UI_RemoveTimer(InterfaceWindow* window, int32_t id); // 0x101d8a00
int32_t UI_RemoveTimerResult(InterfaceWindow* window, int32_t id);

// unit w46: the mouse button entry points (src/game/ui/UI_Base.cpp). Both answer gUIActive != 0 on the Mac; no caller reads it.
extern InterfaceWindow* gUIButtonWindow[3];   // 0x10395bc4, the window each button went down in (set by UI_DeviceButtonDown, cleared by Up)

// unit w51 (src/game/ui/UI_Base.cpp): forgetting a window that is going.
// Clears the globals that hold the window: the pointer's window, the focus, the device capture and the three buttons' windows.
void ClearAllPointers(InterfaceWindow* window);   // 0x101ca254
// -1 when the queue (the general one, the draw one, the timed one) does not exist or the window is NULL; else the queue's InvalidateMsgs.
int32_t UI_InvalidateMessages(InterfaceWindow* window);           // 0x101c9ca4
int32_t UI_InvalidateDrawMessages(InterfaceWindow* window);       // 0x101c9d2c
int32_t UI_InvalidateDeferredMessages(InterfaceWindow* window);   // 0x101c9db8
