// std::basic_string<char> of the Metrowerks library, main app: replace(pos, n1, n2, c) 0x10002d20 and init(n, c, allocator) 0x10072e6c.
// The throw paths build the exception object by hand (strlen, new[], counter, strcpy, __throw); the fakes of __throw return,
// so the code goes on after them, as in the original. The string is one word: the address of the shared representation.
#include <cstdint>
#include <cstring>

#include "std_basic_filebuf.h"
#include "ui_ime.h"

char gMslReplaceNulCopy = 0;    // 0x10278bd0
char gMslReplaceNulPlace = 0;   // 0x10278bd1
char gMslInitFillNul = 0;       // 0x1029d295

namespace {

const char kOutOfRangeNames[] = "!std::exception!!std::logic_error!!std::out_of_range!!";   // 0x1023bdb6
const char kLengthErrorNames[] = "!std::exception!!std::logic_error!!std::length_error!!";  // 0x1023bded, 0x1023dc4e
const char kPosMessage[] = "basic_string::replace pos out of range";                          // 0x1023bcc5
const char kLengthMessage[] = "basic_string::replace length error";                           // 0x1023bcec
const char kCtorMessage[] = "string constructor: n > max_size";                               // 0x1023db82

// The inline construction of the exception object and the throw (strlen, new[], new, strcpy, __throw).
void ThrowMsg(const char* names, const char* message, const void* vtbl, const void* dtors) {
    IME::MslExceptionObject object;
    object.mVtbl = IME::gMslExcVtblC;
    object.mVtbl = IME::gMslExcVtblB;
    uint32_t size = static_cast<uint32_t>(IME::MslStrlen(message)) + 1;
    object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(size));
    object.mRc.mCount = nullptr;
    if (object.mRc.mPtr != nullptr) {
        object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
        if (object.mRc.mCount != nullptr) *object.mRc.mCount = 1;
    }
    IME::MslStrcpy(object.mRc.mPtr, message);
    object.mVtbl = vtbl;
    IME::MslThrow(names, &object, dtors);
}

}  // namespace

namespace msl_char_string {

// 0x10002d20
void replace(MslCharString* self, uint32_t pos, uint32_t n1, uint32_t n2, char c) {
    if (pos > self->mRep->len)
        ThrowMsg(kOutOfRangeNames, kPosMessage, IME::gMslExcVtblA, IME::gMslOutOfRangeDtorTV);
    uint32_t avail = self->mRep->len - pos;
    uint32_t n = avail < n1 ? avail : n1;
    if (n2 > 0xfffffffeu || self->mRep->len - n > 0xfffffffeu - n2)
        ThrowMsg(kLengthErrorNames, kLengthMessage, IME::gMslLengthErrorVtbl, IME::gMslLengthErrorDtorTV);
    MslCharRep* rep = self->mRep;
    uint32_t newLen = n2 + (rep->len - n);
    // Offsets are 32-bit sums on the Mac (they wrap), so they are formed as uint32_t before the pointer addition.
    uint32_t tail = pos + n;
    uint32_t head = pos + n2;
    if (rep->refs <= 1 && newLen <= rep->cap) {
        std::memmove(rep->data + head, rep->data + tail, rep->len - tail);
        if (n2 != 0) msl_string_callee::Memset(self->mRep->data + pos, static_cast<uint8_t>(c), n2);
        self->mRep->data[newLen] = gMslReplaceNulPlace;
        self->mRep->len = newLen;
    } else {
        MslCharRep* fresh = static_cast<MslCharRep*>(IME::MslNewRep());
        fresh->cap = newLen + 3 - (newLen & 3);
        fresh->refs = 1;
        fresh->data = static_cast<char*>(IME::MslNew(fresh->cap + 1));
        msl_string_callee::Memcpy(fresh->data, self->mRep->data, pos);
        if (n2 != 0) msl_string_callee::Memset(fresh->data + pos, static_cast<uint8_t>(c), n2);
        msl_string_callee::Memcpy(fresh->data + head, self->mRep->data + tail, self->mRep->len - tail);
        fresh->data[newLen] = gMslReplaceNulCopy;
        fresh->len = newLen;
        MslCharRep* old = self->mRep;
        if (old->refs == 0) old->refs = 1;
        old->refs = old->refs - 1;
        if (old->refs != 0) old = nullptr;
        self->mRep = old;
        if (self->mRep != nullptr) {
            IME::MslDelete(self->mRep->data);
            IME::MslDelete(self->mRep);
        }
        self->mRep = fresh;
    }
}

// 0x10072e6c
void init(MslCharString* self, uint32_t n, char c, const void* allocator) {
    (void)allocator;
    if (n > 0xfffffffeu)
        ThrowMsg(kLengthErrorNames, kCtorMessage, IME::gMslLengthErrorVtbl, IME::gMslLengthErrorDtorTV);
    self->mRep = static_cast<MslCharRep*>(IME::MslNewRep());
    MslCharRep* rep = self->mRep;
    if (rep != nullptr) {
        rep->cap = n + 3 - (n & 3);
        rep->refs = 1;
        rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
    }
    msl_string_callee::Memset(self->mRep->data, static_cast<uint8_t>(c), n);
    self->mRep->len = n;
    self->mRep->data[n] = gMslInitFillNul;
}

}  // namespace msl_char_string
