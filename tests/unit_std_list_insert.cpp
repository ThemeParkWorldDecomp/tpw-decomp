// Test library for std::list<CToken>::insert (unit v5069; libtpw_unit_std_list_insert.dylib, tests/test_std_list_insert.py), main app:
//   src/COnlineParser_ParseGoto_std_list_insert.cpp
// The callees (operator new, new[], strlen, strcpy, the counted pointer, __throw, CToken's copy constructor) are fakes that record their
// arguments; the library globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];

size_t MslStrlen(const char* s) {
    ev("strlen", {}, {s});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {}, {s});
    std::strcpy(d, s);
    return d;
}
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rc", {p != nullptr ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* exc = static_cast<const MslExceptionObject*>(object);
    ev("throw", {exc->mVtbl == gMslLengthErrorVtbl ? 1 : 0, dtorTV == gMslLengthErrorDtorTV ? 1 : 0},
       {typeName, exc->mRc.mPtr});
}
}  // namespace IME

CToken::CToken(const CToken& other) {
    ev("CToken", {other.mWords[0], other.mWords[1], other.mWords[2], other.mWords[3]});
    std::memcpy(mWords, other.mWords, sizeof(mWords));
}

extern "C" {

uint64_t tpw_tli_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tli_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(token_list::TokenListNode, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(token_list::TokenListNode, mNext));
    out[n++] = static_cast<int64_t>(offsetof(token_list::TokenListNode, mValue));
    out[n++] = static_cast<int64_t>(sizeof(token_list::TokenListNode));
    out[n++] = static_cast<int64_t>(sizeof(CToken));
    return n;
}

void tpw_tli_insert(uint64_t result, uint64_t list, uint64_t position, uint64_t token) {
    token_list::TokenList_insert(reinterpret_cast<token_list::TokenListNode**>(static_cast<uintptr_t>(result)),
                                 reinterpret_cast<token_list::TokenList*>(static_cast<uintptr_t>(list)),
                                 reinterpret_cast<token_list::TokenListNode*>(static_cast<uintptr_t>(position)),
                                 reinterpret_cast<const CToken*>(static_cast<uintptr_t>(token)));
}

}  // extern "C"
