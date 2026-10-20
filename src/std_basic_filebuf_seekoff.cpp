// std::basic_filebuf<wchar_t>::seekoff, seekpos, overflow (unit v5093).
#include "std_basic_filebuf.h"

using namespace filebuf_wseek_callee;

namespace {

void* CodecvtOf(uint32_t p) { return reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }

// The original's inlined release of the reference-counted string (refs 0 is first made 1).
void ReleaseStr(MslStrRep*& s) {
    if (s->refs == 0) s->refs = 1;
    int32_t t = s->refs;
    s->refs = t - 1;
    if (t - 1 != 0) s = nullptr;
    if (s != nullptr) {
        Delete(reinterpret_cast<void*>(static_cast<uintptr_t>(s->data)));
        Delete(s);
    }
}

}  // namespace

// 0x10220cd4
FposInt BasicFilebufWchar::seekoff(int32_t off, int32_t dir, uint32_t mode) {
    const FposInt fail = {-1, 0};
    int32_t n = CodecvtEncoding(CodecvtOf(mCodecvt));
    if (mFile == 0 || (off != 0 && n <= 0)) return fail;
    if (!(dir == 2 && off == 0)) {
        if (mFlag4c != 0 && n < 0) {
            if (!Unshift(this)) return fail;
        }
    }
    int32_t whence;
    if (dir == 3) return fail;
    if (dir < 3) {
        if (dir == 1) {
            whence = 0;
        } else if (dir > 1) {  // 2
            whence = 1;
            if (mode & 8) {
                int32_t diff = static_cast<int32_t>(mEgptr - mGptr);
                off = static_cast<int32_t>(static_cast<uint32_t>(off) - static_cast<uint32_t>(diff / 2));
            }
        } else {
            return fail;
        }
    } else {
        if (dir >= 5) return fail;
        whence = 2;  // 4
    }
    if (n < 0) n = 0;
    if (Fseek(mFile, static_cast<int32_t>(static_cast<uint32_t>(n) * static_cast<uint32_t>(off)), whence) != 0) return fail;
    if (mode & 8) {
        uint32_t self30 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = self30;
        mGptr = self30;
        mEgptr = self30;
    }
    mFlag4c = 0;
    FposInt r;
    r.off = Ftell(mFile);
    r.state = 0;
    return r;
}

// 0x10220b30
FposInt BasicFilebufWchar::seekpos(int32_t off, int32_t state, uint32_t mode) {
    const FposInt fail = {-1, 0};
    if (mFile == 0 || (mode & 0x18) == 0) return fail;
    int32_t n = CodecvtEncoding(CodecvtOf(mCodecvt));
    if ((mode & 0x10) && mFlag4c != 0 && n < 0) {
        if (!Unshift(this)) return fail;
    }
    if (Fseek(mFile, off, 0) != 0) return fail;
    if (mode & 8) {
        uint32_t self30 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = self30;
        mGptr = self30;
        mEgptr = self30;
    }
    mFlag4c = 0;
    FposInt r;
    r.off = off;
    r.state = state;
    return r;
}

// 0x10220f18. The listing's loop never leaves except by the error result (r28 stays 1); the code after it
// (the fputc loop at 0x102211cc) is unreachable and is not translated.
char16_t BasicFilebufWchar::overflow(char16_t c) {
    mFlag4c = 1;
    if (mFile == 0) return 0xffff;
    if (c == 0xffff) return 0;
    if (mUnbuffered != 0) {
        if (Fputc(c, mFile) == -1) return 0xffff;
        return c;
    }
    void* cvt = CodecvtOf(mCodecvt);
    int32_t n = CodecvtEncoding(cvt);
    if (n == 1) {
        char16_t ch = c;
        const char16_t* fromNext;
        char b = 0;
        char* toNext;
        int32_t r = CodecvtOut(cvt, mState, &ch, &ch + 1, &fromNext, &b, &b + 1, &toNext);
        if (r == 3) {
            b = static_cast<char>(ch);
        } else if (r >= 1 && r < 3) {
            return 0xffff;
        }
        if (Fputc(static_cast<int32_t>(b), mFile) == -1) return 0xffff;
        return c;
    }
    MslStrRep* str = nullptr;
    char alloc;
    StringInit(&str, 3, 0, &alloc);
    char16_t ch = c;
    for (;;) {
        char* to = reinterpret_cast<char*>(static_cast<uintptr_t>(str->data));
        int32_t len = str->len;
        const char16_t* fromNext;
        char* toNext;
        int32_t r = CodecvtOut(cvt, mState, &ch, &ch + 1, &fromNext, to, to + len, &toNext);
        if (r == 2) {
            ReleaseStr(str);
            return 0xffff;
        }
        if (r == 0) {
            StringResize(&str, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(toNext) - reinterpret_cast<uintptr_t>(to)), 0);
        } else if (r == 1) {
            StringResize(&str, static_cast<uint32_t>(str->len) * 2 + 1, 0);
        } else if (r == 3) {
            StringAssign(&str, 1, static_cast<char>(ch));
        }
    }
}
