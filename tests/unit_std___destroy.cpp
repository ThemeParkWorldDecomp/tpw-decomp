// Test library for std::__destroy<Argument*> / std::__destroy<Argument> (unit v5310; libtpw_unit_std___destroy.dylib, tests/test_std___destroy.py), main app:
//   src/game/MslSupport.cpp   DestroyOne, DestroyRange, DestroyRangeTagged
//
// The callees (Argument::Destruct, the string destructors) are fakes that record the object and the flag.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "online.h"

using namespace calllog;

Argument* Argument::Destruct(Argument* self, int16_t deleteFlag) {
    ev("ArgDtor", {lab(self), static_cast<int64_t>(deleteFlag)});
    return self;
}

namespace email_callee {
void StringDtor(void* str, int16_t flag) { ev("StrDtor", {lab(str), static_cast<int64_t>(flag)}); }
void WStringDtor(void* str, int16_t flag) { ev("WStrDtor", {lab(str), static_cast<int64_t>(flag)}); }
}  // namespace email_callee

extern "C" {

uint64_t tpw_sd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Argument, mString));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mWString));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mCommand));
    out[n++] = static_cast<int64_t>(sizeof(Argument));
    return n;
}

void tpw_sd_one(uint64_t p) { std_destroy::DestroyOne(reinterpret_cast<Argument*>(p)); }
void tpw_sd_range(uint64_t a, uint64_t b) { std_destroy::DestroyRange(reinterpret_cast<Argument*>(a), reinterpret_cast<Argument*>(b)); }
void tpw_sd_tag(uint64_t a, uint64_t b) { std_destroy::DestroyRangeTagged(reinterpret_cast<Argument*>(a), reinterpret_cast<Argument*>(b)); }

}  // extern "C"
