// Test library for the static initialiser of AdvisorScoring.cpp (unit v5018; libtpw_unit_sinit_AdvisorScoring_cpp.dylib,
// tests/test_sinit_AdvisorScoring_cpp.py), main app:
//   src/sinit_AdvisorScoring_cpp.cpp   __sinit_AdvisorScoring_cpp, the string storage and node, DestroyAdvisorString
//
// The string constructor and __register_global_object are fakes that record what the real ones read. The scorer sources and the
// message table are defined here (other files define them in the program).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"

using namespace calllog;

AdvisorScorer gAdvisorScorerSources[kAdvisorMessageCount];
AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2];

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    const auto label = static_cast<int64_t>(newlab(this));
    ev("StringCtor", {label});
    ev("StringText", {text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7]});
}
template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobalObject", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sources", gAdvisorScorerSources},
        {"table", &gAdvisorMessageBlock[1]},
        {"node", gAdvisorStringNode},
        {"dtor", reinterpret_cast<const void*>(&DestroyAdvisorString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(AdvisorMessageRecord));
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageRecord, mScorer));
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageRecord, mMaxDuplicates));
    out[n++] = static_cast<int64_t>(sizeof(AdvisorScorer));
    return n;
}

void tpw_sinit_run() { __sinit_AdvisorScoring_cpp(); }

}  // extern "C"
