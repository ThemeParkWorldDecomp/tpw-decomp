// std::basic_string<char> of the Metrowerks library, main app: compare(pos, n1, s, n2) 0x10209cac (492 B), the substring
// constructor basic_string(const&, pos, n) 0x101b2918 (792 B) and append(n, c) 0x100731b4 (408 B). The string is one pointer to
// a shared representation (len, cap, refs, data; see include/std_basic_filebuf.h).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "std_basic_filebuf.h"
#include "ui_ime.h"

char gMslCharNul = 0;   // 0x102ac358

namespace {

const char kOutOfRangeName[] = "!std::exception!!std::logic_error!!std::out_of_range!!";
const char kLengthErrorName[] = "!std::exception!!std::logic_error!!std::length_error!!";

// The exception object built on the stack (vtable word, message string with its counter) and thrown. The fake __throw returns.
void ThrowMessage(const char* typeName, const char* message, const void* vtbl, const void* dtorTV) {
    IME::MslExceptionObject object;
    object.mVtbl = vtbl;
    size_t length = IME::MslStrlen(message);
    object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
    object.mRc.mCount = nullptr;
    if (object.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
        object.mRc.mCount = count;
        if (count != nullptr) *count = 1;
    }
    IME::MslStrcpy(object.mRc.mPtr, message);
    IME::MslThrow(typeName, &object, dtorTV);
}

}  // namespace

namespace msl_char_string {

// 0x10209cac
int32_t compare(const MslCharString* self, uint32_t pos, uint32_t n1, const char* s, uint32_t n2) {
    if (pos > self->mRep->len)
        ThrowMessage(kOutOfRangeName, "string compare: pos > len", IME::gMslExcVtblA, IME::gMslOutOfRangeDtorTV);
    uint32_t rest = self->mRep->len - pos;
    uint32_t l1 = n1 < rest ? n1 : rest;
    uint32_t count = n2 < l1 ? n2 : l1;
    int32_t result = std::memcmp(self->mRep->data + pos, s, count);
    if (result != 0) return result;
    if (l1 < n2) return -1;
    if (l1 == n2) return 0;
    return 1;
}

// 0x101b2918
MslCharString* construct(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n) {
    self->mRep = src->mRep;
    uint32_t length = src->mRep->len;
    if (pos > length)
        ThrowMessage(kOutOfRangeName, "string copy constructor: pos > str len", IME::gMslExcVtblA, IME::gMslOutOfRangeDtorTV);
    uint32_t count = length - pos;
    if (count > n) count = n;
    if (count == length) {
        MslCharRep* rep = src->mRep;
        MslCharRep* result;
        if (rep->refs != 0) {
            rep->refs = rep->refs + 1;
            result = rep;
        } else {
            MslCharRep* copy = static_cast<MslCharRep*>(IME::MslNew(0x10));
            result = copy;
            if (copy != nullptr) {
                copy->len = rep->len;
                uint32_t len = rep->len;
                copy->cap = len + 3 - (len & 3);
                copy->refs = 1;
                copy->data = static_cast<char*>(IME::MslNew(copy->cap + 1));
                std::memcpy(copy->data, rep->data, copy->len + 1);
            }
        }
        self->mRep = result;
    } else {
        char* data = src->mRep->data;
        const char* start = data + pos;
        self->mRep = static_cast<MslCharRep*>(IME::MslNew(0x10));
        MslCharRep* rep = self->mRep;
        if (rep != nullptr) {
            rep->cap = count + 3 - (count & 3);
            rep->refs = 1;
            rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
        }
        std::memcpy(self->mRep->data, start, count);
        self->mRep->len = count;
        self->mRep->data[count] = gMslCharNul;
    }
    return self;
}

// 0x100731b4
MslCharString* append(MslCharString* self, uint32_t n, char c) {
    if (n != 0) {
        bool tooLong;
        if (n > 0xfffffffeu) tooLong = true;
        else tooLong = !(self->mRep->len <= 0xfffffffeu - n);
        if (tooLong)
            ThrowMessage(kLengthErrorName, "basic_string::append results in string > max_size", IME::gMslLengthErrorVtbl,
                         IME::gMslLengthErrorDtorTV);
        uint32_t oldLength = self->mRep->len;
        change_size(self, oldLength + n, true);
        // The Mac adds 32-bit addresses (a length near 2^32 wraps below the data, which the throw path above reaches since the
        // fake __throw returns); the signed offset is the same wrap on the host.
        std::memset(self->mRep->data + static_cast<int32_t>(oldLength), static_cast<uint8_t>(c), n);
    }
    return self;
}

}  // namespace msl_char_string
