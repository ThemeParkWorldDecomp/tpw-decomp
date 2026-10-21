// Test library for the nocase wide string compare and substring constructor (unit v5229; libtpw_unit_std_basic_string_compare_3.dylib,
// tests/test_std_basic_string_compare_3.py), main app:
//   src/game/MslSupport.cpp   online_callee::NocaseWStringCompare (0x10225a5c), NocaseWStringCopy (0x1022213c)
//
// The callees outside the unit are fakes that record what the real ones read as integers (texts as a checksum, pointers into the
// case's strings as byte offsets from the base the test set) and answer from the queues. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];          // 0x10278ba0
char gMslExcVtblB[16];          // 0x10278bb0
char gMslExcVtblC[16];          // 0x10278bc0
char gMslOutOfRangeDtorTV[8];   // 0x10265698
}  // namespace IME

namespace NS_MacDoze {
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count);
}

namespace {

uint64_t gDataBase = 0;   // the host address of the source string's data
uint64_t gSBase = 0;      // the host address of the compare argument

int64_t TextSum(const char* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return static_cast<int32_t>(h);
}

int64_t Off(const void* p, uint64_t base) { return static_cast<int32_t>(reinterpret_cast<uint64_t>(p) - base); }

void* Alloc(uint32_t n) { return tpw::ArenaAllocZeroed(n); }

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) {
    ev("strlen", {TextSum(s)});
    return std::strlen(s);
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {TextSum(s)});
    if (d != nullptr) std::strcpy(d, s);
    return d;
}

void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    if (ret("nullN") != 0) return nullptr;
    return Alloc(size);
}

void* MslNewArray(uint32_t size) {
    ev("newa", {static_cast<int64_t>(size)});
    if (ret("nullA") != 0) return nullptr;
    return Alloc(size);
}

void* MslNewRep() {
    ev("new", {16});
    if (ret("nullN") != 0) return nullptr;
    return Alloc(sizeof(MslWideStringRep));
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* ex = static_cast<const MslExceptionObject*>(object);
    int64_t message = ex->mRc.mPtr != nullptr ? TextSum(ex->mRc.mPtr) : -1;
    int64_t count = ex->mRc.mCount != nullptr ? *ex->mRc.mCount : -1;
    ev("throw", {TextSum(typeName), lab(const_cast<void*>(ex->mVtbl)), message, count, lab(const_cast<void*>(dtorTV))});
}

char16_t* MslWmemcpy(char16_t* d, const char16_t* s, uint32_t n) {
    ev("wmemcpy", {Off(s, gDataBase), static_cast<int64_t>(n)});
    if (d != nullptr) std::memcpy(d, s, static_cast<size_t>(n) * 2);
    return d;
}

}  // namespace IME

namespace NS_MacDoze {
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count) {
    ev("wcsnicmp", {Off(a, gDataBase), b != nullptr ? Off(b, gSBase) : -1, static_cast<int64_t>(count)});
    return static_cast<int32_t>(ret("wcsnicmp"));
}
}  // namespace NS_MacDoze

namespace online_callee {
void NocaseWStringInit(void* self, const char16_t* first, const char16_t* last) {
    (void)self;
    ev("init", {Off(first, gDataBase), Off(last, gDataBase)});
}
}  // namespace online_callee

extern "C" {

uint64_t tpw_nc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblA", IME::gMslExcVtblA},
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
        {"dtorTV", IME::gMslOutOfRangeDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_nc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_nc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideStringRep));
    return n;
}

void tpw_nc_setbase(uint64_t data, uint64_t s) {
    gDataBase = data;
    gSBase = s;
}

int32_t tpw_nc_compare(uint64_t self, uint32_t pos, uint32_t n1, uint64_t s, uint32_t n2) {
    return online_callee::NocaseWStringCompare(reinterpret_cast<const void*>(self), pos, n1, reinterpret_cast<const char16_t*>(s), n2);
}

void tpw_nc_copy(uint64_t self, uint64_t other, uint32_t pos, uint32_t count) {
    online_callee::NocaseWStringCopy(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(other), pos, count);
}

}  // extern "C"
