// Test library for std::basic_filebuf<wchar_t> seekoff, seekpos, overflow (unit v5093; libtpw_unit_std_basic_filebuf_seekoff.dylib,
// tests/test_std_basic_filebuf_seekoff.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues (codecvt encoding/out,
// unshift, fseek, ftell, fputc, the string init/assign/resize, operator delete). Pointers are logged as small kinds, not addresses.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

char gMslEmptyChar;

namespace {
BasicFilebufWchar* gThis = nullptr;
MslStrRep* gRep = nullptr;
uint32_t gCvt = 0;

void* P(uint32_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }
}  // namespace

namespace filebuf_wseek_callee {

int32_t CodecvtEncoding(void* cvt) {
    ev("Encoding", {static_cast<int64_t>(cvt == P(gCvt) ? 1 : 0)});
    return static_cast<int32_t>(ret("Enc"));
}

int32_t CodecvtOut(void* cvt, void* state, const char16_t* from, const char16_t* fromEnd, const char16_t** fromNext, char* to,
                   char* toEnd, char** toNext) {
    (void)fromNext;
    int64_t isData = (gRep != nullptr && to == static_cast<char*>(P(gRep->data))) ? 1 : 0;
    ev("Out", {static_cast<int64_t>(cvt == P(gCvt) ? 1 : 0), static_cast<int64_t>(state == static_cast<void*>(gThis->mState) ? 1 : 0),
               static_cast<int64_t>(*from), static_cast<int64_t>(fromEnd - from), static_cast<int64_t>(toEnd - to), isData});
    int32_t res = static_cast<int32_t>(ret("Out"));
    int64_t k = ret("OutK");
    int64_t b = ret("OutB");
    *toNext = to + k;
    if (!isData) to[0] = static_cast<char>(b);   // the string body is an opaque handle on the host (arena is above 4 GB)
    return res;
}

bool Unshift(BasicFilebufWchar* fb) {
    ev("Unshift", {static_cast<int64_t>(fb == gThis ? 1 : 0)});
    return (ret("Unshift") & 0xff) != 0;
}

int32_t Fseek(uint32_t file, int32_t offset, int32_t whence) {
    ev("Fseek", {static_cast<int64_t>(file), static_cast<int64_t>(offset), static_cast<int64_t>(whence)});
    return static_cast<int32_t>(ret("Fseek"));
}

int32_t Ftell(uint32_t file) {
    ev("Ftell", {static_cast<int64_t>(file)});
    return static_cast<int32_t>(ret("Ftell"));
}

int32_t Fputc(int32_t c, uint32_t file) {
    ev("Fputc", {static_cast<int64_t>(c), static_cast<int64_t>(file)});
    return static_cast<int32_t>(ret("Fputc"));
}

void StringInit(MslStrRep** s, uint32_t n, char c, void* alloc) {
    (void)alloc;
    ev("StringInit", {static_cast<int64_t>(n), static_cast<int64_t>(c)});
    MslStrRep* rep = static_cast<MslStrRep*>(tpw::ArenaAllocZeroed(sizeof(MslStrRep)));
    rep->len = static_cast<int32_t>(n);
    rep->cap = 256;
    rep->refs = static_cast<int32_t>(ret("InitRefs"));
    rep->data = 0x20000000u;   // opaque handle, never dereferenced (the translation only does arithmetic on it)
    gRep = rep;
    *s = rep;
}

void StringAssign(MslStrRep** s, uint32_t n, char c) {
    ev("StringAssign", {static_cast<int64_t>(n), static_cast<int64_t>(c)});
    MslStrRep* rep = *s;
    rep->len = static_cast<int32_t>(n);
}

void StringResize(MslStrRep** s, uint32_t n, char c) {
    ev("StringResize", {static_cast<int64_t>(static_cast<int32_t>(n)), static_cast<int64_t>(c)});
    (*s)->len = static_cast<int32_t>(n);
}

void Delete(void* p) {
    int64_t kind = 0;
    if (gRep != nullptr) {
        if (p == P(gRep->data)) kind = 1;
        else if (p == gRep) kind = 2;
    }
    ev("Delete", {kind});
}

}  // namespace filebuf_wseek_callee

extern "C" {

uint64_t tpw_wseek_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_wseek_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mState));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mBuf));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFlag4c));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mUnbuffered));
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufWchar));
    return n;
}

static void Begin(uint64_t fb) {
    gThis = reinterpret_cast<BasicFilebufWchar*>(static_cast<uintptr_t>(fb));
    gCvt = gThis->mCodecvt;
    gRep = nullptr;
}

void tpw_wseek_seekoff(uint64_t out, uint64_t fb, int32_t off, int32_t dir, uint32_t mode) {
    Begin(fb);
    FposInt r = gThis->seekoff(off, dir, mode);
    FposInt* o = reinterpret_cast<FposInt*>(static_cast<uintptr_t>(out));
    *o = r;
}

void tpw_wseek_seekpos(uint64_t out, uint64_t fb, int32_t off, int32_t state, uint32_t mode) {
    Begin(fb);
    FposInt r = gThis->seekpos(off, state, mode);
    FposInt* o = reinterpret_cast<FposInt*>(static_cast<uintptr_t>(out));
    *o = r;
}

uint32_t tpw_wseek_overflow(uint64_t fb, uint32_t c) {
    Begin(fb);
    return static_cast<uint32_t>(gThis->overflow(static_cast<char16_t>(c)));
}

}  // extern "C"
