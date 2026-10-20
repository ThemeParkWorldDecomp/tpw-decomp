// Test library for std::basic_filebuf<wchar_t> constructor and open, and std::__tree<pair<const nocase_string, BuddyInfo>>::insert_node_at
// (unit v5944; libtpw_unit_std_basic_filebuf_basic_filebuf_3.dylib, tests/test_std_basic_filebuf_basic_filebuf_3.py), main app:
//   src/std_basic_filebuf_basic_filebuf_3.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The blocks the world
// needs (codecvt object, new node, reference count, string array) are built by the test and handed to the wrappers.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

using namespace calllog;

// ---- globals of other files, defined here
uint32_t gIfsWVtblC[4];
uint32_t gIfsWVtblD[4];
namespace IME {
char gMslLengthErrorDtorTV[8];
char gMslLengthErrorVtbl[16];
char gMslExcVtblB[16];
}  // namespace IME
using IME::gMslLengthErrorDtorTV;
using IME::gMslLengthErrorVtbl;

namespace {
void* gBlkCvt;
void* gBlkNode;
void* gBlkRef;
void* gBlkArr;

struct LenExc {
    const void* vtbl;
    char* data;
    int32_t* refs;
};
}  // namespace

// ---- the fakes
namespace filebuf_char_callee {
void LocaleCtor(void* locale) {
    ev("LocaleCtor", {lab(locale)});
    uint32_t* w = static_cast<uint32_t*>(locale);
    w[0] = static_cast<uint32_t>(ret("LocA"));
    w[1] = static_cast<uint32_t>(ret("LocB"));
}
void LocaleDtor(void* locale, int32_t flag) {
    const uint32_t* w = static_cast<const uint32_t*>(locale);
    ev("LocaleDtor", {flag});
    ev("LocaleDtorW", {w[0], w[1]});
}
int32_t CodecvtAlwaysNoconv(void* cvt) {
    ev("Noconv", {lab(cvt)});
    return static_cast<int32_t>(ret("Noconv"));
}
uint32_t Fopen(const char* name, const char* mode) {
    ev("Fopen", {}, {name, mode});
    return static_cast<uint32_t>(ret("Fopen"));
}
int32_t Fseek(uint32_t file, int32_t offset, int32_t whence) {
    ev("Fseek", {file, offset, whence});
    return static_cast<int32_t>(ret("Fseek"));
}
int32_t Fclose(uint32_t file) {
    ev("Fclose", {file});
    return static_cast<int32_t>(ret("Fclose"));
}
void DeleteArray(void* p) { ev("DeleteArray", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(p))}); }
}  // namespace filebuf_char_callee

namespace filebuf_wclose_callee {
void Getloc(void* out, void* streambuf) {
    ev("Getloc", {lab(out), lab(streambuf)});
    uint32_t* w = static_cast<uint32_t*>(out);
    w[0] = static_cast<uint32_t>(ret("GetlocA"));
    w[1] = static_cast<uint32_t>(ret("GetlocB"));
}
void* UseFacetCodecvtW(void* locale) {
    const uint32_t* w = static_cast<const uint32_t*>(locale);
    ev("UseFacet", {});
    ev("UseFacetW", {w[0], w[1]});
    return gBlkCvt;
}
}  // namespace filebuf_wclose_callee

namespace filebuf_wseek_callee {
bool Unshift(BasicFilebufWchar* fb) {
    ev("Unshift", {lab(fb)});
    return ret("Unshift") != 0;
}
}  // namespace filebuf_wseek_callee

namespace buddy_tree_callee {
uint32_t Strlen(const char* s) {
    ev("Strlen", {}, {s});
    return static_cast<uint32_t>(std::strlen(s));
}
void* NewArray(uint32_t size) {
    ev("NewArray", {size});
    return ret("ArrNull") ? nullptr : gBlkArr;
}
void* New(uint32_t size) {
    ev("New", {size});
    if (size == 0x14) return gBlkNode;
    return ret("RefNull") ? nullptr : gBlkRef;
}
char* Strcpy(char* to, const char* from) {
    ev("Strcpy", {lab(to)}, {from});
    if (to) std::strcpy(to, from);
    return to;
}
void Throw(const char* type, void* object, const void* dtor) {
    ev("Throw", {}, {type});
    const LenExc* exc = static_cast<const LenExc*>(object);
    int64_t dataOk = exc->data ? (std::strcmp(exc->data, "tree::insert length error") == 0 ? 1 : 0) : 2;
    ev("ThrowObj", {exc->vtbl == static_cast<const void*>(gMslLengthErrorVtbl), dtor == static_cast<const void*>(gMslLengthErrorDtorTV),
                    lab(exc->data), lab(exc->refs), exc->refs ? *exc->refs : -1, dataOk});
}
void* PairCtor(void* self, const void* src) {
    ev("PairCtor", {lab(self), lab(src)});
    uint32_t* w = static_cast<uint32_t*>(self);
    w[0] = static_cast<uint32_t>(ret("PairA"));
    w[1] = static_cast<uint32_t>(ret("PairB"));
    return self;
}
void BalanceInsert(void* node, void* root) { ev("Balance", {lab(node), lab(root)}); }
}  // namespace buddy_tree_callee

extern "C" {

uint64_t tpw_fbw3_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblC", gIfsWVtblC},
        {"vtblD", gIfsWVtblD},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fbw3_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fbw3_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mCodecvt));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mBuf));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mFlag4c));
    out[n++] = static_cast<int64_t>(offsetof(BasicFilebufWchar, mUnbuffered));
    out[n++] = static_cast<int64_t>(sizeof(BuddyTreeNode));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTreeNode, mPair));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mField8));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mLeftmost));
    out[n++] = static_cast<int64_t>(sizeof(BuddyTree));
    return n;
}

uint32_t tpw_fbw3_ctor(uint64_t obj, uint32_t file, uint64_t cvt) {
    gBlkCvt = reinterpret_cast<void*>(static_cast<uintptr_t>(cvt));
    BasicFilebufWchar* r = FilebufWcharConstruct(reinterpret_cast<BasicFilebufWchar*>(static_cast<uintptr_t>(obj)), file);
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(r));
}

uint32_t tpw_fbw3_open(uint64_t obj, uint64_t name, uint32_t mode) {
    BasicFilebufWchar* r = FilebufWcharOpen(reinterpret_cast<BasicFilebufWchar*>(static_cast<uintptr_t>(obj)),
                                            reinterpret_cast<const char*>(static_cast<uintptr_t>(name)), mode);
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(r));
}

uint32_t tpw_fbw3_insert(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint64_t pair, uint64_t node, uint64_t ref,
                         uint64_t arr) {
    gBlkNode = reinterpret_cast<void*>(static_cast<uintptr_t>(node));
    gBlkRef = reinterpret_cast<void*>(static_cast<uintptr_t>(ref));
    gBlkArr = reinterpret_cast<void*>(static_cast<uintptr_t>(arr));
    BuddyTreeNode* r = BuddyTreeInsertNodeAt(reinterpret_cast<BuddyTree*>(static_cast<uintptr_t>(tree)),
                                             reinterpret_cast<BuddyTreeNode*>(static_cast<uintptr_t>(parent)), (left & 0xff) != 0,
                                             (leftmost & 0xff) != 0, reinterpret_cast<const void*>(static_cast<uintptr_t>(pair)));
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(r));
}

}  // extern "C"
