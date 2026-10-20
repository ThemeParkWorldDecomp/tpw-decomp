// std::basic_streambuf<wchar_t>::xsputn 0x1021feac, ::xsgetn 0x10220024.
#include "std_basic_streambuf_wchar.h"

// Bytes between two pointers as the Mac's 32-bit subtraction, halved toward zero (srawi + addze).
static int32_t HalfBytes(const char16_t* hi, const char16_t* lo) {
    int32_t d = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hi)) -
                                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lo)));
    return d / 2;
}

int32_t MslWStreambuf::xsputn(const char16_t* s, int32_t n) {
    int32_t avail = HalfBytes(mPEnd, mPNext);
    int32_t k = avail < n ? avail : n;
    if (k > 0) {
        wmemcpy(mPNext, s, static_cast<size_t>(k));
        s += k;
        mPNext = reinterpret_cast<char16_t*>(reinterpret_cast<char*>(mPNext) + k * 2);
        n -= k;
    }
    while (n > 0) {
        char16_t c = *s++;
        uint32_t r;
        if (mPNext < mPEnd) {
            char16_t* p = mPNext;
            mPNext = p + 1;
            *p = c;
            r = *p;
        } else {
            r = overflow(c);
        }
        if (static_cast<uint16_t>(r) == 0xffff) break;
        k++;
        n--;
    }
    return k;
}

int32_t MslWStreambuf::xsgetn(char16_t* s, int32_t n) {
    int32_t avail = HalfBytes(mGEnd, mGNext);
    int32_t k = avail < n ? avail : n;
    if (k > 0) {
        wmemcpy(s, mGNext, static_cast<size_t>(k));
        s += k;
        mGNext = reinterpret_cast<char16_t*>(reinterpret_cast<char*>(mGNext) + k * 2);
        n -= k;
    }
    while (n > 0) {
        uint32_t r;
        if (mGNext < mGEnd) {
            char16_t* p = mGNext;
            mGNext = p + 1;
            r = *p;
        } else {
            r = uflow();
        }
        if (static_cast<uint16_t>(r) == 0xffff) break;
        *s++ = static_cast<char16_t>(r);
        k++;
        n--;
    }
    return k;
}
