// Test library for std::list<unsigned int>::insert/merge/remove/splice (unit v5931; libtpw_unit_std_list_insert_2.dylib,
// tests/test_std_list_insert_2.py), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit (operator new, new[], delete, strlen, strcpy, __throw) are fakes that record what the PowerPC stubs record.
// Allocation comes from the arena (32-bit addressable). The globals of the library nobody names are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_list_uint.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblA[16];
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslOutOfRangeDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];

namespace {
uint32_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s; s++) h = h * 31u + static_cast<uint8_t>(*s);
    return h;
}
}  // namespace

size_t MslStrlen(const char* s) { return std::strlen(s); }
char* MslStrcpy(char* d, const char* s) { return std::strcpy(d, s); }
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void MslDelete(void* p) {
    ev("delete", {static_cast<int64_t>(*reinterpret_cast<uint32_t*>(static_cast<char*>(p) + 8))});
}
void MslDeleteArray(void*) { ev("delete[]", {}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {static_cast<int64_t>(Hash(typeName)), static_cast<int64_t>(Hash(o->mRc.mPtr)),
                 o->mVtbl == gMslLengthErrorVtbl ? 1 : 0, dtorTV == gMslLengthErrorDtorTV ? 1 : 0,
                 o->mRc.mCount ? static_cast<int64_t>(*o->mRc.mCount) : -1});
}
}  // namespace IME

extern "C" {

uint64_t tpw_sl_addr(const char*) { return 0; }

// Blocks are named by their arena offset (the Ptr32 the words hold); tpw_sl_base is the offset 0.
uint64_t tpw_sl_alloc(uint32_t size) { return tpw::ArenaOffset(tpw::ArenaAllocZeroed(size)); }
uint64_t tpw_sl_base() {
    tpw::ArenaAllocZeroed(16);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase()));
}

int32_t tpw_sl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(std_list_uint::List));
    out[n++] = static_cast<int64_t>(sizeof(std_list_uint::Node));
    return n;
}

static uint8_t* P(uint64_t offset) { return tpw::ArenaBase() + offset; }
static std_list_uint::List* L(uint64_t a) { return reinterpret_cast<std_list_uint::List*>(P(a)); }

void tpw_sl_insert(uint64_t result, uint64_t list, uint32_t pos, uint64_t value) {
    std_list_uint::Insert(reinterpret_cast<uint32_t*>(P(result)), L(list), pos, reinterpret_cast<const uint32_t*>(P(value)));
}
void tpw_sl_merge(uint64_t list, uint64_t other) { std_list_uint::Merge(L(list), L(other)); }
void tpw_sl_remove(uint64_t list, uint64_t value) { std_list_uint::Remove(L(list), reinterpret_cast<const uint32_t*>(P(value))); }
void tpw_sl_splice(uint64_t list, uint32_t pos, uint64_t other) { std_list_uint::Splice(L(list), pos, L(other)); }

}  // extern "C"
