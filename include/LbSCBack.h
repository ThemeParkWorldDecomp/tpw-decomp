#pragma once
// LbSCBack.cpp of the bullfrog library (unit v5284): window and screen-saver helpers.
#include <cstddef>
#include <cstdint>

struct GrafPort;

namespace NS_MacDoze {
struct InternalHandle;
// A Windows MSG (28 bytes); only its address is used here.
struct tagMSG { uint8_t mBytes[28]; };
struct _devicemodeA;
InternalHandle* CreateDC(const char* driver, const char* device, const char* output, const _devicemodeA* mode);
int32_t GetDeviceCaps(InternalHandle* dc, int32_t index);
int32_t DeleteDC(InternalHandle* dc);
int32_t DispatchMessage(const tagMSG* msg);
int32_t TranslateMessage(const tagMSG* msg);
int32_t PeekMessage(tagMSG* msg, GrafPort* window, uint32_t min, uint32_t max, uint32_t remove);
void InitializeCriticalSection(void** section);
int32_t SystemParametersInfoA(uint32_t action, uint32_t param, void* data, uint32_t winIni);
}  // namespace NS_MacDoze

// The screen mode the display reports.
struct TbScreenMode {
    int32_t mWidth;    // 0x00 (HORZRES)
    int32_t mHeight;   // 0x04 (VERTRES)
    int32_t mDepth;    // 0x08 (BITSPIXEL)
};

// A TbError-like result returned through a hidden pointer (r3 on the Mac).
struct LbAppResult { int32_t mCode; };

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count);
extern "C" void __register_global_object(void* object, void* destructor, void* node);

// Not translated yet (empty stand-ins, defined in LbFile_LbApp_GetWindowsScreenMode.cpp).
void LbSEFunctionList_Construct(void* self);        // TbPtrList<_LbSEFunction>::TbPtrList 0x100144f0
void LbSEFunctionList_Destruct(void* self);         // TbPtrList<_LbSEFunction>::~TbPtrList 0x10014450
void LbSystemEventCallback_Destruct(void* self);    // _LbSystemEventCallback::~_LbSystemEventCallback 0x10014564

// The global at 0x10073e0c: six lists of 16 bytes, then the critical section handle.
struct LbSCBackGlobals {
    uint8_t mLists[0x60];       // 0x00
    void** mCriticalSection;    // 0x60 (a Mac pointer, 4 bytes)
    uint8_t mNode[16];          // 0x64 registration node (0x10073e70)
};
extern LbSCBackGlobals gLbSCBack;   // 0x10073e0c
extern char gLbWindowName[64];      // 0x10072600

LbAppResult LbApp_SetWindowName(const char* name);   // 0x10014278
LbAppResult LbApp_EnableWindowsScreenSaver(int32_t enable);   // 0x100141a8
void LbApp_ProcessWindowMessages();     // 0x10014018
void LbApp_GetWindowsScreenMode(TbScreenMode& mode);   // 0x10014374
void __sinit_LbSCBack_cpp();     // 0x10014b4c

// Unit v5461 (src/LbFile_LbApp_GetWindowsScreenMode_LbSystemEventCallback_Dtor__LbSystemEventCallbac.cpp).
#include "TbTList.h"
class _LbSystemEventCallback;
// One registered callback. The Mac's callback is a pointer to a TVector (called through __ptr_glue); the host keeps a function pointer.
class _LbSEFunction {
public:
    void* mContext;   // 0x00 (passed as the third argument)
    void (*mCallback)(int32_t type, void* data, void* context);   // 0x04
};
// The object at 0x10073e0c as the host builds it: six lists of 0x10 bytes on the Mac (indexed by event type), then the critical section.
struct LbSystemEventCallbackBody {
    TbPtrList<_LbSEFunction> mLists[6];   // 0x00, 0x10 each on the Mac
    void** mCriticalSection;              // 0x60
};
extern uint8_t kLbSEListDtorVector[];     // 0x100700c4: the TVector of TbPtrList<_LbSEFunction>::~TbPtrList
// Weak: the list's RemoveHead (bullfrog 0x10014a78); a test library replaces it with a fake.
_LbSEFunction* LbSEList_RemoveHead(TbPtrList<_LbSEFunction>* list);
// Weak: operator delete (glue 0x1006b2d4); a test library replaces it with a recording fake.
void LbSE_OperatorDelete(void* block);
namespace NS_MacDoze { void DeleteCriticalSection(void** section); }   // glue 0x1006c0b4
extern "C" void __destroy_arr(void* array, void* destructor, int32_t size, int32_t count);   // glue 0x1006c0cc
// _LbSystemEventCallback::~_LbSystemEventCallback 0x10014564 (flag: the hidden delete-storage short); answers this.
_LbSystemEventCallback* LbSystemEventCallback_DtorBody(_LbSystemEventCallback* self, int16_t flag);

// Unit v5732 (src/LbFile_LbApp_GetWindowsScreenMode_LbSystemEventCallback_Dtor__LbSystemEventCallbac_CritLock_Dtor_CritLock.cpp).
class CritLock {
public:
    void** mSection;   // 0x00 (a Mac pointer, 4 bytes)
};
// CritLock::~CritLock 0x10014738 (flag: the hidden delete-storage short); answers this.
CritLock* CritLock_DtorBody(CritLock* self, int16_t flag);

// Unit v51127 (src/LbFile_LbApp_GetWindowsScreenMode_LbApp_SetWindowIcon.cpp).
struct TbPoint;
class CConverter;
class TbJPEGLoader;
class _LbSemaLock;
class _LbSemaphore;
extern void* gLbAppUserWndProc;    // 0x10073dfc
extern void* gLbAppHWnd;           // 0x10073e00
extern void* gLbAppHInstance;      // 0x10073e04
extern int32_t gLbAppCloseDown;    // 0x10073e08
// The hidden result pointer (r3) gets 0; the icon (a halfword) goes to gLbWindowIcon (0x10073dfa).
LbAppResult LbApp_SetWindowIcon(int16_t icon);   // 0x10014238
void LbApp_SetUserWndProc(void* proc);           // 0x10014138
int32_t LbApp_GetCloseDownRequest();             // 0x100140c0
void LbApp_SetHInstance(void* instance);         // 0x10013f7c
TbPtrList<_LbSEFunction>* TbPtrList_LbSEFunction_Ctor(TbPtrList<_LbSEFunction>* self);   // 0x100144f0
// TbTList<_LbSEFunction*,_LbSEFunction*>: ~TbTList 0x1001491c (flag: the hidden delete-storage short), RemoveAll 0x100149c8 and
// RemoveHead 0x10014a78 are explicit specialisations of the members; the constructor is 0x100148b8 (outside the unit).
TbTList<_LbSEFunction*, _LbSEFunction*>* TbTListLbSEFunction_DtorBody(TbTList<_LbSEFunction*, _LbSEFunction*>* self, int16_t flag);
CConverter* CConverter_DtorBody(CConverter* self, int16_t flag);   // 0x10019358
namespace SamsUtilities {
namespace UBinary { uint32_t RotateLeft_ulong(uint32_t value, int32_t amount); }   // 0x10019f3c
void USwapInplace32_ulong_10009d44(uint32_t& value);
void USwapInplace32_ulong_100445f0(uint32_t& value);
void USwapBlock32_ulong_10009ce4(uint32_t* words, int32_t count);
void USwapBlock32_ulong_10044590(uint32_t* words, int32_t count);
}
// The base loader's constructor 0x10022870 and destructor 0x100228e0 as callees (weak; tests fake them).
void TbJPEGLoader_BaseCtor(void* self);
void TbJPEGLoader_BaseDtor(void* self, int16_t flag);
TbJPEGLoader* TbJPEGLoader_CtorBody(TbJPEGLoader* self);                 // 0x10024228
TbJPEGLoader* TbJPEGLoader_DtorBody(TbJPEGLoader* self, int16_t flag);   // 0x10024290
void _LbSemaLock_Construct(_LbSemaLock* self, _LbSemaphore* semaphore, int32_t wait);   // 0x10024ac4: a bare blr
void _LbSemaLock_Release(_LbSemaLock* self);                                              // 0x10024b9c: a bare blr
void TbPoint_Construct(TbPoint* self, int32_t x, int32_t y);                              // 0x100591c4
