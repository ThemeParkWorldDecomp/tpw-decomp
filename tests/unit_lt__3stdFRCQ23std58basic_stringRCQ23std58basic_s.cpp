// Test library for std::operator< on nocase wide strings (unit v5756; tests/test_lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s.py), main app:
//   src/lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s.cpp   NocaseWStringLess
//
// size, begin and compare are recording fakes that answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace {
alignas(16) uint8_t gObjA[16];
alignas(16) uint8_t gObjB[16];
}  // namespace

uint32_t online_lt::NocaseWStringSize(const void* self) {
    ev("Size", {lab(self)});
    return static_cast<uint32_t>(ret("Size"));
}

const char16_t* online_lt::NocaseWStringBegin(const void* self) {
    ev("Begin", {lab(self)});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("Begin"))));
}

int32_t online_callee::NocaseWStringCompare(const void* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    ev("Compare", {lab(self), pos, n1, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s))), n2});
    return static_cast<int32_t>(ret("Compare"));
}

extern "C" {

uint64_t tpw_lt_addr(const char* name) {
    if (std::strcmp(name, "A") == 0) return reinterpret_cast<uintptr_t>(gObjA);
    if (std::strcmp(name, "B") == 0) return reinterpret_cast<uintptr_t>(gObjB);
    return 0;
}

uint32_t tpw_lt_less(uint64_t a, uint64_t b) {
    return NocaseWStringLess(reinterpret_cast<const void*>(static_cast<uintptr_t>(a)),
                             reinterpret_cast<const void*>(static_cast<uintptr_t>(b))) ? 1u : 0u;
}

}  // extern "C"
