// Test library for std::basic_filebuf<char> (unit v5046; libtpw_unit_std_basic_filebuf_basic_filebuf.dylib,
// tests/test_std_basic_filebuf_basic_filebuf.py), main app:
//   src/game/MslSupport.cpp   construct, open, uflow and the two vtable stand-ins it defines
//
// The callees outside the unit are recording fakes: the locale and facet calls, the three codecvt virtuals (__ptr_glue),
// the C file functions, memmove, delete[], unshift and the string pieces. Pointers into the filebuf are logged as offsets from
// the object the test registers with tpw_fb_setup.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace {

uintptr_t gObj;     // the filebuf under test (host address)
uintptr_t gFacet;   // the codecvt facet block
MslStrRep* gRep;    // the string body the string constructor fake hands out

uint32_t W(const void* p) { return p != nullptr ? static_cast<uint32_t>(tpw::ArenaOffset(p)) : 0u; }
uint32_t Wraw(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
char* P(uint32_t a) { return a != 0 ? reinterpret_cast<char*>((uintptr_t)tpw::gArenaBase + a) : nullptr; }
int64_t Off(const void* p) { return static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - gObj)); }
int64_t FacetOff(const void* p) { return static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - gFacet)); }

int64_t VtIdx() {
    const uint32_t v = *reinterpret_cast<const uint32_t*>(gObj);
    if (v == Wraw(gFilebufCharVtblA)) return 1;
    if (v == Wraw(gFilebufCharVtblB)) return 2;
    return 0;
}

}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
namespace filebuf_char_callee {

void LocaleCtor(void* locale) {
    ev("LocaleCtor", {Off(locale), VtIdx()});
    *static_cast<uint32_t*>(locale) = static_cast<uint32_t>(ret("LocaleCtor"));
}

void Getloc(void*, void* streambuf) { ev("Getloc", {Off(streambuf), VtIdx()}); }

void* UseFacetCodecvt(void*) {
    ev("UseFacet", {0});
    return reinterpret_cast<void*>(gFacet);
}

void LocaleDtor(void*, int32_t flag) { ev("LocaleDtor", {static_cast<int64_t>(flag)}); }

int32_t CodecvtAlwaysNoconv(void* cvt) {
    ev("AlwaysNoconv", {FacetOff(cvt)});
    return static_cast<int32_t>(ret("noconv"));
}

int32_t CodecvtMaxLength(void* cvt) {
    ev("MaxLength", {FacetOff(cvt)});
    return static_cast<int32_t>(ret("maxlen"));
}

int32_t CodecvtInChar(void* cvt, void* state, const char* from, const char* fromEnd, const char**, char* to, char* toEnd,
                      char**) {
    int64_t bytes[6];
    for (int i = 0; i < 6; ++i) bytes[i] = (from + i < fromEnd) ? static_cast<int64_t>(static_cast<int8_t>(from[i])) : -1000;
    ev("In", {FacetOff(cvt), Off(state), static_cast<int64_t>(*static_cast<int32_t*>(state)),
              static_cast<int64_t>(toEnd - to), static_cast<int64_t>(fromEnd - from), bytes[0], bytes[1], bytes[2], bytes[3],
              bytes[4], bytes[5]});
    *static_cast<uint32_t*>(state) = static_cast<uint32_t>(ret("state"));
    *to = static_cast<char>(ret("tobyte"));
    return static_cast<int32_t>(ret("in"));
}

void Memmove(void* dst, const void* src, uint32_t n) {
    ev("Memmove", {Off(dst), Off(src), static_cast<int64_t>(n)});
    std::memmove(dst, src, n);
}

void StringCtor(MslStrRep** s, void*) {
    ev("StringCtor", {0});
    *s = gRep;
}

void StringDtor(MslStrRep**, int32_t flag) { ev("StringDtor", {static_cast<int64_t>(flag)}); }

uint32_t Fopen(const char* name, const char* mode) {
    int64_t packed = 0;
    for (int i = 0; i < 4 && mode[i] != 0; ++i) packed = (packed << 8) | static_cast<uint8_t>(mode[i]);
    ev("Fopen", {Off(name), packed});
    return static_cast<uint32_t>(ret("fopen"));
}

int32_t Fseek(uint32_t file, int32_t offset, int32_t whence) {
    ev("Fseek", {static_cast<int64_t>(static_cast<int32_t>(file)), static_cast<int64_t>(offset), static_cast<int64_t>(whence)});
    return static_cast<int32_t>(ret("fseek"));
}

int32_t Fclose(uint32_t file) {
    ev("Fclose", {static_cast<int64_t>(static_cast<int32_t>(file))});
    return static_cast<int32_t>(ret("fclose"));
}

void DeleteArray(void* p) { ev("Delete", {Off(p)}); }

int32_t Unshift(void* filebuf) {
    ev("Unshift", {Off(filebuf)});
    return static_cast<int32_t>(ret("unshift"));
}

}  // namespace filebuf_char_callee

namespace filebuf_callee {

int32_t Fgetc(void* file) {
    ev("Fgetc", {static_cast<int64_t>(static_cast<int32_t>(Wraw(file)))});
    return static_cast<int32_t>(ret("fgetc"));
}

void StringAppend(MslStrRep** s, uint32_t count, char c) {
    ev("Append", {static_cast<int64_t>(count), static_cast<int64_t>(static_cast<int8_t>(c))});
    MslStrRep* rep = *s;
    for (uint32_t i = 0; i < count; ++i) {
        P(rep->data)[rep->len] = c;
        rep->len += 1;
    }
}

char* StringIndex(MslStrRep** s, uint32_t i) {
    ev("Index", {static_cast<int64_t>(i)});
    return P((*s)->data) + i;
}

}  // namespace filebuf_callee

extern "C" {

uint64_t tpw_fb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtA", gFilebufCharVtblA},
        {"vtB", gFilebufCharVtblB},
    };
    if (std::strcmp(name, "base") == 0) return static_cast<uint64_t>((uintptr_t)tpw::gArenaBase);
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mEback));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mLocale));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mState));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mBuf));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFlag3e));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mUnbuffered));
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufChar));
    out[n++] = static_cast<int64_t>(sizeof(MslStrRep));
    return n;
}

void tpw_fb_setup(uint64_t obj, uint64_t facet, uint64_t rep) {
    gObj = static_cast<uintptr_t>(obj);
    gFacet = static_cast<uintptr_t>(facet);
    gRep = reinterpret_cast<MslStrRep*>(static_cast<uintptr_t>(rep));
}

uint32_t tpw_fb_construct(uint64_t obj, uint32_t file) {
    auto* fb = reinterpret_cast<BasicFilebufChar*>(static_cast<uintptr_t>(obj));
    return W(fb->construct(reinterpret_cast<void*>(static_cast<uintptr_t>(file))));   // an arena offset, like the fields
}

uint32_t tpw_fb_open(uint64_t obj, uint64_t name, int32_t mode) {
    auto* fb = reinterpret_cast<BasicFilebufChar*>(static_cast<uintptr_t>(obj));
    return W(fb->open(reinterpret_cast<const char*>(static_cast<uintptr_t>(name)), mode));
}

int32_t tpw_fb_uflow(uint64_t obj) { return reinterpret_cast<BasicFilebufChar*>(static_cast<uintptr_t>(obj))->uflow(); }

}  // extern "C"
