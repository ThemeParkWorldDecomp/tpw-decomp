// Test library for std::basic_filebuf<wchar_t>::uflow (unit v5025; libtpw_unit_std_basic_filebuf_uflow.dylib,
// tests/test_std_basic_filebuf_uflow.py), main app:
//   src/std_basic_filebuf_uflow.cpp   BasicFilebufWchar::uflow
//
// The callees (fgetc, wmemmove, the codecvt virtuals, operator new/delete, string append and operator[]) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

char gMslEmptyChar;   // 0x102b1fb2

namespace {
uintptr_t gObj = 0;
std::map<const void*, uint32_t> gAllocs;
inline int64_t I(int64_t v) { return v; }
inline uint32_t A32(const void* p) { return tpw::ArenaOffset(p); }
inline char* D(uint32_t v) { return reinterpret_cast<char*>(tpw::gArenaBase + v); }   // gArenaBase is a byte pointer
void* Block(uint32_t n) { return tpw::ArenaAllocZeroed(n); }
}  // namespace

namespace filebuf_callee {

int32_t Fgetc(void* file) {
    ev("fgetc", {I(lab(file))});
    return static_cast<int32_t>(ret("fgetc"));
}

void Wmemmove(void* dst, const void* src, uint32_t n) {
    ev("wmemmove", {I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dst) - gObj)),
                    I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(src) - gObj)), I(n)});
    std::memmove(dst, src, static_cast<size_t>(n) * 2);
}

int32_t CodecvtMaxLength(void* cvt) {
    ev("cvt_max", {I(lab(cvt))});
    return static_cast<int32_t>(ret("max"));
}

int32_t CodecvtIn(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                  char16_t* to, char16_t* toEnd, char16_t** toNext) {
    (void)fromNext;
    (void)toEnd;
    (void)toNext;
    uint32_t n = static_cast<uint32_t>(fromEnd - from);
    int64_t b[8];
    for (int i = 0; i < 8; i++) b[i] = (static_cast<uint32_t>(i) < n) ? static_cast<uint8_t>(from[i]) : 256;
    ev("cvt_in", {I(lab(cvt)), I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(state) - gObj)), I(n), b[0], b[1], b[2],
                  b[3], b[4], b[5], b[6], b[7]});
    *to = static_cast<char16_t>(ret("toval") & 0xffff);
    return static_cast<int32_t>(ret("in"));
}

void* New(uint32_t size) {
    ev("new", {I(size)});
    void* p = Block(size);
    gAllocs[p] = size;
    return p;
}

void Delete(void* p) {
    int64_t k = 0;
    if (p != nullptr) {
        auto it = gAllocs.find(p);
        k = it == gAllocs.end() ? -1 : static_cast<int64_t>(it->second);
    }
    ev("delete", {k});
}

void StringAppend(MslStrRep** s, uint32_t count, char c) {
    MslStrRep* r = *s;
    ev("append", {I(count), I(c), I(r->len)});
    if (static_cast<uint32_t>(r->len) + count > static_cast<uint32_t>(r->cap)) {
        uint32_t ncap = (static_cast<uint32_t>(r->len) + count) * 2;
        uint8_t* nd = static_cast<uint8_t*>(Block(ncap + 1));
        std::memcpy(nd, D(r->data), static_cast<size_t>(r->len));
        r->data = A32(nd);
        r->cap = static_cast<int32_t>(ncap);
    }
    char* d = D(r->data);
    std::memset(d + r->len, c, count);
    r->len += static_cast<int32_t>(count);
}

char* StringIndex(MslStrRep** s, uint32_t i) {
    ev("index", {I(i)});
    return D((*s)->data) + i;
}

}  // namespace filebuf_callee

extern "C" {

uint64_t tpw_fb_addr(const char* name) {
    static void* file = Block(16);
    static void* cvt = Block(8);
    static void* vtbl = Block(0x40);
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"nul", &gMslEmptyChar}, {"file", file}, {"cvt", cvt}, {"vtbl", vtbl},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_fb_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_fb_setobj(uint64_t obj) {
    gObj = static_cast<uintptr_t>(obj);
    gAllocs.clear();
}

int32_t tpw_fb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mEback));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mGptr));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mEgptr));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mState));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mBuf));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFlag4c));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mUnbuffered));
    out[n++] = static_cast<int64_t>(sizeof(MslStrRep));
    return n;
}

uint32_t tpw_fb_uflow(uint64_t obj) {
    return reinterpret_cast<BasicFilebufWchar*>(static_cast<uintptr_t>(obj))->uflow();
}

}  // extern "C"
