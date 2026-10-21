// Test library for ui_IME.cpp's static initialiser (unit v5768; libtpw_unit_sinit_ui_IME_cpp.dylib, tests/test_sinit_ui_IME_cpp.py), main app:
//   src/game/ui/ui_IME.cpp   __sinit_ui_IME_cpp, gUIIMESinitDtorTV, gUIIMESinitNode
//
// IME::CIME::CIME and __register_global_object are recording fakes; gUIIME is defined here with empty destructors.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"
#include "AdvisorScoring.h"

using namespace calllog;

CUIIME gUIIME;

namespace IME {
CIME::CIME() { ev("CIME", {lab(this)}); }
MslWideString::MslWideString() {}   // a member of CIME; the fake CIME() constructs it
MslWideString::~MslWideString() {}
CIME::~CIME() {}
list_cand_data_t::~list_cand_data_t() {}
}  // namespace IME
CUIIME::~CUIIME() {}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobal", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_ime_addr(const char* name) {
    if (std::strcmp(name, "gUIIME") == 0) return reinterpret_cast<uintptr_t>(&gUIIME);
    if (std::strcmp(name, "dtor") == 0) return reinterpret_cast<uintptr_t>(gUIIMESinitDtorTV);
    if (std::strcmp(name, "node") == 0) return reinterpret_cast<uintptr_t>(gUIIMESinitNode);
    return 0;
}

int32_t tpw_ime_layout(int64_t* out) {
    int32_t n = 0;
    const size_t cand = offsetof(CUIIME, mCandidates);
    out[n++] = offsetof(CUIIME, mOwner);
    out[n++] = offsetof(CUIIME, mF08);
    out[n++] = offsetof(CUIIME, mActive);
    out[n++] = offsetof(CUIIME, mF0D) + 3;
    out[n++] = cand + offsetof(IME::list_cand_data_t, mF00);
    out[n++] = cand + offsetof(IME::list_cand_data_t, mStrings) + offsetof(MslVector<IME::MslWideString>, mCapacity);
    out[n++] = cand + offsetof(IME::list_cand_data_t, mStrings) + offsetof(MslVector<IME::MslWideString>, mSize);
    out[n++] = cand + offsetof(IME::list_cand_data_t, mStrings) + offsetof(MslVector<IME::MslWideString>, mData);
    out[n++] = offsetof(CUIIME, mIME);
    out[n++] = sizeof(CUIIME);
    return n;
}

void tpw_ime_sinit() { __sinit_ui_IME_cpp(); }

}  // extern "C"
