// Unit v5041 (src/bullfrog/WinScrn.cpp), bullfrog WinScrn.cpp: DirectDraw object creation, display-mode enumeration,
// the global palette and the static initialiser of the file's surfaces. Mac offsets are in the comments.
#pragma once
#include <cstdint>

#include "LbList.h"
#include "LbPalette.h"
#include "LbSurface.h"
#include "TbColour.h"
#include "TbGraphicFile.h"
#include "platform/indirectx.h"

// One palette entry, four bytes: red, green, blue and a zero byte (the library's 256-entry TbPalette is 0x400 bytes).
struct TbPaletteEntry {
    uint8_t mRed, mGreen, mBlue, mFlags;
};

// The DirectDraw palette object. Host design: only the two members WinScrn.cpp calls, as an abstract interface
// (the Mac object is a COM object: GetEntries is the vtable slot at 0x1c, SetEntries the one at 0x24; the first argument
// after this is the flags word, always 0 here). Both answer an HRESULT, 0 for success.
class IDirectDrawPalette {
public:
    virtual int32_t GetEntries(uint32_t flags, uint32_t start, uint32_t count, TbPaletteEntry* entries) = 0;   // slot 0x1c
    virtual int32_t SetEntries(uint32_t flags, uint32_t start, uint32_t count, const TbPaletteEntry* entries) = 0;   // slot 0x24
};

// A display mode of the list EnumModesCallback builds. Mac 0x14 bytes: the list node (8), then these three words.
struct TbDisplayMode : TbListNode {
    uint32_t mWidth;      // 0x08  from the surface description's width (0x0c)
    uint32_t mHeight;     // 0x0c  from its height (0x08)
    uint32_t mBitCount;   // 0x10  from its pixel format's RGB bit count (0x54)
};
constexpr uint32_t kTbDisplayModeMacSize = 0x14;

// The static `_ScreenFPUHandler` of WinScrn.cpp. Mac 0x10 bytes: a byte at 0, the vtable pointer at 4 and three halfwords
// at 8, 0xa and 0xc (the host pointer is 8 bytes, so the host layout differs).
struct _ScreenFPUHandler {
    uint8_t mF00;              // 0x00
    const void* mVtable;       // 0x04
    uint16_t mF08, mF0A, mF0C; // 0x08, 0x0a, 0x0c
};

// bullfrog 0x1006a514, 112 B: DirectDrawCreate on `guid` into *out, then the registered window to LbApp_SetHWnd. The answer is
// 0 or -1 (the third argument is not used).
TbResult CreateDirectDrawObject(NS_MacDoze::_GUID* guid, IDirectDraw** out, void** unused);
// bullfrog 0x1006a45c, 124 B: appends a TbDisplayMode to the TbList `list` (the DirectDraw mode enumeration callback).
bool EnumModesCallback(DDSURFACEDESC* desc, void* list);
// bullfrog 0x1006a304, 180 B and 0x1006a220, 156 B.
TbPaletteEntry* LbScreen_GetPaletteEntries(TbPaletteEntry* entries, uint32_t start, uint32_t count);
void LbScreen_SetPaletteEntries(const TbPaletteEntry* entries, uint32_t start, uint32_t count);
// bullfrog 0x1006a198, 84 B.
void LbScreen_SwapClear(TbColour colour);
// bullfrog 0x1006a968, 252 B.
void __sinit_WinScrn_cpp();

// Outside the unit (recording boundaries in its tests).
void LbApp_SetHWnd(void* hwnd);          // bullfrog 0x10013fe4, 12 B
TbResult LbScreen_Swap(uint32_t flags);  // bullfrog 0x10069d4c, 632 B

extern void* gWinScrnHWnd;                       // 0x10075238 (name invented)
extern IDirectDrawPalette* gWinScrnPalette;      // 0x10075230 (name invented)
extern TbSurface* gWinScrnSurface;               // 0x10075228 (name invented)
extern TbSurface gWinScrnSurfaceA;               // 0x10075138 (name invented)
extern TbSurface gWinScrnSurfaceB;               // 0x10075188 (name invented), its mDDSurface decides whether palettes are set
extern TbSurface gWinScrnSurfaceC;               // 0x100751d8 (name invented)
extern _ScreenFPUHandler gScreenFPUHandler;      // 0x100750fc (name invented)
extern uint8_t gScreenFPUHandlerNode[];          // 0x1007510c: the registration node, 0x2c bytes
extern const void* const kScreenFPUHandlerVtable[];   // 0x10073a84
extern uint8_t kScreenFPUHandlerDtorVector[];    // 0x10070e1c: the TVector of _ScreenFPUHandler::~_ScreenFPUHandler

// Unit v5056 (src/bullfrog/WinScrn.cpp), bullfrog WinScrn.cpp.
namespace SamsUtilities { class UCursor; }
struct GrafPort;
namespace NS_MacDoze {
void* CreateWindowEx(uint32_t exStyle, const char* className, const char* windowName, uint32_t style, int32_t x, int32_t y,
                     int32_t width, int32_t height, GrafPort* parent, void* menu, void* instance, void* param);
int32_t GetSystemMetrics(int32_t index);
int32_t DefWindowProc(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam);
void SetCursor(SamsUtilities::UCursor* cursor);
}
// The system event callback object (0x10073e0c) and its notifier (bullfrog 0x1001462c).
class _LbSystemEventCallback {
public:
    void EventNotification(int32_t type, void* data);
    void DeleteAllCallbacks();   // 0x100147c8 (unit v5461): empties the six lists under the critical section
    uint8_t mPad[4];
};
typedef int32_t (*LbUserWndProc)(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam);
void LbApp_SetCloseDownRequest(int32_t request);   // 0x100140fc
void* LbApp_GetHWnd();                              // 0x10013fb4
void* LbApp_GetHInstance();                         // 0x10013f44
LbUserWndProc LbApp_GetUserWndProc();               // 0x10014170 (the Mac answers a pointer to a TVector)
const char* LbApp_GetWindowName();                  // 0x10014340
void RegisterBullfrogWindowClass();                 // 0x1006a824
TbResult CreateScreenModeList(int32_t mode, GrafPort* window, NS_MacDoze::_GUID* guid);   // 0x1006a5d8
// bullfrog 0x10068c60, 380 B: the Mac passes the result in a hidden pointer (r3).
TbResult LbScreen_Initialise(NS_MacDoze::_GUID* guid, int32_t mode, void* hwnd, void* window);
void ReleaseMode();                                 // 0x100689ac, 320 B
int32_t WindowProc(GrafPort* window, uint32_t message, uint32_t wParam, int32_t* lParam);   // 0x10068820, 348 B

extern int32_t gWinScrnInitialised;     // 0x100750f0
extern NS_MacDoze::_GUID* gWinScrnGuid; // 0x1007511c
extern int32_t gWinScrnF120;            // 0x10075120
extern int32_t gWinScrnActive;          // 0x10075124
extern NS_MacDoze::IUnknown* gWinScrnClipper;   // 0x1007522c
extern IDirectDraw* gWinScrnDirectDraw;         // 0x10075234
extern GrafPort* gWinScrnCoopWindow;            // 0x1007523c
extern char gWinScrnClassName[];                // 0x10075240
extern int32_t gWinScrnModeFlag;                // 0x10073a48
extern uint8_t gWinScrnFocus;                   // 0x10073df8
extern _LbSystemEventCallback gLbSystemEventCallback;   // 0x10073e0c

// Unit v5334 (src/bullfrog/WinScrn.cpp), bullfrog WinScrn.cpp.
namespace NS_MacDoze {
struct tagPOINT;
void ClientToScreen(GrafPort* window, tagPOINT* point);   // glue 0x1006bdcc
void DestroyWindow(GrafPort* window);                      // glue 0x1006c06c
}
// Members of the Mac's DirectDraw classes that the library header does not declare (host design: free functions).
int32_t IDirectDrawSurface_Flip(IDirectDrawSurface* self, IDirectDrawSurface* other, uint32_t flags);   // glue 0x1006bd9c
int32_t IDirectDraw_EnumDisplayModes(IDirectDraw* self, uint32_t flags, DDSURFACEDESC* desc, void* context,
                                     bool (*callback)(DDSURFACEDESC*, void*));   // glue 0x1006c054
// bullfrog 0x10069fec, 368 B: the Mac passes the result in a hidden pointer (r3). 0 or -1.
TbResult LbScreen_PartSwap(const TbPoint& at, const TbRect& rect, uint32_t flags);
// Unit v5761: bullfrog 0x10069cac, 116 B: the Mac passes the result in a hidden pointer (r3). Always 0.
TbResult LbScreen_RestoreMode();

extern int32_t gWinScrnDoubleBuffer;     // 0x10075118 (name invented): not 0 when the back buffer is a separate surface
extern int32_t gWinScrnModeGate;         // 0x10073a64 (name invented)
extern TbList gWinScrnModeList;          // 0x10075128 (name invented): the TbDisplayModes
extern int32_t gWinScrnClientOrigin[2];  // 0x10073a4c (name invented): the point ClientToScreen starts from

// Unit v5737 (src/bullfrog/WinScrn.cpp), bullfrog WinScrn.cpp.
struct TbScreenMode;
// The find state of the mode enumeration. Mac: the cursor (a TbDisplayMode node) at 0x0c; the rest is not used here.
struct TbScreenModeFind {
    uint32_t mF00, mF04, mF08;   // 0x00..0x08
    TbListNode* mCursor;         // 0x0c (host: pointer sized)
};
namespace NS_MacDoze {
int32_t MessageBoxA(GrafPort* window, const char* text, const char* caption, uint32_t flags);   // glue 0x1006be74
int32_t SetWindowPos(GrafPort* window, GrafPort* after, int32_t x, int32_t y, int32_t cx, int32_t cy, uint32_t flags);   // glue 0x1006be5c
}
int32_t IDirectDraw_FlipToGDISurface(IDirectDraw* self);   // glue 0x1006be44 (host design: free function)
void LbScreen_Terminate();                                  // 0x10068b10, 232 B
void LbApp_ShowMessage(const char* format, ...);            // 0x10068e54, 240 B
IDirectDraw* LbScreen_GetDirectDraw(int32_t addRef);        // 0x100687a8, 72 B
// The Mac passes the result in a hidden pointer (r3): 0, or -1 at the end of the list.
TbResult LbScreen_ModeFindFirst(TbScreenModeFind* find, TbScreenMode* mode);   // 0x10068f70, 100 B
TbResult LbScreen_ModeFindNext(TbScreenModeFind* find, TbScreenMode* mode);    // 0x10069024, 72 B

// Unit v51000 (src/bullfrog/WinScrn.cpp), bullfrog WinScrn.cpp.
struct DDSCAPS { uint32_t dwCaps; };   // 0x00 (the Mac's DDSCAPS is this one word)
namespace NS_MacDoze {
void Sleep(uint32_t ms);   // glue 0x1006bc1c
int32_t GetSystemPaletteEntries(InternalHandle* dc, uint32_t start, uint32_t count, TbPaletteEntry* entries);   // glue 0x1006bf7c
}
// Members of the Mac's DirectDraw classes that the library header does not declare (host design: free functions).
int32_t IDirectDraw_GetDisplayMode(IDirectDraw* self, DDSURFACEDESC* desc);   // glue 0x1006be8c
int32_t IDirectDraw_CreatePalette(IDirectDraw* self, uint32_t flags, const TbPaletteEntry* entries, IDirectDrawPalette** out,
                                  NS_MacDoze::IUnknown* outer);   // glue 0x1006bf1c
int32_t IDirectDrawSurface_GetAttachedSurface(IDirectDrawSurface* self, DDSCAPS* caps, IDirectDrawSurface** out);   // glue 0x1006bf34
int32_t IDirectDrawSurface_GetDC(IDirectDrawSurface* self, NS_MacDoze::InternalHandle** dc);       // glue 0x1006bf64
int32_t IDirectDrawSurface_ReleaseDC(IDirectDrawSurface* self, NS_MacDoze::InternalHandle* dc);    // glue 0x1006bf94
int32_t IDirectDrawSurface_SetPalette(IDirectDrawSurface* self, IDirectDrawPalette* palette);      // glue 0x1006bfac
// bullfrog 0x10069100, 2928 B: the Mac passes the result in a hidden pointer (r3), the parameters start at r4.
TbResult LbScreen_SetMode(uint32_t width, uint32_t height, uint32_t bpp, uint32_t flags, const TbPalette* palette);

// Unit v5963 (src/bullfrog/TbSystemInfo.cpp, src/bullfrog/FileSystem.cpp, src/bullfrog/WinScrn.cpp, src/bullfrog/LbRect.cpp), bullfrog WinScrn.cpp, TbSystemInfo.cpp, LbDraw.cpp.
#include "bullfrog_CPU.h"
namespace NS_MacDoze {
// WNDCLASSA. The Mac struct is 0x28 bytes (ten words: style 0, wndproc 4 (a TVector), extra 8, 0xc, instance 0x10, icon 0x14,
// cursor 0x18, brush 0x1c, menu 0x20, class name 0x24); the host struct has pointer sized members.
struct tagWNDCLASSA {
    uint32_t style;                 // 0x00
    LbUserWndProc lpfnWndProc;      // 0x04
    int32_t cbClsExtra;             // 0x08
    int32_t cbWndExtra;             // 0x0c
    void* hInstance;                // 0x10
    void* hIcon;                    // 0x14
    void* hCursor;                  // 0x18
    void* hbrBackground;            // 0x1c
    const char* lpszMenuName;       // 0x20
    const char* lpszClassName;      // 0x24
};
void* LoadIcon(void* instance, const char* name);   // glue 0x1006bfdc (name: MAKEINTRESOURCE)
void* GetStockObject(int32_t index);                // glue 0x1006bff4
int32_t RegisterClass(const tagWNDCLASSA* wc);      // glue 0x1006c00c
}
int16_t LbApp_GetWindowIcon();                      // 0x10014308, 12 B
extern uint8_t gRegisterBullfrogWindowClassGuard;   // 0x100750f4 (name invented): the function's static init guard
extern int32_t gRegisterBullfrogWindowClassDone;    // 0x100750f8 (name invented): its "registered" static
// _ScreenFPUHandler::~_ScreenFPUHandler 0x1006aa8c, 124 B (flag: the hidden delete-storage short); answers this.
_ScreenFPUHandler* ScreenFPUHandler_Dtor(_ScreenFPUHandler* self, int16_t flag);
extern const void* const kScreenFPUHandlerFlagVtable[];   // 0x10073a90 (name invented): the TbFPUFlag vtable the destructor stores
// Weak: operator delete (glue 0x1006b2d4); a test library replaces it with a recording fake.
void ScreenFPUHandler_OperatorDelete(void* block);
// bullfrog 0x10043d04, 72 B: detects the CPU into the global at 0x100748ac and clears four halfwords after it.
void __sinit_TbSystemInfo_cpp();
extern bullfrog::CPU gTbSystemInfoCPU;       // 0x100748ac (name invented)
extern uint16_t gTbSystemInfoF1C[4];         // 0x100748c8 (name invented)
// bullfrog 0x10029bcc, 112 B and 0x10029a04, 148 B.
bool LbRect_Contains(const TbRect* outer, const TbRect* inner);
void LbRect_Bounding(TbRect* rect, const TbRect* other);
// Weak: operator new[] / operator delete[] (glue 0x1006b334, 0x1006b364) for CRefpack::MemCompress; test libraries fake them.
uint8_t* Refpack_NewArray(uint32_t size);
void Refpack_DeleteArray(void* block);

// Unit v51071 (src/bullfrog/TbSystemInfo.cpp, src/bullfrog/LbTime.cpp), bullfrog TbSystemInfo.cpp, LbDraw.cpp,
// WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp.
struct TbSize;
void LbSystemInfo_CPUGetVendor(char* out);      // 0x10043bcc, 40 B: the 13 bytes of the CPU name
uint32_t LbSystemInfo_CPUGetFamily();           // 0x10043b7c, 32 B
uint32_t LbSystemInfo_CPUhasMMX();              // 0x10043c28, 16 B
int32_t LbSystemInfo_CPUhasRDTSC();             // 0x10043c68, 12 B
int32_t LbSystemInfo_CPUGetTickRate(uint32_t unused);   // 0x10043ca4, 44 B
void LbTime_Delay(uint32_t ms);                 // 0x10046a54, 36 B
int32_t LbSize_Area(const TbSize* size);        // 0x10029f50, 16 B
void LbSize_Set(TbSize* size, uint32_t width, uint32_t height);   // 0x10029f18, 12 B
bool LbSize_IsNull(const TbSize* size);         // 0x10029ec4, 40 B
bool LbRect_Intersects(const TbRect* a, const TbRect* b);   // 0x10029e18, 112 B
bool LbRect_IsNormal(const TbRect* rect);       // 0x10029db8, 48 B
bool LbRect_IsEmpty(const TbRect* rect);        // 0x10029c74, 48 B
void LbRect_Offset(TbRect* rect, int32_t dx, int32_t dy);   // 0x10029928, 52 B
void LbRect_SetWindow(TbRect* rect, int32_t x, int32_t y, int32_t width, int32_t height);   // 0x10029880, 28 B
void LbRect_SetFromSize(TbRect* rect, const TbSize* size);  // 0x10029824, 32 B
void LbRect_Set(TbRect* rect, int32_t left, int32_t top, int32_t right, int32_t bottom);   // 0x100297e4, 20 B
void LbPoint_Offset(TbPoint* point, int32_t dx, int32_t dy);   // 0x10029798, 28 B
void LbPoint_Set(TbPoint* point, int32_t x, int32_t y);        // 0x10029760, 12 B
void __sinit_LbMaths_cpp();                     // 0x1002971c, 28 B
void __sinit_TbJPEGSaver_cpp();                 // 0x100249c0, 56 B
void __sinit_Draw_cpp();                        // 0x100661a8, 52 B
void LbScreen_StopOpenFadePalette();            // 0x10068710, 16 B
void LbScreen_SetDirectDrawGuid(NS_MacDoze::_GUID* guid);   // 0x10068754, 12 B
int32_t LbScreen_IsInitialised();               // 0x10068c24, 12 B
int32_t LbScreen_IsActive();                    // 0x10068e20, 12 B
// IDirectDraw::WaitForVerticalBlank (glue 0x1006bd54; host design: free function).
int32_t IDirectDraw_WaitForVerticalBlank(IDirectDraw* self, uint32_t flags, NS_MacDoze::InternalHandle* handle);
extern int16_t gLbWindowIcon;                   // 0x10073dfa (name invented)
extern uint32_t gTbJPEGSaverMasks[6];           // 0x100747dc (name invented): 0xff, 0xff00, 0xff0000, 0, 0x18, 0
extern double gMathLibPi;                       // 0x1007561c: MathLib::pi (a data import)
extern uint8_t kDrawContextVtable8[];           // 0x100739a4 (name invented)
extern uint8_t kDrawContextVtable16[];          // 0x10073958
extern uint8_t kDrawContextVtable24[];          // 0x1007390c
extern uint8_t kDrawContextVtable32[];          // 0x100738c0
