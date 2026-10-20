// Test library for the red-black tree's balance_insert, the subscription tree's insert_node_at and CSubscription::Load (unit v5942;
// libtpw_unit_std___red_black_tree_balance_insert.dylib, tests/test_std___red_black_tree_balance_insert.py), main app:
//   src/GameClock_std___red_black_tree_balance_insert.cpp   the three functions
//
// The callees outside the unit (operator new, new[], strlen, strcpy, _RefCountedPtr ctor, __throw, LbFile_Read, DEBUG_MESSAGE) are fakes
// that record what the PowerPC recorders record. The rotations are the real ones (src/msl_red_black_tree.cpp).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "LbFile.h"
#include "MslSubscriptionTree.h"
#include "TbError.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s != 0; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return h;
}

}  // namespace

// ---- the fakes
namespace IME {

size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {static_cast<int64_t>(n), Hash(s)});
    return n;
}

char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {static_cast<int64_t>(std::strlen(s)), Hash(s)});
    return std::strcpy(d, s);
}

// The Mac's node is 0xe bytes and the host's is larger: the event shows the Mac's size.
void* MslNew(uint32_t size) {
    ev("new", {size == sizeof(SubscriptionTree::node) ? 0xe : static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(64);
}

void* MslNewArray(uint32_t size) {
    ev("newarr", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size + 16);
}

void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    ev("throw", {Hash(typeName), lab(const_cast<void*>(dtorTV)), lab(const_cast<void*>(o->mVtbl)), Hash(o->mRc.mPtr)});
}

void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("RcCtor", {p != nullptr ? 1 : 0});
    self->mPtr = p;
    self->mCount = nullptr;
}

}  // namespace IME

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), static_cast<int64_t>(length)});
    uint32_t data = static_cast<uint32_t>(ret("data"));
    uint8_t bytes[2] = {static_cast<uint8_t>(data & 0xff), static_cast<uint8_t>((data >> 8) & 0xff)};
    std::memcpy(buffer, bytes, 2);
    *transferred = static_cast<uint32_t>(ret("xfer"));
    return TbError(0);
}

void DEBUG_MESSAGE(const char* message, uint32_t flags) {
    ev("DEBUG_MESSAGE", {static_cast<int64_t>(std::strlen(message)), Hash(message), static_cast<int64_t>(flags)});
}

extern "C" {

uint64_t tpw_bi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"dtortv", IME::gMslLengthErrorDtorTV},
        {"vtbl", IME::gMslLengthErrorVtbl},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(MslRedBlackTree::node_base));
    out[n++] = static_cast<int64_t>(offsetof(SubscriptionTree::node, mValue));
    out[n++] = static_cast<int64_t>(sizeof(SubscriptionTree::node));
    out[n++] = static_cast<int64_t>(offsetof(SubscriptionTree, mRoot));
    out[n++] = static_cast<int64_t>(sizeof(SubscriptionTree));
    return n;
}

void tpw_bi_balance(uint64_t node, uint64_t root) {
    MslRedBlackTree::balance_insert(reinterpret_cast<MslRedBlackTree::node_base*>(node), reinterpret_cast<MslRedBlackTree::node_base*>(root));
}

uint64_t tpw_bi_insert(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint64_t key) {
    SubscriptionTree* t = reinterpret_cast<SubscriptionTree*>(tree);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(t->insert_node_at(
        reinterpret_cast<MslRedBlackTree::node_base*>(parent), (left & 0xff) != 0, (leftmost & 0xff) != 0,
        *reinterpret_cast<const CSubscription*>(key))));
}

int32_t tpw_bi_load(uint64_t self, uint64_t filep) {
    return reinterpret_cast<CSubscription*>(self)->LoadResult(*reinterpret_cast<void**>(filep));
}

}  // extern "C"
