// Elegant Chaos "Sam's Utilities" shim calls made by game-side code (sams utils shared).
// Declarations only; the platform layer implements them (analysis/platform-layer-design.md).
//
// Classes are declared with the members game code touches (Mac offsets in comments);
// the platform layer owns the rest of their state and may add members.
#pragma once
#include <cstdint>

#include "platform/interfacelib.h"

namespace SamsUtilities {

// Main app local variadic debug routine, 0x10000e40 (a no-op in this build).
void UDebug(const char* format, ...);

// _USwapBlock32(unsigned long*, long): byte-swaps `count` 32-bit words in place. The Mac
// build calls it to convert little-endian file data; on a little-endian host it is a
// no-op.
void _USwapBlock32(uint32_t* words, int32_t count);
// _USwapBlock16(unsigned short*, long): the same for 16-bit words.
void _USwapBlock16(uint16_t* words, int32_t count);
// _USwapCopy32(unsigned long*, long): a new array of `count` words, the copy byte-swapped as _USwapBlock32 does; _USwapDelete frees it.
uint32_t* _USwapCopy32(const uint32_t* words, int32_t count);
void _USwapDelete(void* copy);
// _USwapAllocate(unsigned long) (unit w166): a block of `bytes` bytes (contents undefined) that _USwapDelete frees; the file saves swap field by field in it.
void* _USwapAllocate(uint32_t bytes);
template<class T> void USwapBlock32(T* words, int32_t count);
template<class T> void USwapInplace32(T& word);

// UString (UString.cpp): {vptr, length +4, text +8}. The text is a NUL-terminated
// buffer; a default-constructed string points at a shared empty string.
class UString {
public:
    UString();                                   // 0x10007070
    UString(const char* text);                   // 0x100070c8
    UString(const char* text, uint32_t length);  // 0x10007174
    virtual ~UString();                          // 0x100073dc

    // 0x10008044 (the library's UString::operator=; platform/samsutils_version.cpp): the string takes a copy of `other`'s text. (Without it
    // the implicit assignment would copy the text pointer and free it twice.)
    UString& operator=(const UString& other);

    static const UString kEmptyString;
    // Copies a C string into a Pascal string (at most 255 characters).
    static void c2pcpy(unsigned char* dest, const char* source);

    uint32_t mLength;   // 0x04
    char* mText;        // 0x08
};

// UResourceId (UNetwork.cpp range): a resource type name and number, {UString 0x00,
// id 0x0c}. Its destructor is inline (the UString member's).
class UResourceId {
public:
    UResourceId(const UString& type, int32_t id);  // 0x10021014
    UString mType;   // 0x00
    int32_t mId;     // 0x0c
};

// UVersion (UVersion.cpp; platform/samsutils_version.cpp): a version number and its texts, filled in from the application's 'vers'
// resource. The Mac object is 0x24 bytes: the major version (a halfword) at +0, the minor at +2, the fix at +3, a word at +4 (the
// release stage, 0x80 when the constructor makes it), the build (a halfword) at +8, the region at +0xa, then two UStrings at +0xc and
// +0x18. The game (UI_CreateVersionNumber) makes one with the constructor, loads the default 'vers' resource and prints the text at
// +0xc ("2.04b3") into the version label; the comment string at +0x18 is only ever destroyed.
class UVersion {
public:
    // 0x100188d8: the four numbers (the game passes 2, 0, 0, 0), the stage 0x80 and two empty strings.
    UVersion(uint16_t major, uint8_t minor, uint8_t fix, uint16_t build);
    // 0x10018cc8: reads the version resource `resource` into the object (the numbers and the two texts) when there is one. On the
    // host the application has no resource fork: the default resource answers the game's version, text "2.04b3" and no comment,
    // and any other resource leaves the object alone. `create` is the second argument the game passes as 0 (unused here).
    void LoadFromResource(const UResourceId& resource, uint8_t create);
    // The default version resource ('vers' number 1; made by __sinit_UVersion_cp). A data export of the library.
    static const UResourceId kDefaultVersionResource;

    uint16_t mMajor;      // 0x00
    uint8_t mMinor;       // 0x02
    uint8_t mFix;         // 0x03
    int32_t mStage;       // 0x04
    uint16_t mBuild;      // 0x08
    uint8_t mRegion;      // 0x0a
    UString mText;        // 0x0c  the short version text, "2.04b3"
    UString mComment;     // 0x18
};

// File items (UFile.cpp / UFolder.cpp). Virtual slots as in the Mac vtables: the
// destructor 0x08, GetPath 0x28; UFolder::GetItem 0x48.
class UFileItem {
public:
    virtual ~UFileItem();
    virtual void GetPath(UString& path) = 0;
};

class UFolder : public UFileItem {
public:
    // The item called `name` of the given type ('fold' for a folder) inside this folder,
    // a new object the caller deletes; NULL if there is none.
    virtual UFileItem* GetItem(const UString& name, int32_t type);
};

// UProcess (UProcess.cpp): the running application. GetFolder returns the folder that
// holds the application (field +4).
class UProcess {
public:
    UFolder* GetFolder();
};
extern UProcess gThisProcess;

class URectangle {
public:
    // A rectangle in long coordinates, laid out as a Win32 RECT.
    struct LongRect {
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    };
    URectangle() : mRect{0, 0, 0, 0} {}        // 0x1001b3c4
    // 0x1001b1d4: the four edges, then Rationalize (left <= right, top <= bottom).
    URectangle(int32_t left, int32_t top, int32_t right, int32_t bottom);
    virtual ~URectangle();
    LongRect mRect;   // 0x04
};

// URectangleMac (UInterrupts.cpp range): a URectangle made from a QuickDraw Rect.
class URectangleMac : public URectangle {
public:
    explicit URectangleMac(const Rect& rect);  // 0x1001c5ec (copies, then Rationalize)
    // Inline in the game (MOVIE_Play): a URectangle of four edges that is a URectangleMac.
    URectangleMac(int32_t left, int32_t top, int32_t right, int32_t bottom) : URectangle(left, top, right, bottom) {}
    void ToGlobal(GrafPort* port);             // 0x1001ca00: local -> global coordinates
    void ToLocal(GrafPort* port);              // 0x1001c8ec: global -> local coordinates
    // 0x1001cddc: the rectangle as a QuickDraw Rect in a ring of scratch storage (valid
    // until 128 more have been asked for).
    operator Rect*();
};

// A point in long coordinates (Mac h, v).
struct UPoint {
    int32_t x;   // 0x00 (h)
    int32_t y;   // 0x04 (v)
};

class UPointMac : public UPoint {
public:
    void ToLocal(GrafPort* port);   // 0x1001d25c: global -> local of `port` (NULL: current)
};

// UEvent (UThread.cpp range): a Mac EventRecord (what, message, when, where (v at 0x0a,
// h at 0x0c), modifiers 0x0e).
class UEvent {
public:
    UPoint GetPosition() const;          // 0x100255b4: {where.h, where.v}
    uint16_t GetKeyModifiers() const;    // 0x100256e8: the modifiers word
};

enum MouseButton : int32_t {
    kMouseButton0 = 0,
    kMouseButton1 = 1,
    kMouseButton2 = 2
};

struct UMicroseconds;
class UPixels;
class UOpenGL;

// A cursor (UCursor.cpp of the shim: 0x1157c..0x11e58). The members are the shim's names;
// the state is the platform layer's (platform/cursor.cpp), not the Mac object's. No virtual
// members and no constructor of its own: the UIInit unit test builds a fake UCursorOpenGL
// without the platform library.
class UCursor {
public:
    static void SetPosition(const UPointMac& position);   // 0x10011dbc, global coordinates
    void SetRate(const UMicroseconds& rate);               // 0x118cc: the time each frame of an animated cursor shows
    void Set();          // 0x116a4: this becomes the cursor shown
    void Reset();        // 0x11778: the host's arrow again, if this was the cursor shown
    // Show, Hide and IsVisible are static: the Mac members ignore `this` (InterfaceBFCursor's pass their own, which is
    // not even a UCursor). They count: Show adds one (the Mac's ShowCursor when the count was 0), Hide takes one off (its
    // HideCursor when the count reaches 0) and each answers the count; IsVisible is 1 while the count is above 0. The
    // count starts at 1.
    static int32_t Show();         // 0x11950
    static int32_t Hide();         // 0x119ec
    static uint8_t IsVisible();    // 0x11cd0
    void Animate();      // 0x1180c: the next frame when its time has come

    void (*mDraw)(UCursor*) = nullptr;   // platform: draws the current frame over the back buffer (the OpenGL one sets it)
    int64_t mRate = 0;                   // microseconds a frame shows (0: no animation)
    int32_t mFrame = 0;                  // the frame shown (modulo the count)
    uint64_t mFrameSince = 0;            // when it started, SDL ticks (milliseconds)
};

// A cursor of frames (UPixels), drawn by OpenGL (0x1a590..0x1b0f4 of the shim). The Mac object is 0x84 bytes;
// the platform layer owns its state.
class UCursorOpenGL : public UCursor {
public:
    // `frames` is an array of `count` UPixels, one for each frame of the cursor.
    UCursorOpenGL(UPoint& hotSpot, UPixels* frames, int32_t count, UOpenGL* context);
    ~UCursorOpenGL();
    void SetHotspot(UPoint& hotSpot);   // 0x1ab54
    void Swap(UOpenGL& context);        // 0x1abdc: draws the frame over the back buffer before a swap
    void DrawOverlay();

    struct Pictures;                    // platform/cursor.cpp
    Pictures* mPictures = nullptr;
    int32_t mHotX = 0, mHotY = 0;
};

// The device an OpenGL context is made for (EngineStartup asks for 1000).
enum DeviceKind : int32_t {};

// UOpenGL (UVersion.cpp range): an AGL OpenGL context and its pixel format. EngineStartup
// asks for 4 bits each of red, green, blue and alpha (AGL attributes 8, 10, 9, 11) and
// writes three fields directly (Mac offsets; the platform layer gives them meaning).
class UOpenGL {
public:
    static UOpenGL* GetCurrentInstance();
    void Swap();
    int32_t GetMaxTextures(int32_t, int32_t, int32_t);
    void AddAttribute(int32_t attribute, int32_t value);   // 0x10017418

    uint8_t mF00[4];
    uint8_t mF04;            // 0x04  EngineStartup: 0
    uint8_t mF05;
    uint8_t mF06;            // 0x06  EngineStartup: 1
    uint8_t mF07[0x4c - 0x07];
    int32_t mF4C;            // 0x4c  EngineStartup: 16
    uint8_t mF50[0x1d8 - 0x50];
    uint32_t mVideoMemory;   // 0x1d8 bytes; EngineStartup_Final uses the small textures at 8 MB or less
};

class ULogManager {
public:
    ULogManager();
    void SetContext(const char*, const char*, int32_t, uint32_t);
    void Log(const char*, ...);
    uint8_t mOpaque[0x1c];
};
extern ULogManager* gLogger;
class UAllocator {
public:
    UAllocator();
    uint8_t mOpaque[8];
    void Clear(void* dest, uint32_t size);   // sams utils 0x100009a0
};
extern UAllocator* gMemory;
// The allocator of short-lived buffers (sams utils shared; the main program's pointer to it is the import gMemoryTemporary, a
// null pointer until the first user makes one with the constructor: operator new(8) and the constructor). The Mac object is 8
// bytes, a word at +0 and a pointer at +4 to the table the game calls through: the entry at +0xc answers a buffer of the given
// size (NULL when it cannot), the entry at +0x14 releases the buffer the reference names and clears the reference. Both entries
// take the allocator as `this`. PostcardBitmap::copy_from_opengl (src/game/ui/Postcard.cpp) uses them for its 320 x 240 x 4 buffer.
// Host: the two entries are virtual members of the class itself (platform/sams_core.cpp: new[] and delete[]).
class UAllocatorTemporary {
public:
    UAllocatorTemporary();
    virtual ~UAllocatorTemporary();
    virtual uint8_t* Allocate(uint32_t size);   // table entry +0xc
    virtual void Free(uint8_t*& block);         // table entry +0x14
};
extern UAllocatorTemporary* gMemoryTemporary;   // import gMemoryTemporary (sams utils shared)
class UMemoryAlignedBase {
public:
    // `count` elements of `elementSize` bytes each (sams utils 0x100113f8).
    UMemoryAlignedBase(uint32_t count, uint32_t alignment, uint32_t elementSize, UAllocator*);
    ~UMemoryAlignedBase();
    uint32_t mF00;
    void* mData; // 0x04 on the Mac
    uint32_t mF08, mF0C;
};

// UThread (UThread.cpp): cooperative threads.
class UThread {
public:
    static void Yield();   // lets the other threads run
};

// 64-bit microseconds (stored high word first on the Mac).
struct UMicroseconds {
    int64_t mValue;
};

class UTimer {
public:
    // Free-running cycle counter (64-bit, returned in r3:r4 on the PowerPC).
    static uint64_t GetAbsoluteCycles();
    // Absolute time in microseconds (64-bit; returned through a hidden 8-byte buffer on
    // the PowerPC). The engine turns it into milliseconds with an unsigned 64-bit divide.
    static uint64_t GetAbsolute();
    // 0x1000cfe8: the timer's period; `start` starts it.
    void SetRate(const UMicroseconds& rate, bool start);

    int64_t mRate = 0;      // platform state (the Mac object is 0x14 bytes)
    int64_t mNextTick = 0;
    bool mRunning = false;
};

// UDialog (UInterrupts.cpp range): a modal dialog from a 'DLOG' resource, 0x28 bytes on
// the Mac. Filter is called for every event while Show runs; returning true ends the
// dialog with `itemHit`.
class UDialog {
public:
    UDialog(const UResourceId& id, bool movable);   // 0x10019118
    virtual ~UDialog();                              // 0x100191f8
    virtual bool Filter(const UEvent& event, int16_t& itemHit);  // 0x10019360
    void Show();                                     // 0x10019f40
    void SetParamText(const UString& p0, const UString& p1, const UString& p2,
                      const UString& p3);            // 0x10019a2c
    void SetItemTitle(int32_t item, const UString& title);  // 0x10019c20

    int32_t mDefaultItem;   // 0x18, the item Filter reports when it ends the dialog itself
};

// UGamework (UGamework.cpp): the application object. Run calls CheckFeatures and
// Initialise, then, while mRunning is set, processes the pending events (dispatched to
// the Handle* slots) and calls Process once per pass; Process returning false or
// mRunning cleared ends it, and Shutdown follows. Virtual slots in the Mac vtable order.
class UGamework {
public:
    explicit UGamework(int32_t menuBarId);   // 0x100040bc
    virtual ~UGamework();                    // 0x10004288
    void Run();                              // 0x1000438c

    virtual bool Initialise();                                         // 0x10
    virtual void Shutdown();                                           // 0x14
    virtual bool Process();                                            // 0x1c
    virtual void HandleCommand(int32_t command, uint32_t modifiers);    // 0x44, 0x10005a64
    virtual void HandleMouseMoved(const UEvent& event);                // 0x4c
    virtual void HandleMouseUp(const UEvent& event, MouseButton button);    // 0x50
    virtual void HandleMouseDown(const UEvent& event, MouseButton button);  // 0x54
    virtual void HandleMouseWheel(const UEvent& event, int32_t delta);      // 0x58
    virtual void HandleTyping(char character, char keyCode, uint32_t modifiers, bool down);  // 0x64
    virtual void HandleUpdateWindow(GrafPort* window);                       // 0x6c
    virtual void HandleActivateWindow(GrafPort* window, bool active);        // 0x70

    bool mRunning;     // 0x04: Run loops while set
    UString mName;     // 0x20: its text pointer (0x28) is published by ThemeParkWorld
    UTimer mTimer;     // 0x5c
    int32_t mValue70;  // 0x70: zeroed by the constructor, 500000 from ThemeParkWorld
};

}  // namespace SamsUtilities
