// Test library for the buddy map's tree (unit v51025; libtpw_unit_std___tree_std_pair_std_map_nocase_string_BuddyI_2.dylib), main app:
//   src/game/online/online.cpp   find, insert_one, destroy, erase
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "MslSubscriptionTree.h"
#include "online.h"
#include "std_basic_filebuf.h"

using namespace calllog;

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
bool NocaseWStringLess(const void* a, const void* b) {
    ev("lt", {lab(a), lab(b)});
    return ret("lt") != 0;
}

void MslRedBlackTree::balance_erase(node_base* node, node_base* root) { ev("balance_erase", {lab(node), lab(root)}); }

BuddyEntry* BuddyEntryDestruct(BuddyEntry* self, int16_t flag) {
    ev("pair_dtor", {lab(self), flag});
    return self;
}

BuddyTreeNode* BuddyTreeInsertNodeAt(BuddyTree* self, BuddyTreeNode* parent, bool left, bool setLeftmost, const void* pair) {
    ev("insert_node_at", {lab(self), lab(parent), left ? 1 : 0, setLeftmost ? 1 : 0, lab(pair)});
    return reinterpret_cast<BuddyTreeNode*>(static_cast<uintptr_t>(ret("node")));
}

namespace buddy_tree_callee {
void AllocDestroy(void* allocator, void* pair) { ev("alloc_destroy", {lab(allocator), lab(pair)}); }
void Deallocate(void* allocator, void* node, uint32_t count) { ev("dealloc", {lab(allocator), lab(node), count}); }
void Delete(void* p) { ev("delete", {lab(p)}); }
}  // namespace buddy_tree_callee

extern "C" {

uint64_t tpw_buddy_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_buddy_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTree, mLeftmost));
    out[n++] = static_cast<int64_t>(sizeof(BuddyTree));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTreeNode, mParentColor));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTreeNode, mPair));
    out[n++] = static_cast<int64_t>(offsetof(NocaseRep, mRefs));
    out[n++] = static_cast<int64_t>(offsetof(NocaseRep, mData));
    out[n++] = static_cast<int64_t>(offsetof(BuddyEntry, mDirect));
    out[n++] = static_cast<int64_t>(offsetof(BuddyTreeInsertResult, mInserted));
    return n;
}

void tpw_buddy_find(uint64_t out, uint64_t tree, uint64_t key) {
    *reinterpret_cast<BuddyTreeNode**>(out) =
        BuddyTreeFind(reinterpret_cast<BuddyTree*>(tree), reinterpret_cast<const void*>(key));
}

void tpw_buddy_insert_one(uint64_t out, uint64_t tree, uint64_t pair) {
    BuddyTreeInsertOne(reinterpret_cast<BuddyTreeInsertResult*>(out), reinterpret_cast<BuddyTree*>(tree),
                       reinterpret_cast<const void*>(pair));
}

void tpw_buddy_destroy(uint64_t tree, uint64_t node) {
    BuddyTreeDestroy(reinterpret_cast<BuddyTree*>(tree), reinterpret_cast<BuddyTreeNode*>(node));
}

void tpw_buddy_erase(uint64_t tree, uint64_t node) {
    BuddyTreeErase(reinterpret_cast<BuddyTree*>(tree), reinterpret_cast<BuddyTreeNode*>(node));
}

}  // extern "C"
