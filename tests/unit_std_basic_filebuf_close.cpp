// Test library for std::basic_filebuf<wchar_t> close, underflow, pbackfail, imbue (unit v5137;
// libtpw_unit_std_basic_filebuf_close.dylib, tests/test_std_basic_filebuf_close.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace {
uint64_t gImpl, gRc, gFacet;  // what the Getloc / UseFacet fakes hand out (set per case; full host addresses)
uint64_t gHigh;               // the high half of the arena addresses (the Ptr32 fields hold the low half)

const void* P(uint32_t w) { return w ? reinterpret_cast<const void*>(gHigh | w) : nullptr; }
}  // namespace

namespace filebuf_wseek_callee {
bool Unshift(BasicFilebufWchar* fb) {
    ev("Unshift", {static_cast<int64_t>(lab(fb))});
    return (ret("Unshift") & 0xff) != 0;
}
void Delete(void* p) { ev("Delete", {static_cast<int64_t>(lab(p))}); }
}  // namespace filebuf_wseek_callee

namespace filebuf_char_callee {
int32_t Fclose(uint32_t file) {
    ev("fclose", {static_cast<int64_t>(file)});
    return static_cast<int32_t>(ret("fclose"));
}
int32_t CodecvtAlwaysNoconv(void* cvt) {
    ev("Noconv", {static_cast<int64_t>(lab(cvt))});
    return static_cast<int32_t>(ret("nc"));
}
}  // namespace filebuf_char_callee

namespace streambuf_callee {
void LocaleAssign(void* dst, const void* src) {
    ev("LocaleAssign", {static_cast<int64_t>(lab(dst)), static_cast<int64_t>(lab(src))});
}
}  // namespace streambuf_callee

namespace filebuf_wclose_callee {
int32_t VirtualUnderflow(void* fb) {
    ev("VUnderflow", {static_cast<int64_t>(lab(fb))});
    return static_cast<int32_t>(ret("vu"));
}
void Getloc(void* out, void* streambuf) {
    ev("Getloc", {static_cast<int64_t>(lab(streambuf))});
    uint32_t* w = static_cast<uint32_t*>(out);
    w[0] = static_cast<uint32_t>(gImpl);
    w[1] = static_cast<uint32_t>(gRc);
}
void* UseFacetCodecvtW(void* locale) {
    const uint32_t* w = static_cast<const uint32_t*>(locale);
    ev("UseFacet", {static_cast<int64_t>(lab(P(w[0]))), static_cast<int64_t>(lab(P(w[1])))});
    return reinterpret_cast<void*>(gFacet);
}
void LocaleImpDtor(void* impl, int32_t flag) {
    ev("LocaleDtor", {static_cast<int64_t>(lab(impl)), static_cast<int64_t>(flag)});
}
}  // namespace filebuf_wclose_callee

extern "C" {

uint64_t tpw_wcl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_wcl_setup(uint64_t impl, uint64_t rc, uint64_t facet, uint64_t high) {
    gImpl = impl;
    gRc = rc;
    gFacet = facet;
    gHigh = high;
}

int32_t tpw_wcl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFlag4c));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mUnbuffered));
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufWchar));
    return n;
}

uint64_t tpw_wcl_close(uint64_t o) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<BasicFilebufWchar*>(o)->close()));
}
uint32_t tpw_wcl_underflow(uint64_t o) { return reinterpret_cast<BasicFilebufWchar*>(o)->underflow(); }
uint32_t tpw_wcl_pbackfail(uint64_t o, uint32_t c) {
    return reinterpret_cast<BasicFilebufWchar*>(o)->pbackfail(static_cast<char16_t>(c));
}
void tpw_wcl_imbue(uint64_t o, uint64_t loc) {
    reinterpret_cast<BasicFilebufWchar*>(o)->imbue(reinterpret_cast<const void*>(loc));
}

}  // extern "C"
