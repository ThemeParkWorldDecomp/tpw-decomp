// std::basic_string<char> of the Metrowerks library, main app: constructors 0x10002338 and 0x1007363c, destructor 0x1000221c, init
// 0x10002b8c, assign(n, c) 0x100730e8, resize 0x10073558, assign(text, n) 0x101b2440, append 0x1020100c. The string is one word, the
// address of the shared representation (len +0, cap +4, refs +8, data +0xc; native data pointer on the host).
#include <cstdint>
#include <cstring>

#include "std_basic_filebuf.h"
#include "ui_ime.h"

char gMslInitNul;   // 0x10278bd2
char gMslCtorNul;   // 0x1029d294

namespace msl_string_callee {
__attribute__((weak)) void* Memcpy(void* to, const void* from, uint32_t n) { return std::memcpy(to, from, n); }
__attribute__((weak)) void* Memset(void* to, int32_t c, uint32_t n) { return std::memset(to, c, n); }
}  // namespace msl_string_callee

namespace {
const char kLengthErrorName[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1023bded
const char kMaxSizeMessage[] = "string constructor: n > max_size";                          // 0x1023bc67
}  // namespace

namespace msl_char_string {

// 0x1007363c: basic_string(const allocator&)
MslCharString* constructEmpty(MslCharString* self) {
    self->mRep = nullptr;
    self->mRep = static_cast<MslCharRep*>(IME::MslNewRep());
    MslCharRep* rep = self->mRep;
    if (rep != nullptr) {
        rep->cap = 3;
        rep->refs = 1;
        rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
    }
    self->mRep->data[0] = gMslCtorNul;
    self->mRep->len = 0;
    return self;
}

// 0x10002b8c: init(first, last, allocator, random_access_iterator_tag)
void init(MslCharString* self, const char* first, const char* last) {
    self->mRep = static_cast<MslCharRep*>(IME::MslNewRep());
    uint32_t n = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last) - reinterpret_cast<uintptr_t>(first));
    MslCharRep* rep = self->mRep;
    if (rep != nullptr) {
        rep->cap = (n + 3) - (n & 3);
        rep->refs = 1;
        rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
    }
    msl_string_callee::Memcpy(self->mRep->data, first, n);
    self->mRep->len = n;
    // The address wraps at 32 bits on the Mac: n = 0xffffffff writes one byte before the data.
    self->mRep->data[static_cast<int32_t>(n)] = gMslInitNul;
}

// 0x10002338: basic_string(const char*, const allocator&)
MslCharString* constructFromCStr(MslCharString* self, const char* text, const void* allocator) {
    (void)allocator;
    self->mRep = nullptr;
    uint32_t n = static_cast<uint32_t>(IME::MslStrlen(text));
    if (n > 0xfffffffeu) {   // n == npos: length_error("string constructor: n > max_size")
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        uint32_t size = static_cast<uint32_t>(IME::MslStrlen(kMaxSizeMessage)) + 1;
        char* message = static_cast<char*>(IME::MslNewArray(size));
        IME::MslRefCountedCharsCtor(&object.mRc, message);
        IME::MslStrcpy(object.mRc.mPtr, kMaxSizeMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kLengthErrorName, &object, IME::gMslLengthErrorDtorTV);
    }
    init(self, text, reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(text) + n));
    return self;
}

// 0x1000221c: ~basic_string(), r4 = the delete flag (a short)
MslCharString* destroy(MslCharString* self, int16_t flag) {
    if (self != nullptr) {
        MslCharRep* rep = self->mRep;
        if (rep->refs == 0) {
            uint32_t refs = rep->refs;
            if (refs == 0) refs = 1;
            rep->refs = refs;
        }
        uint32_t refs = rep->refs - 1;
        rep->refs = refs;
        if (refs != 0) rep = nullptr;
        self->mRep = rep;
        if (self->mRep != nullptr) {
            IME::MslDelete(self->mRep->data);
            IME::MslDelete(self->mRep);
        }
        if (flag > 0) IME::MslDelete(self);
    }
    return self;
}

// 0x100730e8: assign(n, c)
MslCharString* assignFill(MslCharString* self, uint32_t n, char c) {
    change_size(self, n, false);
    msl_string_callee::Memset(self->mRep->data, static_cast<uint8_t>(c), n);
    return self;
}

// 0x10073558: resize(n, c)
void resize(MslCharString* self, uint32_t n, char c) {
    uint32_t old = self->mRep->len;
    change_size(self, n, true);
    if (old < n) msl_string_callee::Memset(self->mRep->data + old, static_cast<uint8_t>(c), n - old);
}

// 0x101b2440: assign(text, n)
MslCharString* assignChars(MslCharString* self, const char* text, uint32_t n) {
    change_size(self, n, false);
    msl_string_callee::Memcpy(self->mRep->data, text, n);
    return self;
}

// 0x1020100c: append(text, n)
MslCharString* appendChars(MslCharString* self, const char* text, uint32_t n) {
    if (n != 0) {
        uint32_t old = self->mRep->len;
        change_size(self, old + n, true);
        msl_string_callee::Memcpy(self->mRep->data + old, text, n);
    }
    return self;
}

}  // namespace msl_char_string
