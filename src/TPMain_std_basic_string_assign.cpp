// std::basic_string<char> of the Metrowerks library: assign(str, pos, n) 0x10001c80 and change_size 0x100027b0.
// The string is one word, the address of the shared MslCharRep (len +0, cap +4, refs +8, data +0xc; refs 0 = not shared yet).
// The throw paths build the exception object by hand (strlen, new[], counter, strcpy, __throw); the fakes of __throw return,
// so the code goes on afterwards, as the original does under the test.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "std_basic_filebuf.h"
#include "ui_ime.h"

namespace msl_char_string {

namespace {

const char kExceptionNames[] = "!std::exception!!std::logic_error!!std::out_of_range!!";   // 0x1023bdb6
const char kLengthErrorNames[] = "!std::exception!!std::logic_error!!std::length_error!!";  // 0x1023bdb6 + 0x37
const char kAppendMessage[] = "basic_string::append pos out of range";                      // 0x1023bc41
const char kResizeMessage[] = "basic_string::length error.  Resizing out of range";         // 0x1023bc92

}  // namespace

// 0x100027b0: makes the string's own representation (refs <= 1) hold n characters, with a terminator after them.
void change_size(MslCharString* self, uint32_t n, bool keep) {
    if (n > 0xFFFFFFFEu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kResizeMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            object.mRc.mCount = count;
        }
        IME::MslStrcpy(object.mRc.mPtr, kResizeMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kLengthErrorNames, &object, IME::gMslLengthErrorDtorTV);
    }
    if (self->mRep->refs > 1) {
        MslCharRep* old = self->mRep;
        MslCharRep* fresh = static_cast<MslCharRep*>(IME::MslNewRep());
        fresh->cap = (n + 3) - (n & 3);
        fresh->refs = 1;
        fresh->data = static_cast<char*>(IME::MslNew(fresh->cap + 1));
        old->refs = old->refs - 1;
        if (keep) {
            uint32_t count = old->len < n ? old->len : n;
            msl_string_callee::Memcpy(fresh->data, old->data, count + 1);
            fresh->len = old->len;
        }
        self->mRep = fresh;
    }
    MslCharRep* rep = self->mRep;
    if (n > rep->cap) {
        uint32_t size = rep->cap;
        while (size < n) size <<= 1;
        if (size < rep->len) size = rep->len;
        uint32_t cap = (size + 3) - (size & 3);
        if (cap != rep->cap) {   // both listing branches (smaller, larger) do the same copy
            char* old = rep->data;
            rep->data = static_cast<char*>(IME::MslNew(cap + 1));
            msl_string_callee::Memcpy(rep->data, old, rep->len + 1);
            IME::MslDelete(old);
            rep->cap = cap;
        }
    }
    // the address add wraps at 32 bits on the Mac: an index of 2^31 or more reaches below the block
    self->mRep->data[static_cast<int64_t>(static_cast<int32_t>(n))] = gMslChangeSizeNul;
    self->mRep->len = n;
    uint32_t refs = self->mRep->refs;
    if (refs == 0) refs = 1;
    self->mRep->refs = refs;
}

// 0x10001c80: this = src[pos, pos + min(n, size - pos)).
MslCharString* assign(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n) {
    if (pos > src->mRep->len) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kAppendMessage);
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, kAppendMessage);
        object.mVtbl = IME::gMslExcVtblA;
        IME::MslThrow(kExceptionNames, &object, IME::gMslOutOfRangeDtorTV);
    }
    uint32_t rest = src->mRep->len - pos;
    uint32_t count = rest < n ? rest : n;
    MslCharRep* rep = src->mRep;
    if (count == rep->len && rep->refs != 0) {
        MslCharRep* mine = self->mRep;
        if (mine == rep) return self;
        if (mine->refs == 0) mine->refs = 1;
        uint32_t left = mine->refs - 1;
        mine->refs = left;
        if (left != 0) mine = nullptr;
        self->mRep = mine;
        if (self->mRep != nullptr) {
            IME::MslDelete(self->mRep->data);
            IME::MslDelete(self->mRep);
        }
        MslCharRep* from = src->mRep;
        MslCharRep* shared;
        if (from->refs != 0) {
            from->refs = from->refs + 1;
            shared = from;
        } else {
            shared = static_cast<MslCharRep*>(IME::MslNewRep());
            shared->len = from->len;
            shared->cap = (from->len + 3) - (from->len & 3);
            shared->refs = 1;
            shared->data = static_cast<char*>(IME::MslNew(shared->cap + 1));
            msl_string_callee::Memcpy(shared->data, from->data, shared->len + 1);
        }
        self->mRep = shared;
    } else if (self == src) {
        replace(self, 0, pos, 0, 0);
        change_size(self, count, true);
    } else {
        change_size(self, count, false);
        msl_string_callee::Memcpy(self->mRep->data, src->mRep->data + pos, count);
    }
    return self;
}

}  // namespace msl_char_string
