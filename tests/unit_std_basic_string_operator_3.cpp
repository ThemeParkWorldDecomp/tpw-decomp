// Test library for unit v51033 (libtpw_unit_std_basic_string_operator_3.dylib, tests/test_std_basic_string_operator_3.py), main app:
//   src/game/MslSupport.cpp   string operator[], locale ctor, widen, _RefCountedPtr dtor, filebuf<char> close / underflow
//
// The callees are fakes that record what the real ones read; every block the module builds carries a tag word at +0x18, and a pointer
// is logged as that tag (the two sides have different addresses). The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace calllog;

LocaleRefPtr gLocaleGlobal;   // std::locale::global_s (0x103bcfd4)

namespace {

struct ClassicFake {
    LocaleRefPtr p;
    uint8_t pad[0x20];   // the tag word lives at +0x18
};
ClassicFake gClassic;

int64_t tagOf(const void* p) { return p ? *reinterpret_cast<const uint32_t*>(static_cast<const char*>(p) + 0x18) : 0; }

}  // namespace

// ---- the fakes
namespace filebuf_callee {
void* New(uint32_t size) {
    ev("New", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size + 0x20);
}
}  // namespace filebuf_callee

namespace msl_string_callee {
void* Memcpy(void* to, const void* from, uint32_t n) {
    ev("Memcpy", {static_cast<int64_t>(n)});
    std::memmove(to, from, n);
    return to;
}
}  // namespace msl_string_callee

namespace locale_imp_callee {
void Delete(void* p) { ev("Delete", {tagOf(p)}); }
}  // namespace locale_imp_callee

namespace filebuf_wclose_callee {
void LocaleImpDtor(void* impl, int32_t flag) { ev("ImpDtor", {tagOf(impl), static_cast<int64_t>(flag)}); }
int32_t VirtualUnderflow(void* fb) {
    ev("VUnderflow", {tagOf(fb)});
    return static_cast<int32_t>(ret("VUnderflow"));
}
}  // namespace filebuf_wclose_callee

namespace filebuf_char_callee {
void Getloc(void* out, void* streambuf) {
    ev("Getloc", {tagOf(streambuf)});
    LocaleRefPtr* o = static_cast<LocaleRefPtr*>(out);
    const LocaleRefPtr* s = static_cast<const LocaleRefPtr*>(streambuf);
    o->mImp = s->mImp;
    o->mCount = s->mCount;
}
int32_t Unshift(void* filebuf) {
    ev("Unshift", {tagOf(filebuf)});
    return static_cast<int32_t>(ret("Unshift"));
}
int32_t Fclose(uint32_t file) {
    ev("Fclose", {static_cast<int64_t>(file)});
    return static_cast<int32_t>(ret("Fclose"));
}
}  // namespace filebuf_char_callee

void* UseFacetCtypeChar(const UseFacetLocale* locale) {
    ev("UseFacet", {tagOf(locale->mImp)});
    return *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(locale->mImp) + 8);
}

namespace v51033_callee {
const LocaleRefPtr* LocaleClassic() {
    ev("Classic", {0});
    return &gClassic.p;
}
char CtypeWiden(void* facet, char c) {
    ev("Widen", {tagOf(facet), static_cast<int64_t>(c)});
    return static_cast<char>(ret("Widen"));
}
}  // namespace v51033_callee

LocaleRefPtr& LocaleRefPtr::operator=(const LocaleRefPtr& other) {
    ev("Assign", {tagOf(this), tagOf(&other), tagOf(other.mImp), tagOf(other.mCount)});
    return *this;
}

extern "C" {

uint64_t tpw_s_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"global_s", &gLocaleGlobal},
        {"classic", &gClassic},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_s_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }
uint64_t tpw_s_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_s_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, data));
    out[n++] = static_cast<int64_t>(offsetof(LocaleRefPtr, mCount));
    out[n++] = static_cast<int64_t>(sizeof(BasicFilebufChar));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFlag3e));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mEgptr));
    return n;
}

uint64_t tpw_s_index(uint64_t str, uint32_t i) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(MslCharStringIndex(reinterpret_cast<MslCharString*>(str), i)));
}
uint64_t tpw_s_locale(uint64_t self) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LocaleConstruct(reinterpret_cast<LocaleRefPtr*>(self))));
}
int32_t tpw_s_widen(uint64_t self, int32_t c) {
    return BasicIosWiden(reinterpret_cast<const BasicIosChar*>(self), static_cast<char>(c));
}
uint64_t tpw_s_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(LocaleRefPtrDestroy(reinterpret_cast<LocaleRefPtr*>(self), static_cast<int16_t>(flag))));
}
uint64_t tpw_s_close(uint64_t self) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(FilebufCharClose(reinterpret_cast<BasicFilebufChar*>(self))));
}
int32_t tpw_s_underflow(uint64_t self) { return FilebufCharUnderflow(reinterpret_cast<BasicFilebufChar*>(self)); }

}  // extern "C"
