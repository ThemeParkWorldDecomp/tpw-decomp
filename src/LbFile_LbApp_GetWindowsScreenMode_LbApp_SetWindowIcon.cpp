// Unit v51127: small bullfrog functions (LbSCBack.cpp application globals, the _LbSEFunction list instances, CConverter and TbJPEGLoader
// destructors, SamsUtilities swap helpers, TbPoint, _LbSemaLock stand-ins). Mac addresses are in the comments.
#include <cstring>
#include <new>

#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "WinScrn.h"
#include "lobby.h"
#include "platform/samsutils.h"

// The list instance of this unit: the members below are explicit specialisations (the header's generic bodies would call the plain
// operator delete). Declared before any use.
typedef TbTList<_LbSEFunction*, _LbSEFunction*> LbSEList;
template <> LbSEList::TbTList();
template <> LbSEList::~TbTList();
template <> void LbSEList::RemoveAll();
template <> _LbSEFunction* LbSEList::RemoveHead();

// TbTList<_LbSEFunction*,_LbSEFunction*>::TbTList 0x100148b8 is outside the unit: a weak stand-in (the generic body) that a test replaces.
template <>
__attribute__((weak, noinline)) LbSEList::TbTList() : mHead(nullptr), mTail(nullptr), mCount(0) {}

// The application globals (TOC offsets 0x505c..0x5068 from the data base; the icon is gLbWindowIcon, defined elsewhere).
void* gLbAppUserWndProc;     // 0x10073dfc
void* gLbAppHWnd;            // 0x10073e00
void* gLbAppHInstance;       // 0x10073e04
int32_t gLbAppCloseDown;     // 0x10073e08

LbAppResult LbApp_SetWindowIcon(int16_t icon) {   // 0x10014238
    gLbWindowIcon = icon;
    LbAppResult result;
    result.mCode = 0;
    return result;
}
LbUserWndProc LbApp_GetUserWndProc() { return reinterpret_cast<LbUserWndProc>(gLbAppUserWndProc); }   // 0x10014170
void LbApp_SetUserWndProc(void* proc) { gLbAppUserWndProc = proc; }                                  // 0x10014138
void LbApp_SetCloseDownRequest(int32_t request) { gLbAppCloseDown = request; }                       // 0x100140fc
int32_t LbApp_GetCloseDownRequest() { return gLbAppCloseDown; }                                      // 0x100140c0
void LbApp_SetHWnd(void* hwnd) { gLbAppHWnd = hwnd; }                                                // 0x10013fe4
void* LbApp_GetHWnd() { return gLbAppHWnd; }                                                         // 0x10013fb4
void LbApp_SetHInstance(void* instance) { gLbAppHInstance = instance; }                              // 0x10013f7c
void* LbApp_GetHInstance() { return gLbAppHInstance; }                                               // 0x10013f44

// TbPtrList<_LbSEFunction>::TbPtrList 0x100144f0: the base constructor, then the list's vtable.
TbPtrList<_LbSEFunction>* TbPtrList_LbSEFunction_Ctor(TbPtrList<_LbSEFunction>* self) {
    return new (self) TbPtrList<_LbSEFunction>();
}

// TbTList<_LbSEFunction*,_LbSEFunction*>: the destructor (0x1001491c), RemoveAll (0x100149c8) and RemoveHead (0x10014a78).
template <>
void TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveAll() {
    CNode* node = mHead;
    mTail = nullptr;
    mHead = nullptr;
    while (node != nullptr) {
        CNode* next = node->mNext;
        LbSE_OperatorDelete(node);
        node = next;
    }
    mCount = 0;
}

template <>
_LbSEFunction* TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveHead() {
    CNode* node = mHead;
    if (node == nullptr) return nullptr;
    mHead = node->mNext;
    if (mHead == nullptr)
        mTail = nullptr;
    else
        mHead->mPrev = nullptr;
    int32_t count = mCount;
    _LbSEFunction* data = node->mData;
    mCount = count - 1;
    LbSE_OperatorDelete(node);
    return data;
}

template <>
TbTList<_LbSEFunction*, _LbSEFunction*>::~TbTList() {
    RemoveAll();
}

// The destructor with the hidden delete-storage flag; answers this.
TbTList<_LbSEFunction*, _LbSEFunction*>* TbTListLbSEFunction_DtorBody(TbTList<_LbSEFunction*, _LbSEFunction*>* self, int16_t flag) {
    if (self != nullptr) {
        self->TbTList<_LbSEFunction*, _LbSEFunction*>::~TbTList();
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}

// CConverter::~CConverter 0x10019358.
CConverter* CConverter_DtorBody(CConverter* self, int16_t flag) {
    if (self != nullptr) {
        self->Release();
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
void USwapBlock32_ulong_10044590(uint32_t* words, int32_t count) { _USwapBlock32(words, count); }
}  // namespace SamsUtilities

// TbGraphicFileLoader::TbGraphicFileLoader 0x10022870 / ~TbGraphicFileLoader 0x100228e0 as callees (weak: a test replaces them).
__attribute__((weak, noinline)) void TbJPEGLoader_BaseCtor(void* self) { new (self) TbGraphicFileLoader(); }
__attribute__((weak, noinline)) void TbJPEGLoader_BaseDtor(void* self, int16_t flag) {
    (void)flag;
    static_cast<TbGraphicFileLoader*>(self)->~TbGraphicFileLoader();
}

// TbJPEGLoader::TbJPEGLoader 0x10024228 (the Mac then stores the loader's vtable; the host loader has none).
TbJPEGLoader* TbJPEGLoader_CtorBody(TbJPEGLoader* self) {
    TbJPEGLoader_BaseCtor(self);
    return self;
}

// TbJPEGLoader::~TbJPEGLoader 0x10024290: the base destructor with flag 0, then the delete when flag > 0.
TbJPEGLoader* TbJPEGLoader_DtorBody(TbJPEGLoader* self, int16_t flag) {
    if (self != nullptr) {
        TbJPEGLoader_BaseDtor(self, 0);
        if (flag > 0) LbSE_OperatorDelete(self);
    }
    return self;
}

// _LbSemaLock::_LbSemaLock(_LbSemaphore*, int) 0x10024ac4 and _LbSemaLock::Release 0x10024b9c: both are a bare blr.
void _LbSemaLock_Construct(_LbSemaLock* self, _LbSemaphore* semaphore, int32_t wait) {
    (void)self;
    (void)semaphore;
    (void)wait;
}
void _LbSemaLock_Release(_LbSemaLock* self) { (void)self; }

// TbPoint::TbPoint(int, int) 0x100591c4.
void TbPoint_Construct(TbPoint* self, int32_t x, int32_t y) {
    self->x = x;
    self->y = y;
}
