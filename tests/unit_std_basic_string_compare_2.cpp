// Test library for the std::basic_string<char> members compare / substring constructor / append (unit v5146;
// libtpw_unit_std_basic_string_compare_2.dylib, tests/test_std_basic_string_compare_2.py), main app:
//   src/game/MslSupport.cpp   msl_char_string::compare, construct, append; the global gMslCharNul
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: operator new / new[] (the size; arena memory
// that is zeroed), __throw (checksums of the type name and message, the labels of the vtable and destructor vector, the counter),
// change_size (the length before, the new length, the flag; it moves the data into a fresh block with 0x800 spare bytes before it).
// strlen, strcpy, memcpy, memset and memcmp are the C library's on both sides. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

int64_t I(uint64_t v) { return static_cast<int64_t>(v); }

int64_t Checksum(const char* text) {
    uint32_t size = static_cast<uint32_t>(std::strlen(text));
    uint32_t h = size;
    for (uint32_t i = 0; i < size; i++) h = h * 31 + static_cast<uint8_t>(text[i]);
    return h;
}

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) { return std::strlen(s); }
char* MslStrcpy(char* d, const char* s) { return std::strcpy(d, s); }

void* MslNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
}

void* MslNewArray(uint32_t size) {
    ev("newa", {I(size)});
    return tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* o = static_cast<const MslExceptionObject*>(object);
    int64_t count = o->mRc.mCount != nullptr ? static_cast<int64_t>(static_cast<uint32_t>(*o->mRc.mCount)) : 0xffffffffLL;
    ev("throw", {Checksum(typeName), I(lab(o->mVtbl)), I(lab(dtorTV)), Checksum(o->mRc.mPtr), count});
}

}  // namespace IME

namespace msl_char_string {

void change_size(MslCharString* self, uint32_t n, bool keep) {
    MslCharRep* rep = self->mRep;
    uint32_t oldLength = rep->len;
    ev("change_size", {I(oldLength), I(n), I(keep ? 1 : 0)});
    uint8_t* block = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(n + 0x1000));
    uint32_t count = oldLength < n ? oldLength : n;
    std::memcpy(block + 0x800, rep->data, count);
    rep->len = n;
    rep->cap = n;
    rep->data = reinterpret_cast<char*>(block + 0x800);
}

}  // namespace msl_char_string

extern "C" {

uint64_t tpw_sbc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblA", IME::gMslExcVtblA},
        {"vtblLen", IME::gMslLengthErrorVtbl},
        {"dtorOor", IME::gMslOutOfRangeDtorTV},
        {"dtorLen", IME::gMslLengthErrorDtorTV},
        {"nul", &gMslCharNul},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sbc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sbc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslCharString));
    out[n++] = static_cast<int64_t>(sizeof(MslCharRep));
    out[n++] = static_cast<int64_t>(offsetof(MslCharRep, data));
    return n;
}

uint32_t tpw_sbc_compare(uint64_t self, uint32_t pos, uint32_t n1, uint64_t s, uint32_t n2) {
    return static_cast<uint32_t>(msl_char_string::compare(reinterpret_cast<const MslCharString*>(self), pos, n1,
                                                          reinterpret_cast<const char*>(s), n2));
}

uint64_t tpw_sbc_construct(uint64_t self, uint64_t src, uint32_t pos, uint32_t n) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(msl_char_string::construct(
        reinterpret_cast<MslCharString*>(self), reinterpret_cast<const MslCharString*>(src), pos, n)));
}

uint64_t tpw_sbc_append(uint64_t self, uint32_t n, int32_t c) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        msl_char_string::append(reinterpret_cast<MslCharString*>(self), n, static_cast<char>(c))));
}

}  // extern "C"
