// Mouse.cpp (main app): CMouse, the park's mouse modes. PARTIAL; untranslated.
#pragma once
#include <cstdint>

// Unit w143: forward declarations only (the full types are in CellBase.h, ESP_sprites.h and analyser.h).
enum CELL_MODES : int32_t;
enum ESP_Sprites : int32_t;
struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;

class CMouseMode;

class CMouse {
public:
    // Inline in the original (GAMESYSTEM_Tick's delete expands to this call).
    ~CMouse() { AssignMouseMode(nullptr); }
    void AssignMouseMode(CMouseMode* mode);   // 0x1007e2e0
    void ImmediateMouseHandler(int32_t x, int32_t y);   // 0x1007e094 (untranslated; r3 is gMouse)
    // Unit w142: 0x1007e248 (52 bytes): hands the double click to the current mode.
    void DoubleClick(int32_t x, int32_t y);   // 0x1007e248
    // 0x1007de84 (untranslated). No this: r3 is the key, 0xf0 (left) or 0xf1.
    static void ProcessIngameMouse(int32_t key, int32_t down, int32_t x, int32_t y, void* data);

    int32_t mF00;
};
extern CMouse* gMouse;   // 0x102dc1f4, original name unknown

// ---- the mouse mode (unit w101; declarations only, the classes are untranslated)
// The Mac's vtable of CMMDefault (0x1029d5f4) is two header words and then, from +0x08: the destructor, OnLeftClick, OnLeftRelease,
// OnRightClick, OnRightRelease, OnDoubleClick, OnMouseMove, WhileLeftDown, WhileRightDown, GetMode (+0x2c), OnAssign (+0x30), OnRelease
// (+0x34). The base class (vtable 0x1029d6b0) has bodies of 4 bytes (a bare return) for the clicks, releases, moves and OnRelease, so
// those are written empty here; its GetMode slot (+0x2c) is empty (pure here, so the class stays abstract) and its OnAssign (0x1007e454, a
// 4 byte blr) is a real empty body, as are the ten other bodies and the destructor 0x1007e484 (unit w144: all eleven stay inline here, so
// that the class has no key function and the launcher needs no vtable of it). The arguments are the mangled
// names' (positions in the UI's units). UIMain_SetPanel calls GetMode (slot +0x2c) of the current mode and compares the answer with 9.
class CMouseMode {
public:
    virtual ~CMouseMode() = default;                                   // 0x1007e484
    virtual void OnLeftClick(int32_t x, int32_t y) {}                  // 0x1007e060
    virtual void OnLeftRelease(int32_t x, int32_t y) {}                // 0x1007e028
    virtual void OnRightClick(int32_t x, int32_t y) {}                 // 0x1007dff4
    virtual void OnRightRelease(int32_t x, int32_t y) {}               // 0x1007dfbc
    virtual void OnDoubleClick(int32_t x, int32_t y) {}                // 0x1007e2a8
    virtual void OnMouseMove(int32_t a, int32_t b, int32_t c, int32_t d) {}   // 0x1007e210
    virtual void WhileLeftDown(int32_t x, int32_t y) {}                // 0x1007e1d8
    virtual void WhileRightDown(int32_t x, int32_t y) {}               // 0x1007e1a0
    virtual int32_t GetMode() = 0;                                     // vtable +0x2c
    virtual void OnAssign() {}                                         // vtable +0x30 (0x1007e454, 4 bytes: a bare return)
    virtual void OnRelease() {}                                        // 0x1007e4f8
    int32_t mWord4 = 0;   // +4 on the Mac; the default constructor sets it to 1
};

// The mouse's default mode: 8 bytes on the Mac (the vtable word and a word set to 1 by the constructor 0x1007ea78, 36 bytes). Held by pointer
// only (UIMain_SetPanel makes one); the constructor and the overrides below are untranslated.
class CMMDefault : public CMouseMode {
public:
    CMMDefault();                          // 0x1007ea78
    ~CMMDefault() override;                // 0x100813e8
    void OnLeftClick(int32_t x, int32_t y) override;    // 0x1007eac4 (4 bytes: nothing)
    void OnRightClick(int32_t x, int32_t y) override;   // 0x1007eaf8 (4 bytes: nothing)
    int32_t GetMode() override;            // 0x10081468: answers 1
    void OnAssign() override;              // 0x1007eb2c (60 bytes: UIInterface::CheckMouseContext(.., 1) and a trace line)
};

// Three words of CMouse's data (original names unknown). The first two are set to -1 right after a CMouse is made (UIMain_SetPanel);
// the third is the current mouse mode, set by CMouse::AssignMouseMode.
extern int32_t gMouseWord1E0;      // 0x102dc1e0
extern int32_t gMouseWord1E4;      // 0x102dc1e4
extern CMouseMode* gMouseMode;     // 0x102dc1f0
// Unit w142: the two button flags (names invented): ProcessIngameMouse sets them, ImmediateMouseHandler reads them.
extern int32_t gMouseLeftDown;     // 0x102dc1ec (invented name)
extern int32_t gMouseRightDown;    // 0x102dc1e8 (invented name)

// Online replacement of the default mode (8 bytes on the Mac); src/Mouse_modes.cpp, unit w144.
class CMMOnline : public CMouseMode {
public:
    CMMOnline();                                           // 0x1007faac
    ~CMMOnline() override;                                 // 0x100811a8
    void OnLeftClick(int32_t x, int32_t y) override;       // 0x1007fb2c
    void OnRightClick(int32_t x, int32_t y) override;      // 0x1007fbd8
    void OnDoubleClick(int32_t x, int32_t y) override;     // 0x1007fc0c
    int32_t GetMode() override;                            // 0x10081228: 7
    void OnAssign() override;                              // 0x1007fcb8
};

// ---- Unit w143 (src/Mouse_staff.cpp): the sprite placing mode and the two staff modes. Every member name is invented.
// CSpritePlaceMode (vtable 0x1029d678, 0xc bytes): the base of both staff modes. +4 is the base's word, +8 the handle of the script that
// draws the staff member under the pointer (0 for none). GetMode is pure here: the class is abstract (the Mac slot is empty).
class CSpritePlaceMode : public CMouseMode {
public:
    ~CSpritePlaceMode() override;                                        // 0x1007ec14
    void OnMouseMove(int32_t a, int32_t b, int32_t c, int32_t d) override;   // 0x1007e528 (388 bytes)
    void OnRelease() override;                                           // 0x10081550
    int32_t mScript = 0;   // +8  the script handle (SCR_*)
};
// CMMPlaceStaff (vtable 0x1029d5a8, 0x1c bytes): places a staff member just hired. The 0x1c bytes are the Mac's; the host object is larger.
class CMMPlaceStaff : public CSpritePlaceMode {
public:
    CMMPlaceStaff(int32_t model, uint8_t payGrade, uint8_t flag, int32_t value);   // 0x1007eb94
    ~CMMPlaceStaff() override;                                           // 0x10081320
    void OnLeftClick(int32_t x, int32_t y) override;                     // 0x1007ef80 (1,208 bytes)
    int32_t GetMode() override;                                          // 0x100813b0: 5
    void OnAssign() override;                                            // 0x1007ec9c (492 bytes)
    void OnRelease() override;                                           // 0x1007eeb8
    int32_t mModel;              // +0x0c  (CThing::ThingModel on the Mac, an int32 enum) the staff kind (4 mechanic, 5 handyman, 6 entertainer, 7 guard, 8 researcher)
    uint8_t mPayGrade;           // +0x10
    uint8_t mF11;                // +0x11  handed on to the script (ESP_SpawnScript's third argument) and to the constructor
    int32_t mF14;                // +0x14
    uint8_t mPlaced;             // +0x18  1 once OnLeftClick placed the staff member
};
// CMMPickUpStaff (vtable 0x1029d554, 0x14 bytes): carries a staff member picked up in the park.
class CMMPickUpStaff : public CSpritePlaceMode {
public:
    CMMPickUpStaff(ESP_Sprites sprite, uint8_t flag);                    // 0x1007f46c
    ~CMMPickUpStaff() override;                                          // 0x10081258
    void OnLeftClick(int32_t x, int32_t y) override;                     // 0x1007f58c (904 bytes)
    void OnRightClick(int32_t x, int32_t y) override;                    // 0x1007fa74 (4 bytes: nothing)
    int32_t GetMode() override;                                          // 0x100812e8: 6
    void OnAssign() override;                                            // 0x1007f4d8 (132 bytes)
    void OnRelease() override;                                           // 0x1007f948 (252 bytes)
    ESP_Sprites mSprite;   // +0x0c
    uint8_t mF10;          // +0x10
};
// CMMPathDraw (vtable 0x1029d640, 0x14 bytes): the path drawing mode. declared by unit w143 for ContextMouseClick, which makes
// one; translated by unit w144 (src/Mouse_modes.cpp).
class CMMPathDraw : public CMouseMode {
public:
    explicit CMMPathDraw(CELL_MODES mode);                                                          // 0x1007e6e4 (44 bytes)
    CMMPathDraw(CELL_MODES mode, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id);              // 0x1007e744 (56 bytes)
    ~CMMPathDraw() override;                                             // 0x1008149c
    void OnLeftClick(int32_t x, int32_t y) override;                     // 0x1007e7e0
    void OnLeftRelease(int32_t x, int32_t y) override;                   // 0x1007e890
    void OnRightClick(int32_t x, int32_t y) override;                    // 0x1007e944
    void OnMouseMove(int32_t a, int32_t b, int32_t c, int32_t d) override;   // 0x1007e9ec
    void WhileLeftDown(int32_t x, int32_t y) override;                   // 0x1007e97c
    void WhileRightDown(int32_t x, int32_t y) override;                  // 0x1007e9b4
    int32_t GetMode() override;                                          // 0x1008151c: 3
    void OnAssign() override;                                            // 0x10080ee4
    CELL_MODES mCellMode;   // +8   (invented name)
    int32_t mF0C;           // +0xc  0 for the first constructor, 1 for the second
    uint16_t mObjectId;     // +0x10 the object id (the second constructor only)
};

// ---- Unit w144 (src/Mouse_modes.cpp): CMMCoaster and CMMFirstPersonTarget, and the globals of the Mouse.cpp data.
// CMMCoaster (vtable 0x1029d4a0, 8 bytes on the Mac): the roller coaster building mode.
class CMMCoaster : public CMouseMode {
public:
    CMMCoaster();                                          // 0x1007fd14 (28 bytes)
    ~CMMCoaster() override;                                // 0x100810f4
    void OnLeftClick(int32_t x, int32_t y) override;       // 0x1007fd58 (40 bytes): CoasterAcceptLeftMouseClick(1)
    void OnLeftRelease(int32_t x, int32_t y) override;     // 0x1007fdb0 (40 bytes): CoasterAcceptLeftMouseClick(0)
    void OnRightClick(int32_t x, int32_t y) override;      // 0x1007fe0c (4 bytes: nothing)
    int32_t GetMode() override;                            // 0x10081174: 8
    void OnAssign() override;                              // 0x1007fe40 (4 bytes: nothing)
};
// CMMFirstPersonTarget (vtable 0x1029d454, 8 bytes on the Mac): picks the ride (or the walk around) to ride in the first person.
class CMMFirstPersonTarget : public CMouseMode {
public:
    CMMFirstPersonTarget();                                // 0x1007fe70 (28 bytes)
    ~CMMFirstPersonTarget() override;                      // 0x1008102c
    void OnMouseMove(int32_t a, int32_t b, int32_t c, int32_t d) override;   // 0x1007fec0 (416 bytes)
    void OnLeftClick(int32_t x, int32_t y) override;       // 0x1008009c (3,432 bytes; deletes this on the paths that hand the mouse over)
    void OnRightClick(int32_t x, int32_t y) override;      // 0x10080e40 (4 bytes: nothing)
    int32_t GetMode() override;                            // 0x100810b8: 9
    void OnAssign() override;                              // 0x10080e80 (44 bytes): the cursor 0x13
};
// The two words __sinit_Mouse_cpp 0x100815c8 sets (Mouse.cpp's BSS after gMouse; meaning unknown, names invented): 0x102dc1f8 = 0x10000 and
// 0x102dc1fc = 0. The host initialises them at load time.
extern int32_t gMouseWord1F8;   // 0x102dc1f8
extern int32_t gMouseWord1FC;   // 0x102dc1fc
// The null cell id of the pointer (a const halfword of the image at 0x1029d432, 0) and of LLCR_OverRide's answer (0x1029d436, 0). Names invented.
extern const uint16_t gPointerNullId;    // 0x1029d432
extern const uint16_t gOverRideNullId;   // 0x1029d436
