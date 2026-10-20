// Unit v51033: std::basic_string<char>::operator[] 0x100733b0, std::locale::locale() 0x10072d24,
// std::basic_ios<char>::widen 0x10072960, std::_RefCountedPtr<__locale_imp>::~_RefCountedPtr 0x10071f34,
// std::basic_filebuf<char>::close 0x10071b54 and underflow 0x10071a80.
#include "platform/arena.h"
#include "std_basic_filebuf.h"

// operator[]: the non-const access un-shares the string (copy on write when refs > 1), then marks the rep unshared (refs = 0).
// The catch blocks of the listing (operator delete of the new rep, __throw, __end__catch) are only reached by an exception.
char* MslCharStringIndex(MslCharString* self, uint32_t i) {
    MslCharRep* rep = self->mRep;
    if (rep->refs > 1) {
        uint32_t len = rep->len;
        MslCharRep* copy = static_cast<MslCharRep*>(filebuf_callee::New(0x10));
        if (copy) {
            copy->cap = (len + 3) - (len & 3);
            copy->refs = 1;
            copy->data = static_cast<char*>(filebuf_callee::New(copy->cap + 1));
        }
        rep->refs = rep->refs - 1;
        uint32_t n = rep->len < len ? rep->len : len;
        msl_string_callee::Memcpy(copy->data, rep->data, n + 1);
        copy->len = rep->len;
        rep = copy;
        self->mRep = rep;
    }
    rep->refs = 0;
    return rep->data + i;
}

// std::locale::locale(): copies the global locale, takes a reference, falls back to the classic locale when it has no impl.
LocaleRefPtr* LocaleConstruct(LocaleRefPtr* self) {
    self->mImp = gLocaleGlobal.mImp;
    self->mCount = gLocaleGlobal.mCount;
    if (self->mCount) *self->mCount = *self->mCount + 1;
    if (self->mImp == nullptr) self->operator=(*v51033_callee::LocaleClassic());
    return self;
}

// ~_RefCountedPtr with the hidden delete flag (signed short): the last reference destroys the impl and the count.
LocaleRefPtr* LocaleRefPtrDestroy(LocaleRefPtr* self, int16_t flag) {
    if (self) {
        int32_t* count = self->mCount;
        if (count) {
            int32_t v = *count - 1;
            *count = v;
            if (v == 0) {
                filebuf_wclose_callee::LocaleImpDtor(self->mImp, 1);
                locale_imp_callee::Delete(self->mCount);
            }
        }
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

// basic_ios<char>::widen(c) const: getloc, use_facet<ctype<char>>, virtual widen (vtable 0x1c), destroy the temporary locale.
char BasicIosWiden(const BasicIosChar* self, char c) {
    LocaleRefPtr loc{};
    filebuf_char_callee::Getloc(&loc, const_cast<BasicIosChar*>(self));
    void* facet = UseFacetCtypeChar(reinterpret_cast<const UseFacetLocale*>(&loc));
    char r = v51033_callee::CtypeWiden(facet, c);
    LocaleRefPtrDestroy(&loc, -1);
    return r;
}

BasicFilebufChar* FilebufCharClose(BasicFilebufChar* self) {
    if (self->mFile == 0) return nullptr;
    if (self->mFlag3e != 0) {
        if (static_cast<uint8_t>(filebuf_char_callee::Unshift(self)) == 0) return nullptr;
        self->mFlag3e = 0;
    }
    uint32_t buf = tpw::ArenaOffset(self) + 0x30;   // the object's own 32-bit address (arena offset on the host)
    self->mEback = buf;
    self->mGptr = buf;
    self->mEgptr = buf;
    if (filebuf_char_callee::Fclose(self->mFile) != 0) return nullptr;
    self->mFile = 0;
    return self;
}

int32_t FilebufCharUnderflow(BasicFilebufChar* self) {
    self->mFlag3e = 0;
    if (self->mFile == 0) return -1;
    if (self->mGptr < self->mEgptr)
        return *(tpw::ArenaBase() + self->mGptr);
    int32_t r = filebuf_wclose_callee::VirtualUnderflow(self);
    if (r != -1) self->mGptr = self->mGptr - 1;
    return r;
}
