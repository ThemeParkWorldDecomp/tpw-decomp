// Test library for TPGlobals::~TPGlobals (unit v5692; libtpw_unit_TPGlobals_Dtor_TPGlobals.dylib), main app:
//   src/TPGlobals_Dtor_TPGlobals.cpp   TPGlobals::~TPGlobals
//
// The string destructors are fakes that record the member offset (this - object base). The deleting variant's flag is
// done by the wrapper: a NULL object does nothing, a flag > 0 as a short records the "delete" event.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPGlobals.h"

using namespace calllog;

namespace {
uintptr_t gBase = 0;
// The member index: the host's strings are bigger than the Mac's 20 bytes, so the log names the slot (Mac offset 0x4024 + 20 * index).
int64_t Slot(const void* p) {
    return static_cast<int64_t>((reinterpret_cast<uintptr_t>(p) - gBase - offsetof(TPGlobals, mF4024)) / sizeof(TbDynamicStringTemplate<char>));
}
}

template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("dtor_c", {Slot(this)});
}

template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {
    ev("dtor_w", {Slot(this)});
}

// Only so the vtable the fake destructor stores links; never called.
#define TPG_VTABLE_FILLER(CH)                                                                              \
    template <>                                                                                           \
    void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                            \
    template <>                                                                                           \
    void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                           \
    template <>                                                                                           \
    void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                               \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) {        \
        return *this;                                                                                     \
    }                                                                                                     \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate&) { \
        return *this;                                                                                     \
    }                                                                                                     \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) {                      \
        return *this;                                                                                     \
    }                                                                                                     \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) {                      \
        return *this;                                                                                     \
    }                                                                                                     \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) {       \
        return *this;                                                                                     \
    }                                                                                                     \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) {                     \
        return *this;                                                                                     \
    }                                                                                                     \
    template <>                                                                                           \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) {                     \
        return *this;                                                                                     \
    }

TPG_VTABLE_FILLER(char16_t)
TPG_VTABLE_FILLER(char)

extern "C" {

uint64_t tpw_tpg_addr(const char*) { return 0; }

uint64_t tpw_tpg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tpg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mF4024));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mF4C7C));
    out[n++] = static_cast<int64_t>(sizeof(TbDynamicStringTemplate<char>));
    out[n++] = static_cast<int64_t>(sizeof(TbDynamicStringTemplate<char16_t>));
    out[n++] = static_cast<int64_t>(sizeof(TPGlobals));
    return n;
}

// The object at p (already built as zeroed memory), the hidden flag as a short; answers p.
uint64_t tpw_tpg_dtor(uint64_t p, int32_t flag) {
    if (p == 0) return 0;
    TPGlobals* obj = reinterpret_cast<TPGlobals*>(static_cast<uintptr_t>(p));
    gBase = static_cast<uintptr_t>(p);
    obj->~TPGlobals();
    if (static_cast<int16_t>(flag) > 0) ev("delete", {0});
    return p;
}

}  // extern "C"
