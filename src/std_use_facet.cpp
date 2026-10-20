// Unit v51054: small MSL helpers of the locale / streambuf / filebuf / ios_base::failure group (main app).
// Each function is a few instructions; the callees outside the unit are declared in include/std_basic_filebuf.h.
#include <cstdint>

#include "std_basic_filebuf.h"
#include "std_basic_istream.h"
#include "ui_ime.h"
#include "HLSprite.h"

int32_t FLY_GetCameraMesh(int32_t tour);   // 0x10075278 (declared in rsse.h)

namespace v51054 {

// The four use_facet<F>(locale) wrappers pass the locale on to std::__use_facet<F> (the second argument is the
// uninitialised stack word the original loads; nothing reads it).
void* UseFacet_CtypeChar(const UseFacetLocale* locale) {          // 0x10073934
    return ::UseFacetCtypeChar(locale);
}
void* UseFacet_CodecvtCharChar(const UseFacetLocale* locale) {    // 0x1007245c
    return ::UseFacetCodecvtCharChar(locale);
}
void* UseFacet_CtypeWchar(const UseFacetLocale* locale) {         // 0x1021eb10
    return ::UseFacetCtypeWchar(locale);
}
void* UseFacet_CodecvtWcharChar(const UseFacetLocale* locale) {   // 0x10225cb8
    return ::UseFacetCodecvtWcharChar(locale);
}

// std::locale::facet::~facet with the hidden delete flag (signed 16 bit).
UseFacetObj* FacetDestroy(UseFacetObj* self, int16_t flag) {      // 0x10072df0
    if (self != nullptr) {
        self->mVtable = 0x1029d2a0u;
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

// basic_filebuf<char>::unshift: flushes the shift sequence of the codecvt into the file.
bool FilebufCharUnshift(BasicFilebufChar* self) {                 // 0x10072a94
    int32_t result = 1;
    while (result == 1) {
        char buffer[4];
        char* next = buffer;
        const char* p = buffer;
        void* cvt = reinterpret_cast<void*>(static_cast<uintptr_t>(self->mCodecvt));
        void* state = reinterpret_cast<uint8_t*>(self) + 0x28;
        result = filebuf_unshift_callee::CodecvtUnshift(cvt, state, buffer, buffer + 4, &next);
        if (result == 2) return false;
        if (result >= 0 && result < 2) {
            while (p < next) {
                int32_t c = static_cast<int8_t>(*p++);
                if (filebuf_wseek_callee::Fputc(c, self->mFile) == -1) return false;
            }
        }
    }
    return true;
}

// basic_filebuf<char>::sync: -1 when there is no file or fflush fails.
int32_t FilebufCharSync(BasicFilebufChar* self) {                 // 0x100703e4
    bool failed = true;
    if (self->mFile != 0) {
        if (filebuf_unshift_callee::Fflush(self->mFile) == 0) failed = false;
    }
    return failed ? -1 : 0;
}

// std::ios_base::getloc: copies the locale (two words) and counts the new reference.
void IosBaseGetloc(LocaleRefPtr* out, const IosBaseLocalePtr* ios) {   // 0x10072a34
    const LocaleRefPtr* src = ios->mLocale;
    out->mImp = src->mImp;
    out->mCount = src->mCount;
    if (out->mCount != nullptr) *out->mCount = *out->mCount + 1;
}

void CharArrayAllocDestroy(void* alloc, LocaleCharArray* arr) {   // 0x1007272c
    (void)alloc;
    if (arr != nullptr) locale_imp_callee::Delete(arr->mData);
}

// Answers the array itself when the count reaches zero, else NULL. A count of 0 is first made 1 (so it drops to 0).
LocaleCharArray* CharArrayRemoveRef(LocaleCharArray* arr) {       // 0x10072688
    uint32_t refs = arr->mRefs;
    if (refs == 0) refs = 1;
    arr->mRefs = refs;
    refs = arr->mRefs - 1;
    arr->mRefs = refs;
    if (refs == 0) return arr;
    return nullptr;
}

uint32_t FacetVectorSize(const MslPtrVector* vec) {               // 0x10072618
    return vec->mSize;
}

// Address of element i (the original steps 4 bytes per element).
void** FacetVectorIndex(MslPtrVector* vec, uint32_t i) {          // 0x100725a0
    return reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(vec->mData) + (i << 2));
}

void CharArrayAllocDeallocate(void* alloc, void* arr, uint32_t n) {   // 0x10072364
    (void)alloc;
    (void)n;
    locale_imp_callee::Delete(arr);
}

// std::_RefCountedPtr<__locale_imp>::_RefCountedPtr(const _RefCountedPtr&): copies both words, counts the reference.
LocaleRefPtr* LocaleRefPtrCopy(LocaleRefPtr* self, const LocaleRefPtr* other) {   // 0x10072030
    self->mImp = other->mImp;
    self->mCount = other->mCount;
    if (self->mCount != nullptr) *self->mCount = *self->mCount + 1;
    return self;
}

// basic_streambuf<char> defaults.
FposInt StreambufSeekoff(BasicStreambufChar* self, int32_t off, int32_t dir, uint32_t mode) {   // 0x10070b60
    (void)self; (void)off; (void)dir; (void)mode;
    FposInt r;
    r.off = -1;
    r.state = 0;
    return r;
}
FposInt StreambufSeekpos(BasicStreambufChar* self, int32_t off, int32_t state, uint32_t mode) { // 0x10070ad8
    (void)self; (void)off; (void)state; (void)mode;
    FposInt r;
    r.off = -1;
    r.state = 0;
    return r;
}
int32_t StreambufOverflow(BasicStreambufChar* self, int32_t c) { (void)self; (void)c; return -1; }    // 0x100707b0
int32_t StreambufPbackfail(BasicStreambufChar* self, int32_t c) { (void)self; (void)c; return -1; }   // 0x10070754
int32_t StreambufUnderflow(BasicStreambufChar* self) { (void)self; return -1; }                       // 0x10070634
int32_t StreambufShowmanyc(BasicStreambufChar* self) { (void)self; return 0; }                        // 0x100705d8
int32_t StreambufSync(BasicStreambufChar* self) { (void)self; return 0; }                             // 0x10070584
BasicStreambufChar* StreambufSetbuf(BasicStreambufChar* self, char* buffer, int32_t n) {              // 0x10070530
    (void)buffer; (void)n;
    return self;
}

// std::ios_base::failure::~failure with the hidden delete flag.
IME::MslExceptionObject* IosFailureDtor(IME::MslExceptionObject* self, int16_t flag) {   // 0x1007020c
    if (self != nullptr) {
        self->mVtbl = gIosFailureVtbl;
        IME::MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = IME::gMslExcVtblC;
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

const char* IosFailureWhat(const IME::MslExceptionObject* self) {   // 0x100702c8
    return self->mRc.mPtr;
}

}  // namespace v51054

// FLY_GetCameraMesh: word 0x11c of the fly base `tour` (1 to 99), else 0.
int32_t FLY_GetCameraMesh(int32_t tour) {   // 0x10075278
    if (tour > 0 && tour < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[tour]);
        if (base != nullptr) return *reinterpret_cast<int32_t*>(base + 0x11c);
    }
    return 0;
}
