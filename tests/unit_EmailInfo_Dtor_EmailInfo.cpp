// Test library for EmailInfo::~EmailInfo (unit v5711; libtpw_unit_EmailInfo_Dtor_EmailInfo.dylib, tests/test_EmailInfo_Dtor_EmailInfo.py), main app:
//   src/email_load_EmailInfo_Dtor_EmailInfo.cpp
//
// The callees (string destructor, list clear, operator delete) are fakes that record the offset of the address they get from the object.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

namespace {
const void* gThis = nullptr;
int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p) - reinterpret_cast<intptr_t>(gThis)); }
}  // namespace

namespace email_callee {
void StringDtor(void* str, int16_t flag) { ev("StringDtor", {Off(str), static_cast<int64_t>(flag)}); }
void ListClear(void* list) { ev("ListClear", {Off(list)}); }
void OperatorDelete(void* p) { ev("OperatorDelete", {Off(p)}); }
}  // namespace email_callee

extern "C" {

uint64_t tpw_ei_addr(const char*) { return 0; }

uint64_t tpw_ei_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ei_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(EmailInfo, mString04));
    out[n++] = static_cast<int64_t>(offsetof(EmailInfo, mList08));
    out[n++] = static_cast<int64_t>(offsetof(EmailInfo, mString14));
    return n;
}

uint64_t tpw_ei_dtor(uint64_t self, int32_t flag) {
    gThis = reinterpret_cast<const void*>(static_cast<uintptr_t>(self));
    EmailInfo* r = EmailInfo::Destruct(reinterpret_cast<EmailInfo*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
