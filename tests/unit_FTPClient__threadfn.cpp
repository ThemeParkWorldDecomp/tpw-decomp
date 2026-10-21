// Test library for FTPClient::_threadfn and small library functions on the Mac's word layouts (unit v51070;
// libtpw_unit_FTPClient__threadfn.dylib, tests/test_FTPClient__threadfn.py), main app:
//   src/game/MslSupport.cpp, src/game/online/FTPClient.cpp
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs). Pointers are
// logged relative to the case's world block (gW), anything else as -1.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "ui_ime.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
uint8_t gIPSystemVtableBlock[16];       // 0x102b1938
const char* const* gIPErrorTexts;       // 0x10396d18
const int16_t* gIPErrorCodes;           // 0x10396d1c
namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {
uint64_t gW = 0;
int64_t gCondAdjust = 0;
constexpr uint64_t kWorldSize = 0x4000;

int64_t rel(const void* p) {
    uint64_t v = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
    return (v - gW < kWorldSize) ? static_cast<int64_t>(v - gW) : -1;
}

int64_t chk(const char* s) {
    int64_t sum = 0;
    for (int64_t i = 1; *s; ++s, ++i) sum += i * static_cast<uint8_t>(*s);
    return sum;
}
}  // namespace

// ---- the fakes
void FTPClient::ProcessEvents() { ev("ProcessEvents", {rel(this)}); }
int32_t bthread_cond::signal() {
    ev("signal", {rel(this) + gCondAdjust});
    return 0;
}
void Sock::Close() { ev("SockClose", {rel(this)}); }

namespace v51070_callee {
void Delete(void* p) { ev("delete", {rel(p)}); }
void DeleteArray(void* p) { ev("deleteArray", {rel(p)}); }
void* New(uint32_t size) {
    ev("new", {size});
    if (ret("new", 1) == 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void* NewArray(uint32_t size) {
    ev("newArray", {size});
    if (ret("newArray", 1) == 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
uint32_t Strlen(const char* s) {
    ev("strlen", {chk(s)});
    return static_cast<uint32_t>(std::strlen(s));
}
void Strcpy(char* dest, const char* src) { ev("strcpy", {dest != nullptr, chk(src)}); }
void Throw(const char* type, void* object, const void* destructors) {
    ev("throw", {chk(type), object != nullptr, destructors == static_cast<const void*>(IME::gMslLengthErrorDtorTV)});
}
void WStringDtor(void* string, int16_t flag) { ev("WStringDtor", {rel(string), flag}); }
void CTokenListClear(void* list) { ev("ListClear", {rel(list)}); }
int32_t WCompare(const void* self, uint32_t pos, uint32_t n1, const void* s, uint32_t n2) {
    ev("compare", {rel(self), pos, n1, rel(s), n2});
    return static_cast<int32_t>(ret("compare"));
}
int32_t NocaseCompare(const void* self, uint32_t pos, uint32_t n1, const void* s, uint32_t n2) {
    ev("ncompare", {rel(self), pos, n1, rel(s), n2});
    return static_cast<int32_t>(ret("ncompare"));
}
bool WLess(const void* a, const void* b) {
    ev("lt", {rel(a), rel(b)});
    return (ret("lt") & 1) != 0;
}
bool NocaseLess(const void* a, const void* b) {
    ev("nlt", {rel(a), rel(b)});
    return (ret("nlt") & 1) != 0;
}
void OstreamFlush(void* stream) { ev("flush", {rel(stream)}); }
}  // namespace v51070_callee

extern "C" {

uint64_t tpw_v51070_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"codes", gIPErrorCodesData},
        {"texts", gIPErrorTextsData},
        {"vt", gIPSystemVtableBlock + 8},
        {"svt", gSockVtblData},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51070_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v51070_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Sock));
    out[n++] = static_cast<int64_t>(offsetof(Sock, mSocket));
    return n;
}

void tpw_v51070_world(uint64_t w) { gW = w; }
uint64_t tpw_v51070_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }
// which 0: gIPErrorCodes, 1: gIPErrorTexts
void tpw_v51070_setptr(int32_t which, uint64_t value) {
    if (which == 0) gIPErrorCodes = reinterpret_cast<const int16_t*>(static_cast<uintptr_t>(value));
    else gIPErrorTexts = reinterpret_cast<const char* const*>(static_cast<uintptr_t>(value));
}
uint64_t tpw_v51070_getptr(int32_t which) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(which == 0 ? static_cast<const void*>(gIPErrorCodes)
                                                                       : static_cast<const void*>(gIPErrorTexts)));
}

#define P(x) reinterpret_cast<uintptr_t>(x)
#define U32P(a) reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(a))
#define CU32P(a) reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(a))

uint64_t tpw_v51070_threadfn(uint64_t a, uint64_t, uint64_t, uint64_t) {
    FTPClient* client = reinterpret_cast<FTPClient*>(static_cast<uintptr_t>(a));
    gCondAdjust = 0x2768 - static_cast<int64_t>(reinterpret_cast<uint8_t*>(client->mCondB) - reinterpret_cast<uint8_t*>(client));
    return P(FTPClient::_threadfn(client));
}
uint64_t tpw_v51070_ipctor(uint64_t a, uint64_t, uint64_t, uint64_t) {
    v51070::IPBaseCtor(U32P(a));
    return a;
}
uint64_t tpw_v51070_ipdtor(uint64_t a, uint64_t b, uint64_t, uint64_t) { return P(v51070::IPBaseDtor(U32P(a), static_cast<int16_t>(b))); }
uint64_t tpw_v51070_erase(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    return P(v51070::EraseWideStrings(U32P(a), U32P(b), static_cast<uint32_t>(c), static_cast<uint32_t>(d)));
}
uint64_t tpw_v51070_sockctor(uint64_t a, uint64_t, uint64_t, uint64_t) {
    v51070::SockCtor(U32P(a));
    return a;
}
uint64_t tpw_v51070_sockdtor(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return P(v51070::SockDtor(reinterpret_cast<Sock*>(static_cast<uintptr_t>(a)), static_cast<int16_t>(b)));
}
uint64_t tpw_v51070_splice(uint64_t a, uint64_t b, uint64_t c, uint64_t) {
    v51070::StrListSplice(U32P(a), static_cast<uint32_t>(b), U32P(c));
    return a;
}
uint64_t tpw_v51070_listdtor(uint64_t a, uint64_t b, uint64_t, uint64_t) { return P(v51070::CTokenListDtor(U32P(a), static_cast<int16_t>(b))); }
uint64_t tpw_v51070_tokendtor(uint64_t a, uint64_t b, uint64_t, uint64_t) { return P(v51070::CTokenDtor(U32P(a), static_cast<int16_t>(b))); }
uint64_t tpw_v51070_weq(uint64_t a, uint64_t b, uint64_t, uint64_t) { return v51070::WStringEq(CU32P(a), CU32P(b)); }
uint64_t tpw_v51070_nweq(uint64_t a, uint64_t b, uint64_t, uint64_t) { return v51070::NocaseWStringEq(CU32P(a), CU32P(b)); }
uint64_t tpw_v51070_ostream(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return P(v51070::OstreamSentryDtor(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(a)), static_cast<int16_t>(b)));
}
uint64_t tpw_v51070_tie(uint64_t a, uint64_t, uint64_t, uint64_t) {
    return v51070::IosTie(reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(a)));
}
uint64_t tpw_v51070_wfind(uint64_t a, uint64_t b, uint64_t c, uint64_t) {
    v51070::WStringSetFind(U32P(a), CU32P(b), CU32P(c));
    return a;
}
uint64_t tpw_v51070_nfind(uint64_t a, uint64_t b, uint64_t c, uint64_t) {
    v51070::NocaseSetFind(U32P(a), CU32P(b), CU32P(c));
    return a;
}
uint64_t tpw_v51070_seekpos(uint64_t a, uint64_t, uint64_t, uint64_t) {
    v51070::StreambufSeekpos(U32P(a));
    return a;
}
uint64_t tpw_v51070_seekoff(uint64_t a, uint64_t, uint64_t, uint64_t) {
    v51070::StreambufSeekoff(U32P(a));
    return a;
}
uint64_t tpw_v51070_istream(uint64_t a, uint64_t b, uint64_t, uint64_t) { return P(v51070::IstreamSentryDtor(U32P(a), static_cast<int16_t>(b))); }
uint64_t tpw_v51070_itercopy(uint64_t a, uint64_t b, uint64_t, uint64_t) { return P(v51070::ErrorDequeIterCopy(U32P(a), CU32P(b))); }

}  // extern "C"
