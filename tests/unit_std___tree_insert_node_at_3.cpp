// Test library for std::__tree<unsigned short> find / insert_one / insert_node_at, IPSystem::Startup / Shutdown and ccParkInfo::~ccParkInfo
// (unit v5917; libtpw_unit_std___tree_insert_node_at_3.dylib, tests/test_std___tree_insert_node_at_3.py), main app:
//   src/FS_Online_gametype_std___tree_insert_node_at_3.cpp
//
// The callees outside the unit are fakes that record what the real ones read: the runtime (strlen, new, new[], strcpy, __throw, free,
// delete), the _RefCountedPtr constructor, balance_insert, WSAStartup / WSACleanup and IPBase::ShowError. The new node and the new[]
// block are blocks the test prepared (gNewNode, gNewArr). The globals of the exception code are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {
uint64_t gNewNode = 0;
uint64_t gNewArr = 0;
}  // namespace

namespace IME {
size_t MslStrlen(const char* s) {
    ev("strlen", {}, {s});
    return std::strlen(s);
}
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(gNewNode));
}
void* MslNewArray(uint32_t size) {
    ev("newarr", {static_cast<int64_t>(size)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(gNewArr));
}
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("rcctor", {lab(p)});
    self->mPtr = p;
    self->mCount = nullptr;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {lab(d)}, {s});
    std::strcpy(d, s);
    return d;
}
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {static_cast<int64_t>(o->mVtbl == gMslLengthErrorVtbl), static_cast<int64_t>(dtorTV == gMslLengthErrorDtorTV), lab(o->mRc.mPtr)},
       {typeName});
}
}  // namespace IME

namespace v5917_callee {
void BalanceInsert(UshortSetTree::node* node, UshortSetTree::node* root) { ev("balance", {lab(node), lab(root)}); }
int32_t WSAStartup(int32_t version, void* data) {
    ev("WSAStartup", {version, static_cast<int64_t>(data != nullptr)});
    return static_cast<int32_t>(ret("WSAStartup"));
}
int32_t WSACleanup() {
    ev("WSACleanup", {});
    return static_cast<int32_t>(ret("WSACleanup"));
}
void Free(void* p) { ev("free", {lab(p)}); }
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
}  // namespace v5917_callee

void IPBase::ShowError() { ev("ShowError", {lab(this)}); }

extern "C" {

uint64_t tpw_v5917_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The 32-bit word the host's Ptr32 keeps for an address.
uint32_t tpw_v5917_off(uint64_t p) { return p == 0 ? 0 : static_cast<uint32_t>(tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p)))); }

void tpw_v5917_set_new(uint64_t node, uint64_t array) {
    gNewNode = node;
    gNewArr = array;
}

int32_t tpw_v5917_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::node, mRight));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::node, mParent));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::node, mValue));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree, mBegin));
    out[n++] = static_cast<int64_t>(offsetof(UshortSetTree::InsertResult, mInserted));
    out[n++] = static_cast<int64_t>(sizeof(ccParkInfo));
    return n;
}

void tpw_v5917_find(uint64_t tree, uint64_t key, uint64_t result) {
    UshortSetTree* t = reinterpret_cast<UshortSetTree*>(static_cast<uintptr_t>(tree));
    *reinterpret_cast<UshortSetTree::Iterator*>(static_cast<uintptr_t>(result)) = t->find(*reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(key)));
}

void tpw_v5917_insert_one(uint64_t tree, uint64_t key, uint64_t result) {
    UshortSetTree* t = reinterpret_cast<UshortSetTree*>(static_cast<uintptr_t>(tree));
    *reinterpret_cast<UshortSetTree::InsertResult*>(static_cast<uintptr_t>(result)) =
        t->insert_one(*reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(key)));
}

uint64_t tpw_v5917_insert_node_at(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint64_t key) {
    UshortSetTree* t = reinterpret_cast<UshortSetTree*>(static_cast<uintptr_t>(tree));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        t->insert_node_at(reinterpret_cast<UshortSetTree::node*>(static_cast<uintptr_t>(parent)), left != 0, leftmost != 0,
                          *reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(key)))));
}

uint32_t tpw_v5917_startup(uint64_t self) { return reinterpret_cast<IPSystem*>(static_cast<uintptr_t>(self))->Startup() ? 1 : 0; }
uint32_t tpw_v5917_shutdown(uint64_t self) { return reinterpret_cast<IPSystem*>(static_cast<uintptr_t>(self))->Shutdown() ? 1 : 0; }

uint64_t tpw_v5917_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        ccParkInfo::Destruct(reinterpret_cast<ccParkInfo*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag))));
}

}  // extern "C"
