// Mac OS Toolbox routines (InterfaceLib) that game-side code calls directly, without a
// shim. The platform layer implements them (analysis/platform-layer-design.md).
#pragma once
#include <cstdint>

// DebugStr: drops into the debugger with a Pascal string (length byte, then text). The
// platform layer is expected to log the message and continue.
void DebugStr(const unsigned char* message);

// Gestalt(OSType selector, long* response) -> OSErr (0 = noErr). The engine asks for
// 'pclk' (processor clock in Hz).
int16_t Gestalt(uint32_t selector, int32_t* response);

// BlockMoveData(const void* src, void* dest, Size count): memmove without cache flushing.
void BlockMoveData(const void* src, void* dest, int32_t count);

// QuickDraw rectangle (Mac order: top, left, bottom, right; 16-bit coordinates).
struct Rect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
};

// A window's port (WindowPtr / CGrafPort). Game code reads only portRect (Mac offset
// 0x10); the platform layer owns the real structure and may extend this one.
struct GrafPort {
    uint8_t mHeader[0x10];  // 0x00 device, portPixMap/portBits, portVersion...
    Rect portRect;          // 0x10
};

// FrontWindow() -> WindowPtr: the frontmost window (a GrafPort on the classic Mac OS).
GrafPort* FrontWindow();

// File Manager. OSErr results: 0 = noErr.
using OSErr = int16_t;

// FSSpec (packed with 68K alignment on the Mac: vRefNum 0x00, parID 0x02, name 0x06).
struct FSSpec {
    int16_t vRefNum;
    int32_t parID;
    unsigned char name[64];  // Str63
};

// The HVolumeParam fields of the HParamBlockRec that game code sets or reads (Mac
// offsets in the comments); PBHGetVInfoSync fills the volume information.
struct HVolumeParam {
    void* ioCompletion;         // 0x0c
    int16_t ioResult;           // 0x10
    unsigned char* ioNamePtr;   // 0x12
    int16_t ioVRefNum;          // 0x16
    int16_t ioVolIndex;         // 0x1c
    int16_t ioVAtrb;            // 0x26  bit 7: hardware locked, bit 15: software locked
};

// EqualString(a, b, caseSensitive, diacSensitive): two Pascal strings compared.
bool EqualString(const unsigned char* a, const unsigned char* b, bool caseSensitive, bool diacSensitive);
// c2pstr(char*): converts a C string to a Pascal string in place (returns the same
// buffer as a StringPtr).
unsigned char* c2pstr(char* text);
// FSMakeFSSpec(vRefNum, dirID, fileName (Pascal string), spec): a full path when
// vRefNum and dirID are 0 and the name starts with a volume name.
OSErr FSMakeFSSpec(int16_t vRefNum, int32_t dirID, const unsigned char* fileName, FSSpec* spec);
OSErr PBHGetVInfoSync(HVolumeParam* paramBlock);
// ResolveAliasFile(spec, resolveAliasChains, targetIsFolder, wasAliased): follows an alias.
OSErr ResolveAliasFile(FSSpec* spec, bool resolveAliasChains, bool* targetIsFolder, bool* wasAliased);

// QuickDraw and the Window Manager, as the movie player uses them: the current port, a
// window made visible, a rectangle of the current window that needs no redraw, a port's
// graphics device.
void SetPort(GrafPort* port);
void ShowWindow(GrafPort* window);
void ValidRect(const Rect* rect);
struct GDevice;
GDevice** GetGWorldDevice(GrafPort* port);

// SystemTask(): gives desk accessories and the system time to run; the engine calls it
// in its busy-wait loops (the splash screen).
void SystemTask();

// QuickDraw: SetRect(r, left, top, right, bottom).
void SetRect(Rect* rect, int16_t left, int16_t top, int16_t right, int16_t bottom);

// Window Manager: NewCWindow(storage, bounds, title (Pascal string), visible, procID, behind,
// goAwayFlag, refCon). procID 0 is a document window, 2 a plain box (the game's full-screen
// window); behind (WindowPtr)-1 puts it in front.
GrafPort* NewCWindow(void* storage, const Rect* bounds, const unsigned char* title, bool visible, int16_t procID,
                     GrafPort* behind, bool goAwayFlag, int32_t refCon);

// Resource Manager. A Handle is a pointer to a (relocatable) pointer.
using Handle = char**;
int16_t FSpOpenResFile(const FSSpec* spec, int8_t permission);   // -1 when it fails
int16_t CurResFile();
void UseResFile(int16_t refNum);
Handle GetNamedResource(uint32_t type, const unsigned char* name);
void CloseResFile(int16_t refNum);

// Unit w221 (src/bullfrog/LbFile.cpp, src/bullfrog/LbApp.cpp), InterfaceLib imports.
OSErr HGetVol(unsigned char* volName, int16_t* vRefNum, int32_t* dirID); // bullfrog glue 0x1006b55c
void UpperString(unsigned char* text, bool diacSensitive); // glue 0x1006b574
char* p2cstr(unsigned char* text); // glue 0x1006b58c
