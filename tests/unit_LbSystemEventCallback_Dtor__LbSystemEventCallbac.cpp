// Test library for the system event callback object (unit v5461; libtpw_unit_LbSystemEventCallback_Dtor__LbSystemEventCallbac.dylib), main app:
//   src/LbFile_LbApp_GetWindowsScreenMode_LbSystemEventCallback_Dtor__LbSystemEventCallbac.cpp
//
// The callees outside the unit (critical section calls, operator delete, __destroy_arr, the list's RemoveHead, the registered callback) are
// fakes that record what the PowerPC stubs record.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbSCBack.h"
#include "WinScrn.h"
#include "platform/macdoze.h"

using namespace calllog;

// operator delete is a recording no-op.
void LbSE_OperatorDelete(void* pointer) { ev("delete", {lab(pointer)}); }

namespace NS_MacDoze {
void EnterCriticalSection(void** handle) { ev("Enter", {lab(handle)}); }
void LeaveCriticalSection(void** handle) { ev("Leave", {lab(handle)}); }
void DeleteCriticalSection(void** handle) { ev("DeleteCS", {lab(handle)}); }
}  // namespace NS_MacDoze

extern "C" void __destroy_arr(void* array, void* destructor, int32_t size, int32_t count) {
    ev("destroy_arr", {lab(array), destructor == static_cast<void*>(kLbSEListDtorVector) ? 1 : 0, size, count});
}

// The list's RemoveHead: only the count moves; the answer is the case's queue value (a raw address).
_LbSEFunction* LbSEList_RemoveHead(TbPtrList<_LbSEFunction>* list) {
    ev("RemoveHead", {lab(list)});
    if (list->mCount > 0) --list->mCount;
    return reinterpret_cast<_LbSEFunction*>(static_cast<uintptr_t>(ret("RemoveHead")));
}

namespace {
void HostCallback(int32_t type, void* data, void* context) {
    ev("ptr_glue", {type, static_cast<int64_t>(reinterpret_cast<uintptr_t>(data) & 0xffffffffu),
                    static_cast<int64_t>(reinterpret_cast<uintptr_t>(context) & 0xffffffffu)});
}
}  // namespace

extern "C" {

uint64_t tpw_lbse_addr(const char* name) {
    if (std::strcmp(name, "callback") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&HostCallback));
    return 0;
}

uint64_t tpw_lbse_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lbse_layout(int64_t* out) {
    int32_t n = 0;
    alignas(8) char buf[sizeof(TbPtrList<_LbSEFunction>)];
    auto* list = reinterpret_cast<TbPtrList<_LbSEFunction>*>(buf);
    out[n++] = reinterpret_cast<char*>(&list->mHead) - buf;
    out[n++] = reinterpret_cast<char*>(&list->mTail) - buf;
    out[n++] = reinterpret_cast<char*>(&list->mCount) - buf;
    out[n++] = static_cast<int64_t>(sizeof(TbPtrList<_LbSEFunction>));
    out[n++] = static_cast<int64_t>(sizeof(TbPtrList<_LbSEFunction>::CNode));
    out[n++] = static_cast<int64_t>(offsetof(LbSystemEventCallbackBody, mCriticalSection));
    out[n++] = static_cast<int64_t>(sizeof(LbSystemEventCallbackBody));
    out[n++] = static_cast<int64_t>(offsetof(_LbSEFunction, mCallback));
    out[n++] = static_cast<int64_t>(sizeof(_LbSEFunction));
    return n;
}

uint64_t tpw_lbse_dtor(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        LbSystemEventCallback_DtorBody(reinterpret_cast<_LbSystemEventCallback*>(static_cast<uintptr_t>(self)), flag)));
}

void tpw_lbse_event(uint64_t self, int32_t type, uint32_t data) {
    reinterpret_cast<_LbSystemEventCallback*>(static_cast<uintptr_t>(self))
        ->EventNotification(type, reinterpret_cast<void*>(static_cast<uintptr_t>(data)));
}

void tpw_lbse_deleteall(uint64_t self) {
    reinterpret_cast<_LbSystemEventCallback*>(static_cast<uintptr_t>(self))->DeleteAllCallbacks();
}

}  // extern "C"
