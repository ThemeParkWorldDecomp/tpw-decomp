// Test library for the set<wstring> tree (insert_node_at 0x10227c78, erase 0x10228104, destroy 0x10223204; libtpw_unit_std___tree_std_basic_string_std_less_std_basic_s_3.dylib,
// tests/test_std___tree_std_basic_string_std_less_std_basic_s_3.py), main app:
//   src/std___tree_std_basic_string_std_less_std_basic_s_3.cpp
//
// Every callee outside the unit is a recording fake. All objects of a case live in one region of 64 byte slots; an address in the
// region is logged as a tag (slot * 16 + offset / 8), so the Mac (4 byte words) and the host (8 byte words) log the same numbers.
// operator new hands out the region's pool slots filled with a byte that depends on the slot.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"
#include "MslWStringTree.h"
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

const uintptr_t kSlot = 64;
const uintptr_t kWord = 8;
uintptr_t gRegion = 0;
uint32_t gSlots = 0;
uint32_t gPool0 = 0;
uint32_t gPoolNext = 0;
uint8_t gFill = 0;

int64_t Tag(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (v < gRegion || v >= gRegion + kSlot * gSlots) return -1;
    uintptr_t o = v - gRegion;
    return static_cast<int64_t>((o / kSlot) * 16 + (o % kSlot) / kWord);
}

int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - gRegion); }

uint32_t gSmallNext = 0;

// Nodes come from the slots from gPool0; the small blocks (the text, the count) from 4 slots later, filled with another byte.
void* PoolSlot(bool small) {
    uint32_t& next = small ? gSmallNext : gPoolNext;
    void* a = reinterpret_cast<void*>(gRegion + kSlot * ((small ? gPool0 + 4 : gPool0) + next));
    std::memset(a, static_cast<uint8_t>(gFill + next + (small ? 4 : 0)), kSlot);
    next++;
    return a;
}

}  // namespace

// ---- the fakes (event names and arguments as the module's PowerPC stubs)
namespace IME {
void* MslNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size == sizeof(WStringTreeNode) ? 0x10u : size)});
    return PoolSlot(size != sizeof(WStringTreeNode));
}
void* MslNewArray(uint32_t size) {
    ev("new[]", {static_cast<int64_t>(size)});
    return PoolSlot(true);
}
void MslDelete(void* p) { ev("delete", {Tag(p)}); }
size_t MslStrlen(const char* s) {
    size_t n = std::strlen(s);
    ev("strlen", {static_cast<int64_t>(n)});
    return n;
}
char* MslStrcpy(char* d, const char* s) {
    ev("strcpy", {Tag(d), static_cast<int64_t>(std::strlen(s))});
    std::strcpy(d, s);
    return d;
}
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    MslExceptionObject* o = static_cast<MslExceptionObject*>(object);
    int64_t vid = 0;
    if (o->mVtbl == gMslLengthErrorVtbl) vid = 1;
    else if (o->mVtbl == gMslExcVtblB) vid = 2;
    else if (o->mVtbl == gMslExcVtblC) vid = 3;
    int64_t sum = 0;
    for (const char* c = o->mRc.mPtr; c != nullptr && *c != 0; ++c) sum = static_cast<uint32_t>(sum * 31 + static_cast<uint8_t>(*c));
    int64_t cnt = Tag(o->mRc.mCount);
    ev("throw", {vid, Tag(o->mRc.mPtr), cnt, cnt >= 0 ? static_cast<int64_t>(*o->mRc.mCount) : -1, sum,
                 static_cast<int64_t>(std::strcmp(typeName, "!std::exception!!std::logic_error!!std::length_error!!") == 0),
                 static_cast<int64_t>(dtorTV == gMslLengthErrorDtorTV)});
}
MslWideString* MslWideString_construct(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    ev("string", {Tag(self), Tag(str), static_cast<int64_t>(pos), static_cast<int64_t>(n)});
    std::memcpy(static_cast<void*>(self), static_cast<const void*>(str), sizeof(void*));
    return self;
}
}  // namespace IME

namespace masterlist_callee {
void WStringDtor(void* string, int16_t flag) { ev("sdtor", {Tag(string), static_cast<int64_t>(flag)}); }
void OperatorDelete(void* block) { ev("delete", {Tag(block)}); }
}  // namespace masterlist_callee

namespace wstring_alloc {
void Destroy(void* allocator, void* string) { ev("adestroy", {Tag(string), Off(allocator)}); }
}  // namespace wstring_alloc

void wstring_tree_callee::NodeDeallocate(void* allocator, WStringTreeNode* node, unsigned long count) {
    ev("dealloc", {Off(allocator), Tag(node), static_cast<int64_t>(count)});
}

void MslRedBlackTree::balance_insert(node_base* node, node_base* root) { ev("binsert", {Tag(node), Tag(root)}); }
void MslRedBlackTree::balance_erase(node_base* node, node_base* root) { ev("berase", {Tag(node), Tag(root)}); }

extern "C" {

uint64_t tpw_wst_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Starts a case: the region, its slot count, the first pool slot and the fill byte.
void tpw_wst_begin(uint64_t region, uint32_t slots, uint32_t pool0, uint32_t fill) {
    gRegion = static_cast<uintptr_t>(region);
    gSlots = slots;
    gPool0 = pool0;
    gPoolNext = 0;
    gSmallNext = 0;
    gFill = static_cast<uint8_t>(fill);
}

int32_t tpw_wst_layout(int64_t* out) {
    int32_t n = 0;
    WStringTreeNode node;
    IME::MslWideStringRep rep;
    WStringTree tree;
    out[n++] = static_cast<int64_t>(sizeof(WStringTreeNode));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&node.mString) - reinterpret_cast<char*>(&node));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideStringRep));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&rep.mData) - reinterpret_cast<char*>(&rep));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&tree.mRoot) - reinterpret_cast<char*>(&tree));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&tree.mLess) - reinterpret_cast<char*>(&tree));
    return n;
}

uint64_t tpw_wst_insert(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint64_t key) {
    WStringTree* t = reinterpret_cast<WStringTree*>(static_cast<uintptr_t>(tree));
    WStringTreeNode* r = t->insert_node_at(reinterpret_cast<WStringTree::node_base*>(static_cast<uintptr_t>(parent)), (left & 0xff) != 0,
                                           (leftmost & 0xff) != 0, reinterpret_cast<const IME::MslWideString*>(static_cast<uintptr_t>(key)));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(r));
}

void tpw_wst_erase(uint64_t tree, uint64_t position) {
    reinterpret_cast<WStringTree*>(static_cast<uintptr_t>(tree))->erase(reinterpret_cast<WStringTree::node_base*>(static_cast<uintptr_t>(position)));
}

void tpw_wst_destroy(uint64_t tree, uint64_t node) {
    reinterpret_cast<WStringTree*>(static_cast<uintptr_t>(tree))->destroy(reinterpret_cast<WStringTree::node_base*>(static_cast<uintptr_t>(node)));
}

}  // extern "C"
