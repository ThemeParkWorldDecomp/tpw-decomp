// std::__tree<CSubscription, std::less<CSubscription>, std::allocator<CSubscription>> (a std::set of thing ids) and its allocators,
// main app 0x1000fae0 .. 0x1000ff04 and 0x10152c58 .. 0x10152f3c (the inventory files them under Advisor.cpp and RideDB.cpp). Unit
// w107. insert_one (0x101530e4), insert_node_at (0x101538d8) and balance_insert are the subscribe path's and not part of this unit.
// The node: left +0, right +4, parent +8 (colour in bit 0), the id at +0xc; the tree keeps its count and root (the begin word is
// derived on the host, see include/MslSubscriptionTree.h).
#include <new>

#include "MslSubscriptionTree.h"

// 0x1000ff04: zero count (the Mac copies a static word that holds 0), no root; begin = end follows from the empty root.
void SubscriptionTree::Construct() {
    mCount = 0;
    mRoot = nullptr;
}

SubscriptionTree::node_base* SubscriptionTree::begin() {
    node_base* node = mRoot;
    if (node == nullptr) return end();
    while (node->mLeft != nullptr) node = node->mLeft;
    return node;
}

// 0x1000fae0: the Mac unrolls two levels of this recursion; the frees come in the same order, children left to right and then the
// node itself. (std::allocator<CSubscription>::destroy between them does nothing.)
void SubscriptionTree::destroy(node* subtree) {
    if (subtree->mLeft != nullptr) destroy(static_cast<node*>(subtree->mLeft));
    if (subtree->mRight != nullptr) destroy(static_cast<node*>(subtree->mRight));
    ::operator delete(subtree);
}

// 0x10152c58: the lower bound (the first node whose id is not below the key), accepted when it is not above the key either, else end().
// The ids compare as unsigned 16-bit numbers.
SubscriptionTree::node_base* SubscriptionTree::find(const CSubscription& key) {
    node_base* candidate = end();
    node_base* current = mRoot;
    while (current != nullptr) {
        if (static_cast<node*>(current)->mValue.mThingId < key.mThingId) {
            current = current->mRight;
        } else {
            candidate = current;
            current = current->mLeft;
        }
    }
    if (candidate != end() && static_cast<node*>(candidate)->mValue.mThingId <= key.mThingId) return candidate;
    return end();
}

// 0x10152e64: nothing happens to a tree without a root (its count is left as it is).
void SubscriptionTree::clear() {
    if (mRoot != nullptr) {
        destroy(static_cast<node*>(mRoot));
        mCount = 0;
        mRoot = nullptr;
    }
}

// 0x10152f3c: unlinks and frees the node and counts it off. The Mac first moves its begin word to the successor when the first node
// goes (loads only); the host derives begin, so there is nothing to do for it.
void SubscriptionTree::erase(node_base* position) {
    MslRedBlackTree::balance_erase(position, mRoot);
    ::operator delete(position);
    mCount = mCount - 1;
}

void SubscriptionAllocator::destroy(CSubscription*) {}

void SubscriptionAllocator::deallocate(SubscriptionTree::node* node, unsigned long) { ::operator delete(node); }
