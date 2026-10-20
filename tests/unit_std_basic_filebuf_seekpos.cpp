// Test library for std::basic_filebuf<char> seekpos / seekoff / overflow (unit v5233; libtpw_unit_std_basic_filebuf_seekpos.dylib,
// tests/test_std_basic_filebuf_seekpos.py), main app:
//   src/std_basic_filebuf_seekpos.cpp
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
inline int64_t I(int64_t v) { return v; }

BasicFilebufChar* FB(uint64_t a) { return reinterpret_cast<BasicFilebufChar*>(static_cast<uintptr_t>(a)); }
constexpr uintptr_t kCvtHandle = 0x7C0DE;   // the codecvt word the test stores in the host object (opaque)
int64_t CvtLab(void* cvt) { return reinterpret_cast<uintptr_t>(cvt) == kCvtHandle ? 2 : -1; }
}  // namespace

namespace filebuf_wseek_callee {
int32_t CodecvtEncoding(void* cvt) {
    ev("Enc", {I(CvtLab(cvt))});
    return static_cast<int32_t>(ret("Enc"));
}
int32_t Ftell(uint32_t file) {
    ev("Ftell", {I(file)});
    return static_cast<int32_t>(ret("Ftell"));
}
int32_t Fputc(int32_t c, uint32_t file) {
    ev("Fputc", {I(c), I(file)});
    return static_cast<int32_t>(ret("Fputc"));
}
}  // namespace filebuf_wseek_callee

namespace filebuf_charstr_callee {
void StringInit(MslCharRep** s, uint32_t n, char c, void*) {
    ev("StrInit", {I(n), I(static_cast<uint8_t>(c))});
    MslCharRep* rep = static_cast<MslCharRep*>(tpw::ArenaAllocZeroed(sizeof(MslCharRep)));
    char* data = static_cast<char*>(tpw::ArenaAllocZeroed(n + 32));
    std::memset(data, c, n);
    rep->len = n;
    rep->cap = n;
    rep->refs = 0;
    rep->data = data;
    *s = rep;
}
void StringAssign(MslCharRep** s, uint32_t n, char c) {
    MslCharRep* rep = *s;
    ev("StrAssign", {I(rep->len), I(n), I(static_cast<uint8_t>(c))});
    std::memset(rep->data, c, n);
    rep->data[n] = 0;
    rep->len = n;
}
void StringResize(MslCharRep** s, uint32_t n, char c) {
    MslCharRep* rep = *s;
    ev("StrResize", {I(rep->len), I(n), I(static_cast<uint8_t>(c))});
    uint32_t ln = rep->len;
    char* data = rep->data;
    if (n > rep->cap) {
        char* nd = static_cast<char*>(tpw::ArenaAllocZeroed(n + 32));
        std::memcpy(nd, data, ln);
        data = nd;
        rep->cap = n;
    }
    if (n > ln) std::memset(data + ln, c, n - ln);
    data[n] = 0;
    rep->len = n;
    rep->data = data;
}
void StringDtor(MslCharRep**, int32_t flag) { ev("StrDtor", {I(flag)}); }
}  // namespace filebuf_charstr_callee

namespace filebuf_char_callee {
int32_t Fseek(uint32_t file, int32_t offset, int32_t whence) {
    ev("Fseek", {I(file), I(offset), I(whence)});
    return static_cast<int32_t>(ret("Fseek"));
}
int32_t Unshift(void* filebuf) {
    ev("Unshift", {I(lab(filebuf))});
    return static_cast<int32_t>(ret("Unshift"));
}
}  // namespace filebuf_char_callee

namespace filebuf_charout_callee {
int32_t CodecvtOutChar(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext, char* to, char* toEnd,
                       char** toNext) {
    ev("Out", {I(CvtLab(cvt)), I(lab(static_cast<char*>(state) - 0x28)), I(static_cast<uint8_t>(*from)), I(fromEnd - from), I(toEnd - to)});
    int32_t r = static_cast<int32_t>(ret("Out"));
    int64_t fa = ret("OutFa");
    int64_t ta = ret("OutTa");
    int64_t b = ret("OutByte");
    *fromNext = from + fa;
    *toNext = to + ta;
    if (toEnd > to) *to = static_cast<char>(b);
    return r;
}
}  // namespace filebuf_charout_callee

extern "C" {

uint64_t tpw_fbso_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fbso_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mState));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFlag3e));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mUnbuffered));
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufChar));
    return n;
}

static uint64_t Pack(FposInt r) { return static_cast<uint64_t>(static_cast<uint32_t>(r.off)) | (static_cast<uint64_t>(static_cast<uint32_t>(r.state)) << 32); }

uint64_t tpw_fbso_seekpos(uint64_t fb, int32_t off, int32_t state, uint32_t mode) { return Pack(FB(fb)->seekpos(off, state, mode)); }
uint64_t tpw_fbso_seekoff(uint64_t fb, int32_t off, int32_t dir, uint32_t mode) { return Pack(FB(fb)->seekoff(off, dir, mode)); }
int32_t tpw_fbso_overflow(uint64_t fb, int32_t c) { return FB(fb)->overflow(c); }

}  // extern "C"
