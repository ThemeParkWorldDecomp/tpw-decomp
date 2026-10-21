// Test library for the swear filter (unit v5094; libtpw_unit_SwearFilter_Filter.dylib, tests/test_SwearFilter_Filter.py), main app:
//   src/game/online/IPBase.cpp   SwearFilter::Filter, SwearFilter::LoadWordList
//
// The callees outside the unit (stristr, _wcsnicmp, the stream pieces, the exception pieces) are fakes that record the arguments the
// real ones read and answer from the test's queues. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"
#include "std_basic_istream.h"
#include "ui_ime.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint16_t gSwearWctypeMap[256];
char gIfstreamVtbl[16];
char gIosFailureVtbl[16];
char gIosFailureDtorVec[8];
namespace IME {
char gMslExcVtblC[16];
}

namespace {

const char16_t* gTextBase = nullptr;

uint32_t HashBytes(const char* s, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<uint8_t>(s[i])) * 16777619u;
    return h & 0x7fffffffu;
}

uint32_t Hash16(const char16_t* s, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ s[i]) * 16777619u;
    return h & 0x7fffffffu;
}

size_t Len16(const char16_t* s) {
    size_t n = 0;
    while (s[n] != 0) ++n;
    return n;
}

int32_t Low(char16_t c) { return (c >= 65 && c <= 90) ? c + 32 : c; }

int64_t Off(const char16_t* p) { return p - gTextBase; }

}  // namespace

// ---- the fakes
char16_t* stristr(char16_t* haystack, char16_t* needle) {
    size_t hn = Len16(haystack), nn = Len16(needle);
    int64_t found = -1;
    if (nn == 0) {
        found = 0;
    } else {
        for (size_t i = 0; i < hn && found < 0; ++i) {
            bool ok = true;
            for (size_t j = 0; j < nn; ++j) {
                if (i + j >= hn || Low(haystack[i + j]) != Low(needle[j])) {
                    ok = false;
                    break;
                }
            }
            if (ok) found = static_cast<int64_t>(i);
        }
    }
    ev("stristr", {Off(haystack), static_cast<int64_t>(Hash16(haystack, hn)), static_cast<int64_t>(Hash16(needle, nn)),
                   found < 0 ? -1 : Off(haystack) + found});
    return found < 0 ? nullptr : haystack + found;
}

namespace NS_MacDoze {
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count) {
    int32_t result = 0;
    for (uint32_t i = 0; i < count; ++i) {
        int32_t x = Low(a[i]), y = Low(b[i]);
        if (x != y) {
            result = x - y;
            break;
        }
        if (x == 0) break;
    }
    ev("wcsnicmp", {Off(a), static_cast<int64_t>(Hash16(b, count)), static_cast<int64_t>(count), result});
    return result;
}
}  // namespace NS_MacDoze

namespace swear_callee {
void IfstreamCtor(SwearIfstream* s, int32_t flag) {
    ev("IfsCtor", {flag});
    s->mIos = &s->mIosStorage;
    s->mGcount = 0;
    s->mVtbl = nullptr;
    s->mFilebuf.mFile = 0;
    s->mIosStorage.mVtbl = nullptr;
    s->mIosStorage.mRdbuf = static_cast<uint32_t>(ret("rdbuf"));
    s->mIosStorage.mState = 0;
    s->mIosStorage.mMask = static_cast<uint32_t>(ret("mask"));
    s->mIosStorage.mOffset = 0;
}
void FilebufDtor(SwearFilebuf* buf, int16_t flag) { ev("FbDtor", {flag, static_cast<int64_t>(buf->mFile)}); }
void IstreamDtor(SwearIfstream* s, int16_t flag) {
    ev("IsDtor", {flag, static_cast<int64_t>(s->mIos->mState), s->mVtbl == gIfstreamVtbl ? 1 : 0,
                  s->mIos->mVtbl == gIfstreamVtbl + 0xc ? 1 : 0, static_cast<int64_t>(s->mIos->mOffset)});
}
void IosDtor(SwearIos* ios, int16_t flag) { ev("IosDtor", {flag, static_cast<int64_t>(ios->mState)}); }
SwearFilebuf* FilebufOpen(SwearFilebuf* buf, const char* path, int32_t mode) {
    ev("Open", {mode, static_cast<int64_t>(HashBytes(path, std::strlen(path)))});
    buf->mFile = static_cast<uint32_t>(ret("mfile"));
    return ret("open") != 0 ? buf : nullptr;
}
SwearFilebuf* FilebufClose(SwearFilebuf* buf) {
    ev("Close", {});
    return ret("close") != 0 ? buf : nullptr;
}
void IstreamRead(SwearIfstream* s, char* buffer, int32_t count) {
    int64_t v = ret("rd");
    ev("Read", {count, v});
    buffer[0] = static_cast<char>((v >> 8) & 0xff);
    buffer[1] = static_cast<char>(v & 0xff);
    if (v & 0x10000) s->mIos->mState |= 2;
}
}  // namespace swear_callee

namespace IME {
size_t MslStrlen(const char* s) { return std::strlen(s); }
char* MslStrcpy(char* d, const char* s) { return std::strcpy(d, s); }
void* MslNewArray(uint32_t size) { return tpw::ArenaAllocZeroed(size); }
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    self->mPtr = p;
    self->mCount = nullptr;
}
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* ex = static_cast<MslExceptionObject*>(object);
    ev("Throw", {static_cast<int64_t>(HashBytes(typeName, std::strlen(typeName))), ex->mVtbl == gIosFailureVtbl ? 1 : 0,
                 dtorTV == gIosFailureDtorVec ? 1 : 0,
                 static_cast<int64_t>(HashBytes(ex->mRc.mPtr, std::strlen(ex->mRc.mPtr)))});
}
}  // namespace IME

extern "C" {

uint64_t tpw_swear_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"map", gSwearWctypeMap},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_swear_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_swear_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(uint16_t[256]));
    return n;
}

// ---- host objects for the test (std types are native here)
uint64_t tpw_swear_new_filter() { return reinterpret_cast<uintptr_t>(new SwearFilter()); }
void tpw_swear_add(uint64_t filter, int32_t which, const char16_t* chars, int32_t n) {
    SwearFilter* f = reinterpret_cast<SwearFilter*>(filter);
    (which == 0 ? f->mWords : f->mAllowed).push_back(std::u16string(chars, static_cast<size_t>(n)));
}
uint64_t tpw_swear_new_str(const char16_t* chars, int32_t n) {
    return reinterpret_cast<uintptr_t>(new std::u16string(chars, static_cast<size_t>(n)));
}
uint64_t tpw_swear_str_data(uint64_t str) { return reinterpret_cast<uintptr_t>(&(*reinterpret_cast<std::u16string*>(str))[0]); }
int32_t tpw_swear_str_len(uint64_t str) { return static_cast<int32_t>(reinterpret_cast<std::u16string*>(str)->size()); }
uint64_t tpw_swear_new_list() { return reinterpret_cast<uintptr_t>(new std::list<std::u16string>()); }
void tpw_swear_list_push(uint64_t list, const char16_t* chars, int32_t n) {
    reinterpret_cast<std::list<std::u16string>*>(list)->push_back(std::u16string(chars, static_cast<size_t>(n)));
}
int32_t tpw_swear_list_count(uint64_t list) { return static_cast<int32_t>(reinterpret_cast<std::list<std::u16string>*>(list)->size()); }
int32_t tpw_swear_list_item(uint64_t list, int32_t index, char16_t* out) {
    auto it = reinterpret_cast<std::list<std::u16string>*>(list)->begin();
    std::advance(it, index);
    std::memcpy(out, it->data(), it->size() * 2);
    return static_cast<int32_t>(it->size());
}

// ---- the functions
uint32_t tpw_swear_filter(uint64_t filter, uint64_t str, int32_t replace) {
    std::u16string* s = reinterpret_cast<std::u16string*>(str);
    gTextBase = &(*s)[0];
    return reinterpret_cast<SwearFilter*>(filter)->Filter(*s, (replace & 0xff) != 0) ? 1u : 0u;
}
uint32_t tpw_swear_load(uint64_t filter, uint64_t path, uint64_t list, int32_t flag) {
    return reinterpret_cast<SwearFilter*>(filter)->LoadWordList(reinterpret_cast<const char*>(path),
                                                                  *reinterpret_cast<std::list<std::u16string>*>(list), (flag & 0xff) != 0)
               ? 1u
               : 0u;
}

}  // extern "C"
