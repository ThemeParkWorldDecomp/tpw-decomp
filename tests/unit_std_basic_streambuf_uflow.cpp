// Test library for std::basic_streambuf<char> base functions (unit v5104; libtpw_unit_std_basic_streambuf_uflow.dylib), main app:
//   src/std_basic_streambuf_uflow.cpp   uflow, imbue, getloc, destructor
// The callees (underflow virtual, locale copy/assign/destroy, delete) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

uint32_t gFilebufCharVtblA[4];
uint32_t gFilebufCharVtblB[4];

namespace streambuf_callee {
int32_t Underflow(void* sb) {
    ev("Underflow", {lab(sb)});
    return static_cast<int32_t>(ret("Underflow"));
}
void LocaleCopyCtor(void* out, const void* src) { ev("LocaleCopyCtor", {lab(out), lab(const_cast<void*>(src))}); }
void LocaleAssign(void* dst, const void* src) { ev("LocaleAssign", {lab(dst), lab(const_cast<void*>(src))}); }
}  // namespace streambuf_callee

namespace filebuf_char_callee {
void LocaleDtor(void* locale, int32_t flag) { ev("LocaleDtor", {lab(locale), flag}); }
}  // namespace filebuf_char_callee

namespace filebuf_callee {
void Delete(void* p) { ev("Delete", {lab(p)}); }
}  // namespace filebuf_callee

extern "C" {

uint64_t tpw_sb_addr(const char* name) {
    if (std::strcmp(name, "vtbl") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gFilebufCharVtblA));
    return 0;
}

uint64_t tpw_sb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufChar, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufChar, mGptr));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufChar, mZero10));
    out[n++] = static_cast<int64_t>(offsetof(BasicStreambufChar, mLocale));
    out[n++] = static_cast<int64_t>(sizeof(BasicStreambufChar));
    return n;
}

static BasicStreambufChar* SB(uint64_t p) { return reinterpret_cast<BasicStreambufChar*>(static_cast<uintptr_t>(p)); }

int32_t tpw_sb_uflow(uint64_t sb) { return SB(sb)->uflow(); }
void tpw_sb_imbue(uint64_t sb, uint64_t loc) { SB(sb)->imbue(reinterpret_cast<void*>(static_cast<uintptr_t>(loc))); }
void tpw_sb_getloc(uint64_t sb, uint64_t out) { SB(sb)->getloc(reinterpret_cast<void*>(static_cast<uintptr_t>(out))); }
uint64_t tpw_sb_dt(uint64_t sb, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(SB(sb)->destroy(static_cast<int16_t>(flag))));
}

}  // extern "C"
