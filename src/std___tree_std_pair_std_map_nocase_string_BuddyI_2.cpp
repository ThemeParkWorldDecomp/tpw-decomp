// std::__tree<std::pair<const nocase_string, BuddyInfo>, ...> (the buddy map's tree): find 0x1021f8d8, insert_one 0x10222d9c,
// destroy 0x10222564, erase 0x102278fc. Node: left +0, right +4, parent|colour +8, pair +0xc (Mac); the tree: count +0, root +4 (the end
// node's left word), begin +0xc.
#include <cstdint>

#include "MslSubscriptionTree.h"
#include "online.h"
#include "std_basic_filebuf.h"

namespace {

inline BuddyEntry* PairOf(BuddyTreeNode* node) { return reinterpret_cast<BuddyEntry*>(node->mPair); }

inline BuddyTreeNode* EndOf(BuddyTree* tree) { return reinterpret_cast<BuddyTreeNode*>(&tree->mRoot); }

// The key's string destructor, inlined by the compiler: drops one reference; the last one frees the data and the rep.
void ReleaseKey(BuddyTreeNode* node) {
    BuddyEntry* pair = PairOf(node);
    NocaseRep* rep = reinterpret_cast<NocaseRep*>(pair->mKey);
    if (rep->mRefs == 0) rep->mRefs = 1;
    uint32_t before = rep->mRefs;
    rep->mRefs = before - 1;
    if (before - 1 != 0) rep = nullptr;
    pair->mKey = reinterpret_cast<uintptr_t>(rep);
    if (rep) {
        buddy_tree_callee::Delete(rep->mData);
        buddy_tree_callee::Delete(rep);
    }
}

// A grandchild the compiler unrolled: its subtrees, then the allocator's destroy and deallocate (the node is not freed here).
void DestroyLeaf(BuddyTree* self, BuddyTreeNode* node) {
    if (node->mLeft) BuddyTreeDestroy(self, node->mLeft);
    if (node->mRight) BuddyTreeDestroy(self, node->mRight);
    buddy_tree_callee::AllocDestroy(self, node->mPair);
    buddy_tree_callee::Deallocate(&self->mRoot, node, 1);
}

// A child the compiler unrolled: its two children as leaves, then the pair's destructor and operator delete.
void DestroyChild(BuddyTree* self, BuddyTreeNode* child) {
    BuddyTreeNode* first = child->mLeft;
    if (first) DestroyLeaf(self, first);
    BuddyTreeNode* second = child->mRight;
    if (second) DestroyLeaf(self, second);
    BuddyEntryDestruct(PairOf(child), -1);
    buddy_tree_callee::Delete(child);
}

}  // namespace

// 0x1021f8d8
BuddyTreeNode* BuddyTreeFind(BuddyTree* self, const void* key) {
    BuddyTreeNode* best = EndOf(self);
    BuddyTreeNode* node = self->mRoot;
    while (node) {
        if (!NocaseWStringLess(node->mPair, key)) {
            best = node;
            node = node->mLeft;
        } else {
            node = node->mRight;
        }
    }
    if (best == EndOf(self) || NocaseWStringLess(key, best->mPair)) return EndOf(self);
    return best;
}

// 0x10222d9c. The two constants are the bytes at 0x102b1fb0 (1) and 0x102b1fb1 (0).
void BuddyTreeInsertOne(BuddyTreeInsertResult* result, BuddyTree* self, const void* pair) {
    BuddyTreeNode* parent = EndOf(self);
    BuddyTreeNode* notLess = nullptr;
    bool left = true;
    bool leftmost = true;
    BuddyTreeNode* node = self->mRoot;
    while (node) {
        parent = node;
        if (NocaseWStringLess(pair, node->mPair)) {
            node = node->mLeft;
            left = true;
        } else {
            notLess = node;
            node = node->mRight;
            left = false;
            leftmost = false;
        }
    }
    if (notLess == nullptr || NocaseWStringLess(notLess->mPair, pair)) {
        BuddyTreeNode* made = BuddyTreeInsertNodeAt(self, parent, left, leftmost, pair);
        result->mNode = made;
        result->mInserted = true;
    } else {
        result->mNode = notLess;
        result->mInserted = false;
    }
}

// 0x10222564
void BuddyTreeDestroy(BuddyTree* self, BuddyTreeNode* node) {
    BuddyTreeNode* first = node->mLeft;
    if (first) DestroyChild(self, first);
    BuddyTreeNode* second = node->mRight;
    if (second) DestroyChild(self, second);
    ReleaseKey(node);
    buddy_tree_callee::Delete(node);
}

// 0x102278fc
void BuddyTreeErase(BuddyTree* self, BuddyTreeNode* position) {
    BuddyTreeNode* node = position;
    if (node == self->mLeftmost) {
        BuddyTreeNode* it = node->mRight;
        if (it) {
            while (it->mLeft) it = it->mLeft;
        } else {
            it = node;
            for (;;) {
                BuddyTreeNode* current = it;
                BuddyTreeNode* parent = reinterpret_cast<BuddyTreeNode*>(current->mParentColor & ~static_cast<uintptr_t>(1));
                it = parent;
                if (current == parent->mLeft) break;
            }
        }
        self->mLeftmost = it;
    }
    MslRedBlackTree::balance_erase(reinterpret_cast<MslRedBlackTree::node_base*>(node),
                                   reinterpret_cast<MslRedBlackTree::node_base*>(self->mRoot));
    ReleaseKey(node);
    buddy_tree_callee::Delete(node);
    self->mCount = self->mCount - 1;
}
