// std::basic_streambuf<char> xsgetn / xsputn (unit v5147).
#include <cstring>

#include "std_basic_filebuf.h"

// 0x10070974. Copies what the get area holds, then pulls single characters through the virtual uflow (slot 0x2c).
// Like the original, a negative available count is returned as the base of the result.
int32_t BasicStreambufCharIO::xsgetn(char* dst, int32_t count) {
    int32_t avail = static_cast<int32_t>(mEgptr - mGptr);
    int32_t n = avail < count ? avail : count;
    if (n > 0) {
        std::memcpy(dst, mGptr, static_cast<size_t>(n));
        dst += n;
        mGptr += n;
        count -= n;
    }
    while (count > 0) {
        int32_t c;
        if (mGptr < mEgptr) {
            c = *mGptr++;
        } else {
            c = streambuf_io_callee::Uflow(this);
        }
        if (c == -1) break;
        n++;
        *dst++ = static_cast<char>(c);
        count--;
    }
    return n;
}

// 0x10070808. Copies into the put area, then pushes single characters through the virtual overflow (slot 0x38).
int32_t BasicStreambufCharIO::xsputn(const char* src, int32_t count) {
    int32_t avail = static_cast<int32_t>(mEpptr - mPptr);
    int32_t n = avail < count ? avail : count;
    if (n > 0) {
        std::memcpy(mPptr, src, static_cast<size_t>(n));
        src += n;
        mPptr += n;
        count -= n;
    }
    while (count > 0) {
        uint8_t ch = static_cast<uint8_t>(*src++);
        int32_t r;
        if (mPptr < mEpptr) {
            uint8_t* p = mPptr++;
            *p = ch;
            r = *p;
        } else {
            r = streambuf_io_callee::Overflow(this, ch);
        }
        if (r == -1) break;
        n++;
        count--;
    }
    return n;
}
