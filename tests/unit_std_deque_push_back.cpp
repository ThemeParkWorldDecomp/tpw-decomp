// Test library for std::deque<errorthingy>::push_back and grow_buf_back (unit v5243; libtpw_unit_std_deque_push_back.dylib,
// tests/test_std_deque_push_back.py), main app:
//   src/game/MslSupport.cpp   push_back, grow_buf_back, gMslErrorDequePad
//
// The callees outside the unit are fakes that record what the real ones read: the runtime (new, new[], delete, strlen, strcpy,
// __throw, the counted string constructor) and the block table (__cdeque<const void*>: operator[], reserve, resize, pop_front,
// push_back, swap, do_assign). The table fakes have simple deterministic effects, the same as the module's PowerPC stubs.
// Pointers in events are labels: an index into the registry of the blocks of the case and the offset inside.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"

using namespace calllog;

uint64_t gMslMacAddressHigh = 0;

namespace IME {
char gMslExcVtblB[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

using Cd = std::__cdeque<const void*, std::allocator<const void*>>;

struct Ent {
    uintptr_t addr;
    uint32_t size;
    int32_t kind;   // 0 words, 1 bytes
};
std::vector<Ent> gReg;
uintptr_t gLastNewA = 0;

int64_t Lab(uintptr_t p) {
    if (p == 0) return 0;
    for (size_t i = 0; i < gReg.size(); ++i)
        if (p >= gReg[i].addr && p < gReg[i].addr + gReg[i].size) return static_cast<int64_t>(((i + 1) << 16) | (p - gReg[i].addr));
    return -1;
}

int64_t S(uint32_t v) { return static_cast<int32_t>(v); }

int64_t Hash(const char* s) {
    uint32_t h = 0;
    if (s)
        for (; *s; ++s) h = h * 31 + static_cast<uint8_t>(*s);
    return S(h);
}

void* NewBlock(uint32_t size, int32_t kind, uint32_t alloc) {
    void* p = tpw::ArenaAllocZeroed(alloc ? alloc : 4);
    gReg.push_back({reinterpret_cast<uintptr_t>(p), size, kind});
    return p;
}

uint32_t* W(const void* cd) { return reinterpret_cast<uint32_t*>(const_cast<void*>(cd)); }
uint32_t* Tab(const uint32_t* cd) {
    return cd[3] == 0 ? nullptr : reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cd[3]));
}

}  // namespace

// ---- the runtime
size_t IME::MslStrlen(const char* s) {
    ev("strlen", {Hash(s)});
    return std::strlen(s);
}
void* IME::MslNew(uint32_t size) {
    ev("new", {S(size)});
    return NewBlock(size, 0, size);
}
void* IME::MslNewArray(uint32_t size) {
    ev("new_a", {S(size)});
    void* p = NewBlock(size, 1, size);
    gLastNewA = reinterpret_cast<uintptr_t>(p);
    return p;
}
void IME::MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {reinterpret_cast<uintptr_t>(p) == gLastNewA ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}
char* IME::MslStrcpy(char* d, const char* s) {
    ev("strcpy", {reinterpret_cast<uintptr_t>(d) == gLastNewA ? 1 : 0, Hash(s)});
    std::strcpy(d, s);
    return d;
}
void IME::MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* o = static_cast<const MslExceptionObject*>(object);
    ev("throw", {Hash(typeName), o->mVtbl == gMslLengthErrorVtbl ? 1 : 0, dtorTV == gMslLengthErrorDtorTV ? 1 : 0, Hash(o->mRc.mPtr)});
}

// ---- the block table
void errdeque_callee::OperatorDelete(void* p) { ev("delete", {Lab(reinterpret_cast<uintptr_t>(p))}); }

void* errdeque_callee::CdequeIndex(void* cdeque, uint32_t index) {
    uint32_t* d = W(cdeque);
    ev("cd_index", {Lab(reinterpret_cast<uintptr_t>(cdeque)), S(index)});
    if (d[0] == 0) return Tab(d);
    return Tab(d) + ((d[2] + index) % d[0]);
}

void errdeque_callee::CdequeReserve(void* cdeque, uint32_t n) {
    uint32_t* d = W(cdeque);
    ev("cd_reserve", {Lab(reinterpret_cast<uintptr_t>(cdeque)), S(n)});
    if (n > d[0]) {
        uint32_t* t = static_cast<uint32_t*>(NewBlock(192, 0, 192));
        uint32_t* old = Tab(d);
        if (d[0])
            for (uint32_t i = 0; i < d[1]; ++i) t[i] = old[(d[2] + i) % d[0]];
        d[0] = n;
        d[2] = 0;
        d[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(t));
    }
}

void errdeque_callee::CdequeResize(void* cdeque, uint32_t n, const void* fill) {
    uint32_t* d = W(cdeque);
    ev("cd_resize", {Lab(reinterpret_cast<uintptr_t>(cdeque)), S(n), S(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fill)))});
    d[1] = n;
}

void errdeque_callee::CdequePopFront(void* cdeque) {
    uint32_t* d = W(cdeque);
    ev("cd_pop", {Lab(reinterpret_cast<uintptr_t>(cdeque))});
    if (d[1]) {
        d[1]--;
        if (d[0]) {
            d[2]++;
            if (d[2] >= d[0]) d[2] -= d[0];
        }
    }
}

void errdeque_callee::CdequePushBack(void* cdeque, const uint32_t* value) {
    uint32_t* d = W(cdeque);
    ev("cd_push", {Lab(reinterpret_cast<uintptr_t>(cdeque)), value == &gMslErrorDequePad ? 1 : 0, S(*value)});
    d[1]++;
}

void errdeque_callee::CdequeDoAssign(void* cdeque, const MslErrorDequeAssignIter& b, const MslErrorDequeAssignIter& e) {
    uint32_t* d = W(cdeque);
    uint32_t* bc = W(b.mCdeque);
    uint32_t* ec = W(e.mCdeque);
    uint32_t boff = static_cast<uint32_t>((b.mSlot - Tab(bc)) * 4);
    uint32_t eoff = static_cast<uint32_t>((e.mSlot - Tab(ec)) * 4);
    ev("cd_assign", {Lab(reinterpret_cast<uintptr_t>(cdeque)), Lab(reinterpret_cast<uintptr_t>(b.mCdeque)), S(b.mIndex), S(boff),
                     Lab(reinterpret_cast<uintptr_t>(e.mCdeque)), S(e.mIndex), S(eoff)});
    uint32_t len = e.mIndex - b.mIndex;
    uint32_t start = boff / 4;
    uint32_t* st = Tab(bc);
    if (bc[0] && d[0] && len <= 64) {
        uint32_t* dt = Tab(d);
        for (uint32_t i = 0; i < len; ++i) dt[(d[2] + i) % d[0]] = st[(start + i) % bc[0]];
    }
    d[1] = len;
}

void std::__cdeque<const void*, std::allocator<const void*>>::swap(__cdeque& other) {
    ev("cd_swap", {Lab(reinterpret_cast<uintptr_t>(this)), Lab(reinterpret_cast<uintptr_t>(&other))});
    uint32_t t = mWord0;
    mWord0 = other.mWord0;
    other.mWord0 = t;
    t = mWord4;
    mWord4 = other.mWord4;
    other.mWord4 = t;
    t = mWord8;
    mWord8 = other.mWord8;
    other.mWord8 = t;
    t = mWordC;
    mWordC = other.mWordC;
    other.mWordC = t;
}

extern "C" {

uint64_t tpw_sdp_addr(const char* name) {
    if (std::strcmp(name, "pad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gMslErrorDequePad));
    return 0;
}

uint64_t tpw_sdp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sdp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDeque));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mBlocks));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mFirst));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDeque, mSize));
    out[n++] = static_cast<int64_t>(sizeof(Cd));
    return n;
}

void tpw_sdp_set_high(uint64_t high) { gMslMacAddressHigh = high; }
void tpw_sdp_begin() {
    gReg.clear();
    gLastNewA = 0;
}
void tpw_sdp_reg(uint64_t addr, uint32_t size, int32_t kind) { gReg.push_back({static_cast<uintptr_t>(addr), size, kind}); }
uint32_t tpw_sdp_count() { return static_cast<uint32_t>(gReg.size()); }
void tpw_sdp_entry(uint32_t i, uint64_t* out) {
    out[0] = gReg[i].addr;
    out[1] = gReg[i].size;
    out[2] = static_cast<uint64_t>(gReg[i].kind);
}

void tpw_sdp_push_back(uint64_t deque, uint64_t record) {
    MslErrorDeque_push_back(reinterpret_cast<MslErrorDeque*>(static_cast<uintptr_t>(deque)),
                            reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(record)));
}

uint32_t tpw_sdp_grow(uint64_t deque, uint32_t from, uint64_t spare, uint64_t added, uint32_t to) {
    return MslErrorDeque_grow_buf_back(reinterpret_cast<MslErrorDeque*>(static_cast<uintptr_t>(deque)), from,
                                       reinterpret_cast<Cd*>(static_cast<uintptr_t>(spare)),
                                       reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(added)), to);
}

}  // extern "C"
