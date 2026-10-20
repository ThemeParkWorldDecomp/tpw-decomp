// std::basic_filebuf<char>::seekpos 0x10070eb4, seekoff 0x10071058, overflow 0x10071294 (unit v5233).
#include "std_basic_filebuf.h"

namespace {
namespace fc = filebuf_char_callee;
namespace fw = filebuf_wseek_callee;

inline void* Cvt(uint32_t p) { return reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }
}  // namespace

FposInt BasicFilebufChar::seekpos(int32_t off, int32_t state, uint32_t mode) {
    const FposInt fail = {-1, 0};
    if (mFile == 0 || (mode & 0x18) == 0) return fail;
    int32_t enc = fw::CodecvtEncoding(Cvt(mCodecvt));
    if ((mode & 0x10) != 0 && mFlag3e != 0 && enc < 0) {
        if ((fc::Unshift(this) & 0xff) == 0) return fail;
    }
    if (fc::Fseek(mFile, off, 0) != 0) return fail;
    if ((mode & 8) != 0) {
        uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = p;
        mGptr = p;
        mEgptr = p;
    }
    mFlag3e = 0;
    return FposInt{off, state};
}

FposInt BasicFilebufChar::seekoff(int32_t off, int32_t dir, uint32_t mode) {
    const FposInt fail = {-1, 0};
    int32_t enc = fw::CodecvtEncoding(Cvt(mCodecvt));
    if (mFile == 0 || (off != 0 && enc <= 0)) return fail;
    if ((dir != 2 || off != 0) && mFlag3e != 0 && enc < 0) {
        if ((fc::Unshift(this) & 0xff) == 0) return fail;
    }
    int32_t whence;
    if (dir == 3) return fail;
    if (dir < 3) {
        if (dir == 1) {
            whence = 0;
        } else if (dir > 1) {
            whence = 1;
            if ((mode & 8) != 0) off = off - static_cast<int32_t>(mEgptr - mGptr);
        } else {
            return fail;
        }
    } else {
        if (dir >= 5) return fail;
        whence = 2;
    }
    if (enc < 0) enc = 0;
    if (fc::Fseek(mFile, enc * off, whence) != 0) return fail;
    if ((mode & 8) != 0) {
        uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = p;
        mGptr = p;
        mEgptr = p;
    }
    mFlag3e = 0;
    return FposInt{fw::Ftell(mFile), 0};
}

int32_t BasicFilebufChar::overflow(int32_t c) {
    mFlag3e = 1;
    if (mFile == 0) return -1;
    if (c == -1) return 0;
    if (mUnbuffered != 0) {
        if (fw::Fputc(c, mFile) == -1) return -1;
        return c;
    }
    void* cvt = Cvt(mCodecvt);
    int32_t enc = fw::CodecvtEncoding(cvt);
    if (enc == 1) {
        char in = static_cast<char>(c);
        char out = 0;   // the original leaves this stack byte unset unless the facet writes it
        const char* fromNext;
        char* toNext;
        int32_t r = filebuf_charout_callee::CodecvtOutChar(cvt, &mState, &in, &in + 1, &fromNext, &out, &out + 1, &toNext);
        if (r == 3) {
            out = in;
        } else if (r < 3 && r > 0) {
            return -1;
        }
        if (fw::Fputc(static_cast<int32_t>(out), mFile) == -1) return -1;
        return c;
    }
    MslCharRep* s = nullptr;
    char allocator = 0;
    filebuf_charstr_callee::StringInit(&s, 3, 0, &allocator);
    char in = static_cast<char>(c);
    // The original never leaves this loop except through the error answer (the flag it tests is never changed);
    // the fputc loop after it is unreachable.
    for (;;) {
        char* data = s->data;
        int32_t len = static_cast<int32_t>(s->len);
        const char* fromNext;
        char* toNext;
        int32_t r = filebuf_charout_callee::CodecvtOutChar(cvt, &mState, &in, &in + 1, &fromNext, data, data + len, &toNext);
        if (r == 2) {
            filebuf_charstr_callee::StringDtor(&s, -1);
            return -1;
        }
        if (r < 2) {
            if (r == 0) {
                filebuf_charstr_callee::StringResize(&s, static_cast<uint32_t>(toNext - data), 0);
            } else if (r > 0) {
                filebuf_charstr_callee::StringResize(&s, static_cast<uint32_t>(s->len) * 2 + 1, 0);
            }
        } else if (r < 4) {
            filebuf_charstr_callee::StringAssign(&s, 1, static_cast<char>(static_cast<uint8_t>(in)));
        }
    }
}
