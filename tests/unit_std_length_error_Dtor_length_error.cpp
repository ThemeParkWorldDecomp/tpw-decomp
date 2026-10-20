// Test library for std::length_error::~length_error (unit v5749; libtpw_unit_std_length_error_Dtor_length_error.dylib), main app:
//   src/ltms_textformatter_std_length_error_Dtor_length_error.cpp
//
// Callees: the message's destructor and operator delete are recording fakes (nothing is freed: the arena owns the memory).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbTextFormatter.h"

using namespace calllog;

// The runtime's operator delete (0x1000f694), a recording fake that replaces the weak definition in the src file; nothing is freed.
void LengthErrorOperatorDelete(void* p) { ev("delete", {lab(p)}); }

void MslRefCountedCharsDtor(MslRefCountedCharsBlock* self, int16_t deleteFlag) {
    ev("RcDtor", {lab(self), deleteFlag});
}


extern "C" {

uint64_t tpw_lerr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"length", gLengthErrorVtbl},
        {"logic", gLogicErrorVtbl},
        {"exception", gExceptionVtbl},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lerr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(std::calloc(1, size))); }

int32_t tpw_lerr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(LengthError, mMessage));
    return n;
}

uint64_t tpw_lerr_dtor(uint64_t self, int32_t flag) {
    return reinterpret_cast<uintptr_t>(LengthErrorDtor(reinterpret_cast<LengthError*>(self), static_cast<int16_t>(flag)));
}

// The vtable word of the object, as the address it holds.
uint64_t tpw_lerr_vptr(uint64_t self) { return reinterpret_cast<uintptr_t>(reinterpret_cast<LengthError*>(self)->mVtable); }

}  // extern "C"
