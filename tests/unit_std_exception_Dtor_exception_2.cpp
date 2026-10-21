// Test library for the main app's std::exception destructor (unit v5710; libtpw_unit_std_exception_Dtor_exception_2.dylib).
//   src/game/MslSupport.cpp   TPMainExc::ExceptionDtor
//
// operator delete (glue 0x10238218) is a recording fake; the vtable global gMslExcVtblC is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPMain.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblC[16];   // 0x10278bc0
}

namespace TPMainExc {
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
}

extern "C" {

uint64_t tpw_exc_addr(const char* name) {
    if (std::strcmp(name, "vtbl") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(IME::gMslExcVtblC));
    return 0;
}

uint64_t tpw_exc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_exc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TPMainExc::Exception));
    return n;
}

uint32_t tpw_exc_dtor(uint64_t self, int32_t flag) {
    TPMainExc::Exception* r = TPMainExc::ExceptionDtor(reinterpret_cast<TPMainExc::Exception*>(self), static_cast<int16_t>(flag));
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
