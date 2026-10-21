// Test library for the small MSL locale / streambuf / filebuf / ios_base::failure helpers (unit v51054;
// libtpw_unit_std_use_facet.dylib, tests/test_std_use_facet.py), main app:
//   src/game/MslSupport.cpp, src/game/rides/FlyingRide.cpp   the 25 functions of the unit
//
// The callees outside the unit are fakes that record the arguments the real ones read: operator delete, the
// _RefCountedPtr<char> destructor, the codecvt unshift (__ptr_glue), fputc, fflush and the four __use_facet instances.
// Pointers are logged as offsets from the case's block (set_base), so both sides log the same numbers.
// Globals of other files (gIosFailureVtbl, gMslExcVtblC, gFlyBases) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_basic_istream.h"
#include "ui_ime.h"
#include "HLSprite.h"

using namespace calllog;

int32_t FLY_GetCameraMesh(int32_t tour);

char gIosFailureVtbl[16];
void* gFlyBases[100];
namespace IME {
char gMslExcVtblC[16];
}

namespace {
uintptr_t g_base = 0;

int64_t Rel(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    // Compared on the low 32 bits: some words of the world are 32-bit handles of block addresses.
    uint32_t lo = static_cast<uint32_t>(v), b = static_cast<uint32_t>(g_base);
    if (lo >= b && lo < b + 0x400) return static_cast<int64_t>(lo - b);
    return static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(v)));
}
int64_t RelW(uint32_t w) { return Rel(reinterpret_cast<const void*>(static_cast<uintptr_t>(w))); }
}  // namespace

// ---- the fakes
namespace locale_imp_callee {
void Delete(void* p) { ev("Delete", {Rel(p)}); }
}  // namespace locale_imp_callee

namespace IME {
MslRefCountedChars* MslRefCountedCharsDtor25e0(MslRefCountedChars* self, int16_t deleteFlag) {
    ev("RcDtor", {Rel(self) - 8, static_cast<int64_t>(deleteFlag)});
    return self;
}
}  // namespace IME

namespace filebuf_unshift_callee {
int32_t CodecvtUnshift(void* cvt, void* state, char* to, char* toEnd, char** toNext) {
    ev("Unshift", {Rel(cvt), Rel(state), static_cast<int64_t>(toEnd - to), static_cast<int64_t>(*toNext - to)});
    int32_t result = static_cast<int32_t>(ret("unshift_r"));
    int32_t n = static_cast<int32_t>(ret("unshift_n"));
    for (int32_t i = 0; i < n; ++i) to[i] = static_cast<char>(ret("unshift_b"));
    *toNext = to + n;
    return result;
}
int32_t Fflush(uint32_t file) {
    ev("Fflush", {static_cast<int64_t>(file)});
    return static_cast<int32_t>(ret("fflush"));
}
}  // namespace filebuf_unshift_callee

namespace filebuf_wseek_callee {
int32_t Fputc(int32_t c, uint32_t file) {
    ev("Fputc", {static_cast<int64_t>(c), static_cast<int64_t>(file)});
    return static_cast<int32_t>(ret("fputc"));
}
}  // namespace filebuf_wseek_callee

void* UseFacetCtypeChar(const UseFacetLocale* locale) {
    ev("UseFacetCtypeChar", {Rel(locale)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("use_facet")));
}
void* UseFacetCodecvtCharChar(const UseFacetLocale* locale) {
    ev("UseFacetCodecvtCharChar", {Rel(locale)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("use_facet")));
}
void* UseFacetCtypeWchar(const UseFacetLocale* locale) {
    ev("UseFacetCtypeWchar", {Rel(locale)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("use_facet")));
}
void* UseFacetCodecvtWcharChar(const UseFacetLocale* locale) {
    ev("UseFacetCodecvtWcharChar", {Rel(locale)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("use_facet")));
}

extern "C" {

uint64_t tpw_sf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ios", gIosFailureVtbl},
        {"exc", IME::gMslExcVtblC},
        {"fly", gFlyBases},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_sf_set_base(uint64_t base) { g_base = static_cast<uintptr_t>(base); }

int32_t tpw_sf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(LocaleCharArray, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(LocaleCharArray, mData));
    out[n++] = static_cast<int64_t>(offsetof(MslPtrVector, mSize));
    out[n++] = static_cast<int64_t>(offsetof(MslPtrVector, mData));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslExceptionObject, mRc));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslExceptionObject));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufChar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(IosBaseLocalePtr, mLocale));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

static void* P(uint64_t a) { return reinterpret_cast<void*>(static_cast<uintptr_t>(a)); }
static uint64_t U(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }

// One entry for every function: op numbers are the module's OPS.
uint64_t tpw_sf_call(int32_t op, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    using namespace v51054;
    (void)d;
    (void)e;
    switch (op) {
        case 0: return U(UseFacet_CtypeChar(static_cast<const UseFacetLocale*>(P(a))));
        case 1: return U(UseFacet_CodecvtCharChar(static_cast<const UseFacetLocale*>(P(a))));
        case 2: return U(UseFacet_CtypeWchar(static_cast<const UseFacetLocale*>(P(a))));
        case 3: return U(UseFacet_CodecvtWcharChar(static_cast<const UseFacetLocale*>(P(a))));
        case 4: return U(FacetDestroy(static_cast<UseFacetObj*>(P(a)), static_cast<int16_t>(b)));
        case 5: return FilebufCharUnshift(static_cast<BasicFilebufChar*>(P(a))) ? 1 : 0;
        case 6: return static_cast<uint32_t>(FilebufCharSync(static_cast<BasicFilebufChar*>(P(a))));
        case 7: IosBaseGetloc(static_cast<LocaleRefPtr*>(P(a)), static_cast<const IosBaseLocalePtr*>(P(b))); return 0;
        case 8: CharArrayAllocDestroy(P(a), static_cast<LocaleCharArray*>(P(b))); return 0;
        case 9: return U(CharArrayRemoveRef(static_cast<LocaleCharArray*>(P(a))));
        case 10: return FacetVectorSize(static_cast<const MslPtrVector*>(P(a)));
        case 11: return static_cast<uint32_t>(U(FacetVectorIndex(static_cast<MslPtrVector*>(P(a)), static_cast<uint32_t>(b))));
        case 12: CharArrayAllocDeallocate(P(a), P(b), static_cast<uint32_t>(c)); return 0;
        case 13: return U(LocaleRefPtrCopy(static_cast<LocaleRefPtr*>(P(a)), static_cast<const LocaleRefPtr*>(P(b))));
        case 14: {
            FposInt r = StreambufSeekoff(static_cast<BasicStreambufChar*>(P(b)), static_cast<int32_t>(c), static_cast<int32_t>(d), static_cast<uint32_t>(e));
            *static_cast<FposInt*>(P(a)) = r;
            return 0;
        }
        case 15: {
            FposInt r = StreambufSeekpos(static_cast<BasicStreambufChar*>(P(b)), static_cast<int32_t>(c), static_cast<int32_t>(d), static_cast<uint32_t>(e));
            *static_cast<FposInt*>(P(a)) = r;
            return 0;
        }
        case 16: return static_cast<uint32_t>(StreambufOverflow(static_cast<BasicStreambufChar*>(P(a)), static_cast<int32_t>(b)));
        case 17: return static_cast<uint32_t>(StreambufPbackfail(static_cast<BasicStreambufChar*>(P(a)), static_cast<int32_t>(b)));
        case 18: return static_cast<uint32_t>(StreambufUnderflow(static_cast<BasicStreambufChar*>(P(a))));
        case 19: return static_cast<uint32_t>(StreambufShowmanyc(static_cast<BasicStreambufChar*>(P(a))));
        case 20: return static_cast<uint32_t>(StreambufSync(static_cast<BasicStreambufChar*>(P(a))));
        case 21: return U(StreambufSetbuf(static_cast<BasicStreambufChar*>(P(a)), static_cast<char*>(P(b)), static_cast<int32_t>(c)));
        case 22: return U(IosFailureDtor(static_cast<IME::MslExceptionObject*>(P(a)), static_cast<int16_t>(b)));
        case 23: return U(IosFailureWhat(static_cast<const IME::MslExceptionObject*>(P(a))));
        case 24: return static_cast<uint32_t>(FLY_GetCameraMesh(static_cast<int32_t>(a)));
    }
    return 0;
}

}  // extern "C"
