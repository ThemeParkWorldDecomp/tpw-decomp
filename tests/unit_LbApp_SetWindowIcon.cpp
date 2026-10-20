// Test library for the small bullfrog functions of unit v51127 (libtpw_unit_LbApp_SetWindowIcon.dylib, tests/test_LbApp_SetWindowIcon.py), bullfrog:
//   src/LbFile_LbApp_GetWindowsScreenMode_LbApp_SetWindowIcon.cpp
//
// Callees outside the unit are recording fakes: operator delete (LbSE_OperatorDelete), the TbTList constructor, CConverter::Release,
// the loader base constructor and destructor, and the platform's _USwapBlock32. gLbWindowIcon is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "WinScrn.h"
#include "lobby.h"
#include "platform/samsutils.h"

using namespace calllog;

int16_t gLbWindowIcon;

// The unit's list members are specialisations defined in the src file (declared before any use, as there).
template <> TbTList<_LbSEFunction*, _LbSEFunction*>::TbTList();
template <> TbTList<_LbSEFunction*, _LbSEFunction*>::~TbTList();
template <> void TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveAll();
template <> _LbSEFunction* TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveHead();

// ---- the fakes
void LbSE_OperatorDelete(void* block) { ev("delete", {lab(block)}); }

template <>
TbTList<_LbSEFunction*, _LbSEFunction*>::TbTList() {
    ev("TbTList", {lab(this)});
    mHead = nullptr;
    mTail = nullptr;
    mCount = 0;
}

void CConverter::Release() { ev("Release", {lab(this)}); }

void TbJPEGLoader_BaseCtor(void* self) { ev("BaseCtor", {lab(self)}); }
void TbJPEGLoader_BaseDtor(void* self, int16_t flag) { ev("BaseDtor", {lab(self), flag}); }

namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) { ev("USwapBlock32", {lab(words), count}); }
}  // namespace SamsUtilities

static uint32_t P(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
template <class T> static T* A(uint64_t a) { return reinterpret_cast<T*>(static_cast<uintptr_t>(a)); }
typedef TbTList<_LbSEFunction*, _LbSEFunction*> LbList;

extern "C" {

uint64_t tpw_lbapp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"icon", &gLbWindowIcon},
        {"wndproc", &gLbAppUserWndProc},
        {"hwnd", &gLbAppHWnd},
        {"hinst", &gLbAppHInstance},
        {"close", &gLbAppCloseDown},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lbapp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Host layout of the list: vptr, then three members.
int32_t tpw_lbapp_layout(int64_t* out) {
    typedef TbTList<_LbSEFunction*, _LbSEFunction*> List;
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(List, mHead));
    out[n++] = static_cast<int64_t>(offsetof(List, mTail));
    out[n++] = static_cast<int64_t>(offsetof(List, mCount));
    out[n++] = static_cast<int64_t>(offsetof(List::CNode, mNext));
    out[n++] = static_cast<int64_t>(offsetof(List::CNode, mPrev));
    out[n++] = static_cast<int64_t>(sizeof(List::CNode));
    return n;
}

typedef TbTList<_LbSEFunction*, _LbSEFunction*> LbList;

int32_t tpw_lbapp_SetWindowIcon(int32_t icon, uint64_t result) {
    LbAppResult r = LbApp_SetWindowIcon(static_cast<int16_t>(icon));
    *A<int32_t>(result) = r.mCode;
    return 0;
}
uint32_t tpw_lbapp_GetUserWndProc() { return P(reinterpret_cast<const void*>(LbApp_GetUserWndProc())); }
void tpw_lbapp_SetUserWndProc(uint32_t v) { LbApp_SetUserWndProc(A<void>(v)); }
void tpw_lbapp_SetCloseDownRequest(int32_t v) { LbApp_SetCloseDownRequest(v); }
int32_t tpw_lbapp_GetCloseDownRequest() { return LbApp_GetCloseDownRequest(); }
void tpw_lbapp_SetHWnd(uint32_t v) { LbApp_SetHWnd(A<void>(v)); }
uint32_t tpw_lbapp_GetHWnd() { return P(LbApp_GetHWnd()); }
void tpw_lbapp_SetHInstance(uint32_t v) { LbApp_SetHInstance(A<void>(v)); }
uint32_t tpw_lbapp_GetHInstance() { return P(LbApp_GetHInstance()); }

uint32_t tpw_lbapp_PtrListCtor(uint64_t self) { return P(TbPtrList_LbSEFunction_Ctor(A<TbPtrList<_LbSEFunction>>(self))); }
uint32_t tpw_lbapp_ListDtor(uint64_t self, int32_t flag) { return P(TbTListLbSEFunction_DtorBody(A<LbList>(self), static_cast<int16_t>(flag))); }
void tpw_lbapp_RemoveAll(uint64_t self) { A<LbList>(self)->RemoveAll(); }
uint32_t tpw_lbapp_RemoveHead(uint64_t self) { return P(A<LbList>(self)->RemoveHead()); }
uint32_t tpw_lbapp_ConverterDtor(uint64_t self, int32_t flag) { return P(CConverter_DtorBody(A<CConverter>(self), static_cast<int16_t>(flag))); }
uint32_t tpw_lbapp_RotateLeft(uint32_t value, int32_t amount) { return SamsUtilities::UBinary::RotateLeft_ulong(value, amount); }
void tpw_lbapp_Inplace_10009d44(uint64_t p) { SamsUtilities::USwapInplace32_ulong_10009d44(*A<uint32_t>(p)); }
void tpw_lbapp_Inplace_100445f0(uint64_t p) { SamsUtilities::USwapInplace32_ulong_100445f0(*A<uint32_t>(p)); }
void tpw_lbapp_Block_10009ce4(uint64_t p, int32_t n) { SamsUtilities::USwapBlock32_ulong_10009ce4(A<uint32_t>(p), n); }
void tpw_lbapp_Block_10044590(uint64_t p, int32_t n) { SamsUtilities::USwapBlock32_ulong_10044590(A<uint32_t>(p), n); }
uint32_t tpw_lbapp_JpegCtor(uint64_t self) { return P(TbJPEGLoader_CtorBody(A<TbJPEGLoader>(self))); }
uint32_t tpw_lbapp_JpegDtor(uint64_t self, int32_t flag) { return P(TbJPEGLoader_DtorBody(A<TbJPEGLoader>(self), static_cast<int16_t>(flag))); }
void tpw_lbapp_SemaCtor(uint64_t self, uint64_t sema, int32_t wait) { _LbSemaLock_Construct(A<_LbSemaLock>(self), A<_LbSemaphore>(sema), wait); }
void tpw_lbapp_SemaRelease(uint64_t self) { _LbSemaLock_Release(A<_LbSemaLock>(self)); }
void tpw_lbapp_PointCtor(uint64_t self, int32_t x, int32_t y) { TbPoint_Construct(A<TbPoint>(self), x, y); }

}  // extern "C"
