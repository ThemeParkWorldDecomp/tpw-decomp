// Unit v51057: small e-mail functions on Mac-layout objects (the address book cursor, the outbox's unmark calls, the MSL ios_base and
// list accessors, the destructors the e-mail classes share). Objects are byte pointers with words at the Mac's offsets.
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <ios>
#include <list>
#include <string>
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_list_uint.h"
#include "platform/arena.h"

extern uint32_t gEmailBlob;   // 0x10396aa8 (also declared in FS_Online.h)

uint32_t gStringListInitWord;   // 0x102b1794
uint32_t gUintListInitWord;     // 0x102b1790

namespace {

// Words are arena offsets (0: null); the globals the code stores or answers are truncated addresses (AG).
inline uint8_t* P(uint32_t address) { return address != 0 ? tpw::gArenaBase + address : nullptr; }
inline uint32_t A(const void* pointer) { return tpw::ArenaOffset(pointer); }
inline uint32_t AG(const void* pointer) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)); }
inline uint32_t& W(const uint8_t* base, uint32_t offset) {
    return *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(base) + offset);
}

// The list constructors: the first word is copied from a global, the sentinel (+4) points at itself (the two words the original
// loads from uninitialised stack slots are overwritten at once).
uint8_t* ListInit(uint8_t* self, uint32_t initWord) {
    W(self, 0) = initWord;
    uint32_t sentinel = A(self) + 4;
    W(self, 4) = sentinel;
    W(self, 8) = sentinel;
    return self;
}

}  // namespace

namespace email_callee {
// 0x102029f4: the mail connection's callback: (a, b, data, context) calls context->Callback(a, b, data).
void MailCbfn(int32_t a, int32_t b, uint8_t* data, void* context) {
    static_cast<Email*>(context)->Callback(static_cast<uint32_t>(a), b, data);
}
}  // namespace email_callee

namespace v51057 {

// 0x10206594: the cursor (+0xc) is a node; at the sentinel (this + 4) the answer is the address of gEmailBlob (the TOC slot).
uint32_t AddressBookNext(uint8_t* self) {
    uint32_t current = W(self, 0xc);
    if (current != A(self) + 4) {
        W(self, 0xc) = W(P(current), 4);
        return current + 8;
    }
    return AG(&gEmailBlob);
}

// 0x10206534
uint32_t AddressBookFirst(uint8_t* self) {
    W(self, 0xc) = W(self, 8);
    return AddressBookNext(self);
}

// 0x102058b4
void OutboxUnmarkToSend(uint8_t* self) {
    uint32_t item = W(self, 0x4c);
    std_list_uint::Remove(reinterpret_cast<std_list_uint::List*>(self + 0x34), reinterpret_cast<const uint32_t*>(P(item + 8)));
}

// 0x102059a0
void OutboxUnmarkToDelete(uint8_t* self) {
    uint32_t item = W(self, 0x4c);
    std_list_uint::Remove(reinterpret_cast<std_list_uint::List*>(self + 0x40), reinterpret_cast<const uint32_t*>(P(item + 8)));
}

uint32_t IosExceptions(const uint8_t* ios) { return W(ios, 0x38); }

uint32_t* IosOrAssign(uint32_t* a, uint32_t b) {
    *a = *a | b;
    return a;
}

uint32_t IosState(uint8_t* ios) { return A(ios) + 0x34; }

uint32_t IosAnd(uint32_t a, uint32_t b) { return a & b; }

uint32_t IosRdstate(const uint8_t* ios) { return W(ios, 0x34); }

uint32_t BasicIosTie(const uint8_t* ios) { return W(ios, 0x3c); }

// cntlzw of (0 - state) >> 5: 1 only for a zero state.
uint32_t IosGood(const uint8_t* ios) { return W(ios, 0x34) == 0 ? 1u : 0u; }

// 0x10207f84: frees the object for a positive (signed 16 bit) flag. Answers this.
uint8_t* IstreamSentryDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr && flag > 0) MslOperatorDelete(self);
    return self;
}

// 0x102043a8: the stream's vtable word and the virtual ios part's back offset; a non-zero flag destroys the ios part.
uint8_t* IstreamDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        uint32_t ifstreamVt = AG(gIfstreamVtblC);
        W(self, 8) = ifstreamVt;
        uint8_t* ios = P(W(self, 0));
        W(ios, 0) = ifstreamVt + 0xc;
        ios = P(W(self, 0));
        W(ios, 0x44) = (A(self) + 0xc) - A(ios);
        if (flag != 0) {
            if (A(self) + 0xc != 0) {
                W(self, 0xc) = AG(gOstreamVt1);
                OstreamIosBaseDtor(reinterpret_cast<MacOstreamIos*>(self + 0xc), 0);
            }
        }
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

// 0x10208abc
uint8_t* StringListCtor(uint8_t* self) { return ListInit(self, gStringListInitWord); }

// 0x1020951c
uint8_t* UintListCtor(uint8_t* self) { return ListInit(self, gUintListInitWord); }

// 0x10203a4c
uint8_t* UintListDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        std_list_uint::Clear(reinterpret_cast<std_list_uint::List*>(self));
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

// 0x1020236c
uint8_t* StringListDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        email_callee::ListClear(self);
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

// 0x1020a0d4 / 0x1020a068: a string is a word that points at its rep: the size is the rep's word +0, the data pointer word +0xc.
uint32_t StringSize(const uint8_t* str) { return W(P(W(str, 0)), 0); }

uint32_t StringBegin(const uint8_t* str) { return W(P(W(str, 0)), 0xc); }

// 0x10209f04: a < b is compare(a, 0, a.size(), b.begin(), b.size()) < 0 (signed).
uint32_t StringLess(const uint8_t* a, const uint8_t* b) {
    uint32_t size = StringSize(b);
    uint32_t begin = StringBegin(b);
    uint32_t sizeA = StringSize(a);
    int32_t result = msl_char_string::compare(reinterpret_cast<const MslCharString*>(a), 0, sizeA, reinterpret_cast<const char*>(P(begin)), size);
    return result < 0 ? 1u : 0u;
}

// 0x10201fcc
uint8_t* CSockDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        W(self, 0) = 0x102b1638;
        csock_callee::SockDtor(self, 0);
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

}  // namespace v51057
