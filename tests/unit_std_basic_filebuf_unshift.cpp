// Test library for std::basic_filebuf<wchar_t> unshift, sync, ~basic_filebuf (unit v5743; libtpw_unit_std_basic_filebuf_unshift.dylib), main app:
//   src/game/MslSupport.cpp
//
// The callees (codecvt unshift, fputc, fflush, close, the locale destructor, operator delete) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

using namespace calllog;

uint32_t gIfsWVtblC[4];
uint32_t gIfsWVtblD[4];
uint8_t gIfsWFiles[0xfc];

static void* P(uint32_t v) { return v == 0 ? nullptr : reinterpret_cast<void*>(tpw::gArenaBase + v); }   // gArenaBase is a byte pointer

namespace filebuf_unshift_callee {
int32_t CodecvtUnshift(void* cvt, void* state, char* to, char* toEnd, char** toNext) {
    int32_t code = static_cast<int32_t>(ret("cvt"));
    int32_t n = static_cast<int32_t>(ret("cvtn"));
    ev("CodecvtUnshift", {lab(cvt), lab(state), static_cast<int64_t>(toEnd - to), code, n});
    for (int32_t i = 0; i < n; i++) to[i] = static_cast<char>((0x60 + 0x15 * (i + code + 1)) & 0xff);
    *toNext = to + n;
    return code;
}
int32_t Fflush(uint32_t file) {
    ev("Fflush", {lab(P(file))});
    return static_cast<int32_t>(ret("Fflush"));
}
void LocaleDtor(void* locale, int32_t flag) { ev("LocaleDtor", {lab(locale), flag}); }
}  // namespace filebuf_unshift_callee

namespace filebuf_wseek_callee {
int32_t Fputc(int32_t c, uint32_t file) {
    ev("Fputc", {c, lab(P(file))});
    return static_cast<int32_t>(ret("Fputc"));
}
void Delete(void* p) { ev("Delete", {lab(p)}); }
}  // namespace filebuf_wseek_callee

BasicFilebufWchar* BasicFilebufWchar::close() {
    ev("Close", {lab(this)});
    return this;
}

extern "C" {

uint64_t tpw_fbu_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"files", gIfsWFiles},
        {"vtC", gIfsWVtblC},
        {"vtD", gIfsWVtblD},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fbu_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

uint64_t tpw_fbu_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fbu_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mState));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mCodecvt));
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufWchar));
    return n;
}

uint32_t tpw_fbu_unshift(uint64_t fb) { return reinterpret_cast<BasicFilebufWchar*>(fb)->unshift() ? 1 : 0; }
int32_t tpw_fbu_sync(uint64_t fb) { return reinterpret_cast<BasicFilebufWchar*>(fb)->sync(); }
uint32_t tpw_fbu_dtor(uint64_t fb, int16_t flag) {
    BasicFilebufWchar* self = reinterpret_cast<BasicFilebufWchar*>(fb);
    return self->destroy(flag) == self ? 1 : 0;
}

}  // extern "C"
