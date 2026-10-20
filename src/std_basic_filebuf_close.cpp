// std::basic_filebuf<wchar_t>::close, underflow, pbackfail, imbue (unit v5137).
#include "std_basic_filebuf.h"

using namespace filebuf_wclose_callee;

// The Ptr32 fields hold the low half of a host address (arena memory is above 4 GB): the high half is taken from the object.
static inline void* Hi(const void* self, uint32_t w) {
    return w ? reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(self) & ~static_cast<uintptr_t>(0xffffffffu)) | w) : nullptr;
}

// 0x10221b40
BasicFilebufWchar* BasicFilebufWchar::close() {
    if (mFile == 0) return nullptr;
    if (mFlag4c != 0) {
        if (!filebuf_wseek_callee::Unshift(this)) return nullptr;
        mFlag4c = 0;
    }
    uint32_t area = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
    mEback = area;
    mGptr = area;
    mEgptr = area;
    if (filebuf_char_callee::Fclose(mFile) != 0) return nullptr;
    mFile = 0;
    return this;
}

// 0x10221a64
uint32_t BasicFilebufWchar::underflow() {
    mFlag4c = 0;
    if (mFile == 0) return 0xffff;
    if (mGptr < mEgptr) return *static_cast<const char16_t*>(Hi(this, mGptr));
    uint32_t r = static_cast<uint32_t>(VirtualUnderflow(this));
    if ((r & 0xffff) != 0xffff) mGptr -= 2;
    return r;
}

// 0x10221360. Returns 0 for eof, otherwise the character.
char16_t BasicFilebufWchar::pbackfail(char16_t c) {
    if (mFile == 0 || !(mEback < mGptr)) return 0xffff;
    uint32_t g = mGptr - 2;
    mGptr = g;
    if (c != 0xffff) *static_cast<char16_t*>(Hi(this, g)) = c;
    return c == 0xffff ? 0 : c;
}

// 0x10220964
void BasicFilebufWchar::imbue(const void* loc) {
    streambuf_callee::LocaleAssign(reinterpret_cast<uint8_t*>(this) + 0x1c, loc);
    struct {
        uint32_t impl;
        uint32_t rc;
    } l;
    Getloc(&l, this);
    mCodecvt = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(UseFacetCodecvtW(&l)));
    if (l.rc != 0) {
        int32_t* p = static_cast<int32_t*>(Hi(this, l.rc));
        int32_t n = *p - 1;
        *p = n;
        if (n == 0) {
            LocaleImpDtor(Hi(this, l.impl), 1);
            filebuf_wseek_callee::Delete(p);
        }
    }
    mUnbuffered = static_cast<uint8_t>(filebuf_char_callee::CodecvtAlwaysNoconv(Hi(this, mCodecvt)));
}
