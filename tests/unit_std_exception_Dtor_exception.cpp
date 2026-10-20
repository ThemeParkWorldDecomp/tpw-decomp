// Test library for std::exception::~exception (unit v5125; libtpw_unit_std_exception_Dtor_exception.dylib), main app:
//   src/ltms_textformatter_std_exception_Dtor_exception.cpp   LtmsStdExceptionDtor, gLtmsStdExceptionVtbl
//
// operator delete is a fake that records the pointer and frees nothing.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

extern const void* gLtmsStdExceptionVtbl[2];
void* LtmsStdExceptionDtor(void* self, int16_t deleteFlag);

struct LtmsStdException {
    static void operator delete(void* object);
};
void LtmsStdException::operator delete(void* p) { ev("OperatorDelete", {lab(p)}); }

extern "C" {

uint64_t tpw_stdexc_addr(const char* name) {
    if (std::strcmp(name, "vtbl") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gLtmsStdExceptionVtbl));
    return 0;
}

uint64_t tpw_stdexc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_stdexc_layout(int64_t* out) {
    (void)out;
    return 0;
}

uint64_t tpw_stdexc_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LtmsStdExceptionDtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag))));
}

}  // extern "C"
