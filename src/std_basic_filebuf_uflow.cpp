// std::basic_filebuf<wchar_t>::uflow (unit v5025).
#include "platform/arena.h"
#include "std_basic_filebuf.h"

using namespace filebuf_callee;

namespace {

// 32-bit fields hold offsets from the arena base (the Mac's pointers).
inline void* P(uint32_t v) { return v != 0 ? static_cast<void*>(tpw::gArenaBase + v) : nullptr; }
inline uint32_t A(const void* p) { return tpw::ArenaOffset(p); }

// The refcount release the compiler inlined at every exit of the string path.
void StringRelease(MslStrRep*& str) {
    MslStrRep* rep = str;
    if (rep->refs == 0) {
        int32_t t = rep->refs;
        if (t == 0) t = 1;
        rep->refs = t;
    }
    int32_t n = rep->refs;
    rep->refs = n - 1;
    if (n - 1 != 0) rep = nullptr;
    str = rep;
    if (str != nullptr) {
        Delete(P(str->data));
        Delete(str);
    }
}

// Puts one character in the pushback buffer and points the get area behind it.
void PutBack(BasicFilebufWchar* t, uint16_t ch) {
    uint32_t eback = t->mEback;
    int32_t diff = static_cast<int32_t>(t->mEgptr - eback);
    uint32_t k = static_cast<uint32_t>(diff / 2);
    if (k == 0xe) {
        Wmemmove(P(eback), P(eback + 8), 10);
        k = static_cast<uint32_t>((static_cast<uint64_t>(0xAAAAAAABu) * 14u) >> 32) >> 1;   // 4
    }
    uint32_t self = A(t);
    t->mEback = self + 0x30;
    uint32_t p = self + (k << 1) + 0x32;
    t->mGptr = p;
    t->mEgptr = p;
    *static_cast<uint16_t*>(P(t->mGptr - 2)) = ch;
}

}  // namespace

// 0x10221410
uint32_t BasicFilebufWchar::uflow() {
    mFlag4c = 0;
    if (mFile == 0) return 0xffff;
    if (mGptr < mEgptr) {
        mGptr += 2;
        return *static_cast<uint16_t*>(P(mGptr - 2));
    }
    if (mUnbuffered != 0) {
        int32_t c = Fgetc(P(mFile));
        if (c == -1) return 0xffff;
        PutBack(this, static_cast<uint16_t>(c));
        return static_cast<uint16_t>(c);
    }
    int32_t n = CodecvtMaxLength(P(mCodecvt));
    const char* fromNext;
    char16_t* toNext;
    if (n == 1) {
        int32_t c = Fgetc(P(mFile));
        if (c == -1) return 0xffff;
        int8_t b = static_cast<int8_t>(c);
        char16_t to = 0;
        int32_t res = CodecvtIn(P(mCodecvt), mState, reinterpret_cast<const char*>(&b),
                                reinterpret_cast<const char*>(&b) + 1, &fromNext, &to, &to + 1, &toNext);
        if (res == 3) {
            to = static_cast<char16_t>(static_cast<int16_t>(b));
        } else if (res <= 3 && res >= 1) {
            return 0xffff;
        }
        PutBack(this, to);
        return to;
    }
    // Multibyte path: grow a string byte by byte until the facet converts it. The code after the
    // loop in the listing (r27 != 1) is unreachable: r27 stays 1, so only the returns below leave.
    MslStrRep* str = nullptr;
    MslStrRep* rep = static_cast<MslStrRep*>(New(0x10));
    str = rep;
    if (rep != nullptr) {
        rep->cap = 3;
        rep->refs = 1;
        rep->data = A(New(static_cast<uint32_t>(rep->cap + 1)));
    }
    *static_cast<char*>(P(str->data)) = gMslEmptyChar;
    str->len = 0;
    for (; n > 1; n--) {
        int32_t c = Fgetc(P(mFile));
        if (c == -1) {
            StringRelease(str);
            return 0xffff;
        }
        StringAppend(&str, 1, static_cast<char>(c));
    }
    for (;;) {
        int32_t c = Fgetc(P(mFile));
        if (c == -1) {
            StringRelease(str);
            return 0xffff;
        }
        StringAppend(&str, 1, static_cast<char>(c));
        char16_t to = 0;
        const char* data = static_cast<const char*>(P(str->data));
        int32_t res = CodecvtIn(P(mCodecvt), mState, data, data + str->len, &fromNext, &to, &to + 1, &toNext);
        if (res == 3) {
            to = static_cast<char16_t>(static_cast<int16_t>(*StringIndex(&str, 0)));
            (void)to;
            continue;
        }
        if (res > 3) continue;
        if (res >= 2) {
            StringRelease(str);
            return 0xffff;
        }
    }
}
