// Test library for the MSL std::list<std::string> members (unit v5086; libtpw_unit_std_list_std_basic_string_std_allocator_std_basi.dylib,
// tests/test_std_list_std_basic_string_std_allocator_std_basi.py), main app:
//   src/ui_Postcard_panel_std_list_std_basic_string_std_allocator_std_basi.cpp   insert, sort, erase (two), merge
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: operator new and new[] hand out blocks of a pool the test
// allocated, delete, throw, the _RefCountedPtr constructor, the string copy constructor, operator<, list clear and splice. The globals of
// other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

uintptr_t gPool[16];
int gPoolNext = 0;

int64_t Fnv(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

void* Take(uint32_t size, const char* name) {
    void* p = reinterpret_cast<void*>(gPool[gPoolNext++]);
    ev(name, {static_cast<int64_t>(size), lab(p)});
    return p;
}

}  // namespace

namespace IME {

size_t MslStrlen(const char* s) { return std::strlen(s); }
char* MslStrcpy(char* d, const char* s) { return std::strcpy(d, s); }
void* MslNew(uint32_t size) { return Take(size, "new"); }
void* MslNewArray(uint32_t size) { return Take(size, "new[]"); }
void MslDelete(void* p) { ev("delete", {lab(p)}); }

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* o = static_cast<const MslExceptionObject*>(object);
    int64_t vtbl = o->mVtbl == gMslLengthErrorVtbl ? 1 : o->mVtbl == gMslExcVtblB ? 2 : o->mVtbl == gMslExcVtblC ? 3 : 0;
    ev("throw", {Fnv(typeName), Fnv(o->mRc.mPtr), vtbl, dtorTV == gMslLengthErrorDtorTV ? 1 : 0, lab(o->mRc.mPtr), lab(o->mRc.mCount)});
}

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {lab(self), lab(p)});
    self->mPtr = p;
    self->mCount = nullptr;
}

void MslString_construct(MslString* self, const MslString* src, uint32_t pos, uint32_t n) {
    ev("strctor", {lab(self), lab(src), static_cast<int64_t>(pos), static_cast<int64_t>(n)});
    *self = *src;
}

bool MslString_less(const MslString* a, const MslString* b) {
    ev("lt", {lab(a), lab(b)});
    return reinterpret_cast<uintptr_t>(a->mRep) < reinterpret_cast<uintptr_t>(b->mRep);
}

void MslStrList_clear(MslStrList* self) {
    ev("clear", {lab(self)});
    MslStrNode* sentinel = reinterpret_cast<MslStrNode*>(&self->mHead);
    self->mSize = 0;
    self->mHead.mPrev = sentinel;
    self->mHead.mNext = sentinel;
}

void MslStrList_splice(MslStrList* self, MslStrNode* pos, MslStrList* other) {
    ev("splice", {lab(self), lab(pos), lab(other)});
    MslStrNode* sentinel = reinterpret_cast<MslStrNode*>(&other->mHead);
    if (other->mHead.mNext != sentinel) {
        MslStrNode* first = other->mHead.mNext;
        MslStrNode* last = other->mHead.mPrev;
        MslStrNode* pp = pos->mPrev;
        pp->mNext = first;
        first->mPrev = pp;
        last->mNext = pos;
        pos->mPrev = last;
        self->mSize = self->mSize + other->mSize;
        other->mSize = 0;
        other->mHead.mPrev = sentinel;
        other->mHead.mNext = sentinel;
    }
}

}  // namespace IME

extern "C" {

uint64_t tpw_sls_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_sls_pool(const uint64_t* blocks, int32_t n) {
    gPoolNext = 0;
    for (int i = 0; i < n; ++i) gPool[i] = static_cast<uintptr_t>(blocks[i]);
}

// sizeof and offsets the module relies on: the pointer width, the list head, the node, the representation
int32_t tpw_sls_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(IME::MslStrNode));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrNode, mStr));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslStrList));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrList, mHead));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslStrRep));
    out[n++] = static_cast<int64_t>(offsetof(IME::MslStrRep, mData));
    return n;
}

static IME::MslStrList* L(uint64_t a) { return reinterpret_cast<IME::MslStrList*>(static_cast<uintptr_t>(a)); }
static IME::MslStrNode* N(uint64_t a) { return reinterpret_cast<IME::MslStrNode*>(static_cast<uintptr_t>(a)); }

uint64_t tpw_sls_insert(uint64_t list, uint64_t pos, uint64_t str) {
    return reinterpret_cast<uintptr_t>(
        IME::MslStrList_insert(L(list), N(pos), reinterpret_cast<const IME::MslString*>(static_cast<uintptr_t>(str))));
}
void tpw_sls_sort(uint64_t list) { IME::MslStrList_sort(L(list)); }
uint64_t tpw_sls_erase_r(uint64_t list, uint64_t first, uint64_t last) {
    return reinterpret_cast<uintptr_t>(IME::MslStrList_erase(L(list), N(first), N(last)));
}
uint64_t tpw_sls_erase_1(uint64_t list, uint64_t pos) { return reinterpret_cast<uintptr_t>(IME::MslStrList_erase1(L(list), N(pos))); }
void tpw_sls_merge(uint64_t list, uint64_t other) { IME::MslStrList_merge(L(list), L(other)); }

}  // extern "C"
