// LbApp.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AudioPlaceHolder.h"
#include "bfl_FileSystem.h"
#include "bfl_Refpack.h"
#include "DrawState.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "LbSurface.h"
#include "lobby.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "TbFPUPrecision.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "WinScrn.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

// Bullfrog library, LbSCBack.cpp range: remaining file wrappers 0x10000048..0x10001e34,
// LbMem_ReAlloc 0x10013dec, plus the out-of-line copies of TbError's inline members:
// 0x10001564 TbError(long) 8 B.
// 0x100014b8 TbError(const TbError&) 12 B.
// 0x100014f0 operator long() 8 B. src/LbFile.cpp, src/LbFile_stamp.cpp and
// src/LbTime.cpp hold the rest of the range.

// 0x10013dec (140)
void* LbMem_ReAlloc(void* block, uint32_t oldSize, uint32_t newSize) {
    void* p = LbMem_Alloc(newSize);
    if (p != nullptr) {
        std::memcpy(p, block, oldSize < newSize ? oldSize : newSize);
        LbMem_Free(block);
    }
    return p;
}

// Bullfrog's project paths and file wrappers (LbSCBack.cpp in the symbol inventory).

void* LbMem_Alloc(uint32_t size) {
    return size == 0 ? nullptr : ::operator new[](size);
}

void LbMem_Free(void* block) {
    ::operator delete[](block);
}

// Unit v51127: small bullfrog functions (LbSCBack.cpp application globals, the _LbSEFunction list instances, CConverter and TbJPEGLoader
// destructors, SamsUtilities swap helpers, TbPoint, _LbSemaLock stand-ins). Mac addresses are in the comments.

// The list instance of this unit: the members below are explicit specialisations (the header's generic bodies would call the plain
// operator delete). Declared before any use.
typedef TbTList<_LbSEFunction*, _LbSEFunction*> LbSEList;
template <> LbSEList::TbTList();
template <> LbSEList::~TbTList();
template <> void LbSEList::RemoveAll();
template <> _LbSEFunction* LbSEList::RemoveHead();
// The application globals (TOC offsets 0x505c..0x5068 from the data base; the icon is gLbWindowIcon, defined elsewhere).
void* gLbAppUserWndProc;     // 0x10073dfc
void* gLbAppHWnd;            // 0x10073e00
void* gLbAppHInstance;       // 0x10073e04
int32_t gLbAppCloseDown;     // 0x10073e08

void* LbApp_GetHInstance() { return gLbAppHInstance; }                                               // 0x10013f44

void LbApp_SetHInstance(void* instance) { gLbAppHInstance = instance; }                              // 0x10013f7c
void* LbApp_GetHWnd() { return gLbAppHWnd; }                                                         // 0x10013fb4
void LbApp_SetHWnd(void* hwnd) { gLbAppHWnd = hwnd; }                                                // 0x10013fe4

// LbSCBack.cpp of the bullfrog library: window name, screen saver, message pump, screen mode (unit v5284).

// The string table at 0x1006c689: "Bullfrog" followed by "DISPLAY" (at +9).
static const char kBullfrogStrings[] = "Bullfrog\0DISPLAY";

char gLbWindowName[64] = "Bullfrog";   // 0x10072600
// Empty host adapters for __construct_array: the list bodies are TbPtrList_LbSEFunction_Ctor and _DtorBody (units v51127, v5L21),
// but the host's gLbSCBack keeps the Mac's 0x10-byte stride, which a host TbPtrList does not fit.
void LbSEFunctionList_Construct(void*) {}
void LbSEFunctionList_Destruct(void*) {}
void LbSystemEventCallback_Destruct(void*) {}

// 0x10014018: drains up to 2001 pending messages.
void LbApp_ProcessWindowMessages() {
    NS_MacDoze::tagMSG msg;
    int32_t budget = 2000;
    for (;;) {
        if (NS_MacDoze::PeekMessage(&msg, nullptr, 0, 0, 1) == 0) break;
        bool last = (budget == 0);
        budget--;
        if (last) break;
        NS_MacDoze::TranslateMessage(&msg);
        NS_MacDoze::DispatchMessage(&msg);
    }
}

int32_t LbApp_GetCloseDownRequest() { return gLbAppCloseDown; }                                      // 0x100140c0
void LbApp_SetCloseDownRequest(int32_t request) { gLbAppCloseDown = request; }                       // 0x100140fc
void LbApp_SetUserWndProc(void* proc) { gLbAppUserWndProc = proc; }                                  // 0x10014138
LbUserWndProc LbApp_GetUserWndProc() { return reinterpret_cast<LbUserWndProc>(gLbAppUserWndProc); }   // 0x10014170

// 0x100141a8: the result is 0 when the call succeeds, -1 otherwise.
LbAppResult LbApp_EnableWindowsScreenSaver(int32_t enable) {
    LbAppResult result;
    if (NS_MacDoze::SystemParametersInfoA(0x11, static_cast<uint32_t>(enable), nullptr, 0) != 0)
        result.mCode = 0;
    else
        result.mCode = -1;
    return result;
}

LbAppResult LbApp_SetWindowIcon(int16_t icon) {   // 0x10014238
    gLbWindowIcon = icon;
    LbAppResult result;
    result.mCode = 0;
    return result;
}
// TbPtrList<_LbSEFunction>::TbPtrList 0x100144f0: the base constructor, then the list's vtable.
TbPtrList<_LbSEFunction>* TbPtrList_LbSEFunction_Ctor(TbPtrList<_LbSEFunction>* self) {
    return new (self) TbPtrList<_LbSEFunction>();
}

// The destructor with the hidden delete-storage flag; answers this.
TbTList<_LbSEFunction*, _LbSEFunction*>* TbTListLbSEFunction_DtorBody(TbTList<_LbSEFunction*, _LbSEFunction*>* self, int16_t flag) {
    if (self != nullptr) {
        self->TbTList<_LbSEFunction*, _LbSEFunction*>::~TbTList();
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}

// TbPtrList<_LbSEFunction>::~TbPtrList 0x10014450 (unit v5L21): the list's vtable, the base destructor with no delete, then the
// storage when the flag is positive; answers this.
TbPtrList<_LbSEFunction>* TbPtrList_LbSEFunction_DtorBody(TbPtrList<_LbSEFunction>* self, int16_t flag) {
    if (self != nullptr) {
        TbTListLbSEFunction_DtorBody(self, 0);
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}

// SamsUtilities::UBinary::RotateLeft<unsigned long> 0x10019f3c: rotlw.
namespace SamsUtilities {
namespace UBinary {
uint32_t RotateLeft_ulong(uint32_t value, int32_t amount) {
    uint32_t n = static_cast<uint32_t>(amount) & 31u;
    return (value << n) | (value >> ((32u - n) & 31u));
}
}  // namespace UBinary

// 0x10009d44 and 0x100445f0: lwz, stwbrx.
void USwapInplace32_ulong_10009d44(uint32_t& value) { value = __builtin_bswap32(value); }
void USwapInplace32_ulong_100445f0(uint32_t& value) { value = __builtin_bswap32(value); }
// 0x10009ce4 and 0x10044590: the platform's _USwapBlock32.
void USwapBlock32_ulong_10009ce4(uint32_t* words, int32_t count) { _USwapBlock32(words, count); }
}  // namespace SamsUtilities

// 0x10014278: a NULL name gives "Bullfrog"; the copy is cut at 64 bytes (no terminator then).
LbAppResult LbApp_SetWindowName(const char* name) {
    if (name != nullptr)
        strncpy(gLbWindowName, name, 0x40);
    else
        strncpy(gLbWindowName, kBullfrogStrings, 0x40);
    LbAppResult result;
    result.mCode = 0;
    return result;
}

// bullfrog TbSystemInfo.cpp, LbDraw.cpp, WinScrn.cpp, LbMaths.cpp, TbJPEGSaver.cpp, Draw.cpp, LbSCBack.cpp small functions (unit v51071).

int16_t LbApp_GetWindowIcon() { return gLbWindowIcon; }   // 0x10014308
// ---- LbSCBack.cpp
const char* LbApp_GetWindowName() { return gLbWindowName; }   // 0x10014340

// 0x10014374
void LbApp_GetWindowsScreenMode(TbScreenMode& mode) {
    NS_MacDoze::InternalHandle* dc = NS_MacDoze::CreateDC(kBullfrogStrings + 9, nullptr, nullptr, nullptr);
    if (dc != nullptr) {
        mode.mDepth = NS_MacDoze::GetDeviceCaps(dc, 12);
        mode.mWidth = NS_MacDoze::GetDeviceCaps(dc, 8);
        mode.mHeight = NS_MacDoze::GetDeviceCaps(dc, 10);
        NS_MacDoze::DeleteDC(dc);
    }
}
