// std::basic_filebuf<char>: constructor 0x10072bbc, open 0x10071c44, uflow 0x1007165c (unit v5046).
#include "Ptr32.h"
#include "std_basic_filebuf.h"

// The vtables the constructor stores (TOC slots -0x7020 and -0x7018); the program's own objects are not rebuilt here.
uint32_t gFilebufCharVtblA[4];   // 0x1029d0f8
uint32_t gFilebufCharVtblB[4];   // 0x1029d148

namespace {

// A 32-bit pointer field holds an offset from the arena base on the host (0: null); the vtable words are the low half of the
// static's address and the FILE* is an opaque 32-bit token.
uint32_t W(const void* p) { return p != nullptr ? static_cast<uint32_t>(tpw::ArenaOffset(p)) : 0u; }
uint8_t* P(uint32_t a) { return a != 0 ? reinterpret_cast<uint8_t*>((uintptr_t)tpw::gArenaBase + a) : nullptr; }
uint32_t Wraw(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void* File(uint32_t token) { return reinterpret_cast<void*>(static_cast<uintptr_t>(token)); }

// The fopen mode string of an openmode with the ate bit (2) cleared; the jump table at 0x1029d1c0 covers 8..0x3c and sends the
// gaps to the default (return 0).
const char* ModeString(uint32_t m) {
    switch (m) {
        case 0x10: case 0x30: return "w";
        case 0x11: return "a";
        case 0x08: return "r";
        case 0x18: return "r+";
        case 0x38: return "w+";
        case 0x14: case 0x34: return "wb";
        case 0x15: return "ab";
        case 0x0c: return "rb";
        case 0x1c: return "r+b";
        case 0x3c: return "w+b";
        default: return nullptr;
    }
}

}  // namespace

using namespace filebuf_char_callee;

BasicFilebufChar* BasicFilebufChar::construct(void* file) {
    mVtable = Wraw(gFilebufCharVtblA);
    mEback = 0;
    mGptr = 0;
    mEgptr = 0;
    mZero10[0] = 0;
    mZero10[1] = 0;
    mZero10[2] = 0;
    LocaleCtor(&mLocale);
    mVtable = Wraw(gFilebufCharVtblB);
    mFile = Wraw(file);
    mState = 0;
    mFlag3e = 0;
    mEback = W(this) + 0x30;
    mGptr = W(this) + 0x30;
    mEgptr = W(this) + 0x30;
    uint32_t loc[2];
    Getloc(loc, this);
    void* facet = UseFacetCodecvt(loc);
    mCodecvt = W(facet);
    LocaleDtor(loc, -1);
    mUnbuffered = static_cast<uint8_t>(CodecvtAlwaysNoconv(P(mCodecvt)));
    return this;
}

BasicFilebufChar* BasicFilebufChar::open(const char* name, int32_t mode) {
    if (mFile != 0) return nullptr;
    const uint32_t m = static_cast<uint32_t>(mode) & 0xfffffffdu;
    if (m - 8 > 0x34) return nullptr;
    const char* str = ModeString(m);
    if (str == nullptr) return nullptr;
    mFile = Fopen(name, str);
    if (mFile == 0) return nullptr;
    if ((static_cast<uint32_t>(mode) & 2) != 0 && Fseek(mFile, 0, 2) != 0) {
        DeleteArray(P(mEback));
        if (mFile != 0) {
            if (mFlag3e != 0) {
                if ((Unshift(this) & 0xff) == 0) return nullptr;
                mFlag3e = 0;
            }
            mEback = W(this) + 0x30;
            mGptr = W(this) + 0x30;
            mEgptr = W(this) + 0x30;
            if (Fclose(mFile) == 0) mFile = 0;
        }
        return nullptr;
    }
    return this;
}

int32_t BasicFilebufChar::uflow() {
    mFlag3e = 0;
    if (mFile == 0) return -1;
    if (mGptr < mEgptr) {
        mGptr = mGptr + 1;
        return *P(mGptr - 1);
    }

    // Puts the byte in the pushback area (the area moves down by 4 when it holds 14 bytes).
    auto pushback = [this](uint8_t byte) {
        uint32_t n = mEgptr - mEback;
        if (n == 0xe) {
            Memmove(P(mEback), P(mEback + 4), 10);
            n = static_cast<uint32_t>((static_cast<uint64_t>(0xAAAAAAABu) * n) >> 32) >> 1;
        }
        mEback = W(this) + 0x30;
        mGptr = W(this) + n + 0x31;
        mEgptr = W(this) + n + 0x31;
        *P(mGptr - 1) = byte;
    };

    if (mUnbuffered != 0) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) return -1;
        pushback(static_cast<uint8_t>(static_cast<int8_t>(c)));
        return static_cast<uint8_t>(static_cast<int8_t>(c));
    }

    void* cvt = P(mCodecvt);
    const int32_t maxlen = CodecvtMaxLength(cvt);
    if (maxlen == 1) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) return -1;
        char from = static_cast<char>(c);
        char to;
        const char* fromNext;
        char* toNext;
        const int32_t r = CodecvtInChar(cvt, &mState, &from, &from + 1, &fromNext, &to, &to + 1, &toNext);
        if (r == 3) {
            to = from;
        } else if (r < 3 && r > 0) {
            return -1;
        }
        pushback(static_cast<uint8_t>(to));
        return static_cast<uint8_t>(to);
    }

    // Several bytes per character: gather them in a string. The loop below has no exit but the returns (the original's
    // code after it is unreachable): end of file or a codecvt answer of 2 leaves with -1.
    MslStrRep* str;
    uint8_t allocator[4];
    StringCtor(&str, allocator);
    for (int32_t n = maxlen; n > 1; --n) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) {
            StringDtor(&str, -1);
            return -1;
        }
        filebuf_callee::StringAppend(&str, 1, static_cast<char>(c));
    }
    for (;;) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) {
            StringDtor(&str, -1);
            return -1;
        }
        filebuf_callee::StringAppend(&str, 1, static_cast<char>(c));
        const char* from = reinterpret_cast<const char*>(P(str->data));
        char to;
        const char* fromNext;
        char* toNext;
        const int32_t r = CodecvtInChar(cvt, &mState, from, from + str->len, &fromNext, &to, &to + 1, &toNext);
        if (r == 3) {
            to = *filebuf_callee::StringIndex(&str, 0);
        } else if (r < 3 && r >= 2) {
            StringDtor(&str, -1);
            return -1;
        }
    }
}
