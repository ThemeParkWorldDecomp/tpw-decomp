// Test library for Argument::Argument and Argument::~Argument (unit v5138; libtpw_unit_Argument_Argument.dylib,
// tests/test_Argument_Argument.py), main app: src/email_Argument_Argument.cpp.
// The callees (string constructors and destructors, wide string copy, operator delete) are fakes that record their arguments;
// pointers are recorded as the offset from the test's block so both sides log the same numbers.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "online.h"

using namespace calllog;

namespace {
uintptr_t gBase = 0;
int64_t Rel(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - gBase)); }
}  // namespace

namespace online_callee {
void StringCtor(void* self) { ev("StringCtor", {Rel(self)}); }
void StringDtor(void* self, int16_t flag) { ev("StringDtor", {Rel(self), static_cast<int64_t>(flag)}); }
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {Rel(self), static_cast<int64_t>(flag)}); }
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {Rel(self), Rel(other), static_cast<int64_t>(pos), static_cast<int64_t>(count)});
}
void OperatorDelete(void* p) { ev("OperatorDelete", {Rel(p)}); }
}  // namespace online_callee

extern "C" {

uint64_t tpw_arg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_arg_base(uint64_t base) { gBase = static_cast<uintptr_t>(base); }

int32_t tpw_arg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Argument));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mWString));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mCommand));
    return n;
}

uint64_t tpw_arg_ctor(uint64_t self, uint64_t text) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Argument::Construct(reinterpret_cast<Argument*>(self), reinterpret_cast<const void*>(text))));
}

uint64_t tpw_arg_dtor(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Argument::Destruct(reinterpret_cast<Argument*>(self), flag)));
}

}  // extern "C"
