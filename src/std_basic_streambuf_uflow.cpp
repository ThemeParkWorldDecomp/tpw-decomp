// std::basic_streambuf<char> base functions (unit v5104).
#include "std_basic_filebuf.h"

// 0x10070690: the virtual at vtable 0x28 (underflow) is called; -1 is passed on, else the byte at gptr is
// consumed and returned (unsigned).
int32_t BasicStreambufChar::uflow() {
    int32_t result = -1;
    if (streambuf_callee::Underflow(this) != -1) {
        const uint8_t* p = mGptr;
        mGptr = p + 1;
        result = *p;
    }
    return result;
}

// 0x1007048c
void BasicStreambufChar::imbue(const void* loc) { streambuf_callee::LocaleAssign(&mLocale, loc); }

// 0x10070e1c: copies the locale into the hidden result.
void BasicStreambufChar::getloc(void* out) const { streambuf_callee::LocaleCopyCtor(out, &mLocale); }

// 0x10070304: a NULL this does nothing; the vtable is reset, the locale destroyed, and the object
// deleted when the (signed 16-bit) flag is positive.
BasicStreambufChar* BasicStreambufChar::destroy(int16_t flag) {
    if (this != nullptr) {
        mVtable = gFilebufCharVtblA;
        filebuf_char_callee::LocaleDtor(&mLocale, -1);
        if (flag > 0) filebuf_callee::Delete(this);
    }
    return this;
}
