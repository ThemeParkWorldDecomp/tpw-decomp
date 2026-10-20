// std::__red_black_tree<__int128>: the Metrowerks standard library's red-black tree code that every set and map shares (main app
// 0x10152db0 increment, 0x10153290 balance_erase, 0x10153dec rotate_left, 0x10153efc rotate_right; the inventory files them under
// RideDB.cpp). Unit w107. Colour: bit 0 of a node's parent word, 1 = red. A null child counts as black. balance_insert (0x10153b24) is
// not part of this unit.
#include "MslSubscriptionTree.h"

namespace {

using node_base = MslRedBlackTree::node_base;

inline node_base* ParentOf(const node_base* node) { return reinterpret_cast<node_base*>(node->mParent & ~uintptr_t(1)); }
inline bool IsRed(const node_base* node) { return (node->mParent & 1) != 0; }
inline void MakeRed(node_base* node) { node->mParent |= 1; }
inline void MakeBlack(node_base* node) { node->mParent &= ~uintptr_t(1); }
// A parent pointer with the child's own colour kept.
inline void SetParent(node_base* node, node_base* parent) { node->mParent = reinterpret_cast<uintptr_t>(parent) | (node->mParent & 1); }
// null or black
inline bool IsBlack(const node_base* node) { return node == nullptr || !IsRed(node); }

}  // namespace

// The successor: with a right child, its leftmost descendant; otherwise climb until we come up out of a left child (the root's parent is
// the end node, whose left word is the root, so the walk stops there after the last node).
void MslRedBlackTree::increment(node_base*& iterator) {
    node_base* right = iterator->mRight;
    if (right == nullptr) {
        node_base* parent;
        do {
            const node_base* current = iterator;
            parent = ParentOf(current);
            iterator = parent;
            if (current == parent->mLeft) break;
        } while (true);
        return;
    }
    node_base* node;
    do {
        node = right;
        right = node->mLeft;
    } while (right != nullptr);
    iterator = node;
}

void MslRedBlackTree::rotate_left(node_base* node, node_base*& root) {
    node_base* pivot = node->mRight;
    if (root == node) root = pivot;
    node->mRight = pivot->mLeft;
    if (pivot->mLeft != nullptr) SetParent(pivot->mLeft, node);
    pivot->mParent = (pivot->mParent & 1) | (node->mParent & ~uintptr_t(1));
    node_base* parent = ParentOf(node);
    if (node == parent->mLeft)
        parent->mLeft = pivot;
    else
        parent->mRight = pivot;
    pivot->mLeft = node;
    SetParent(node, pivot);
}

void MslRedBlackTree::rotate_right(node_base* node, node_base*& root) {
    node_base* pivot = node->mLeft;
    if (root == node) root = pivot;
    node->mLeft = pivot->mRight;
    if (pivot->mRight != nullptr) SetParent(pivot->mRight, node);
    pivot->mParent = (pivot->mParent & 1) | (node->mParent & ~uintptr_t(1));
    node_base* parent = ParentOf(node);
    if (node == parent->mLeft)
        parent->mLeft = pivot;
    else
        parent->mRight = pivot;
    pivot->mRight = node;
    SetParent(node, pivot);
}

// The textbook delete: y (z, or z's in-order successor when z has two children) is spliced out and, when it takes z's place, adopts
// z's links and colour; if a black node left, the colours are repaired from x (y's only child, possibly null) upwards. `root` is a
// local copy of the root: the rotations update it, and the loop compares with it. The Mac's loop keeps x's parent and which side x is
// on in two variables because x can be null.
void MslRedBlackTree::balance_erase(node_base* z, node_base* root) {
    node_base* y = z;
    if (z->mLeft != nullptr) {
        node_base* right = z->mRight;
        if (right != nullptr) {
            while (right->mLeft != nullptr) right = right->mLeft;
            y = right;
        }
    }
    node_base* x = y->mLeft;
    if (x == nullptr) x = y->mRight;
    node_base* xParent = ParentOf(y);
    if (x != nullptr) SetParent(x, xParent);
    bool xIsLeft;
    node_base* yParent = ParentOf(y);
    if (y == yParent->mLeft) {
        yParent->mLeft = x;
        xIsLeft = true;
    } else {
        xIsLeft = false;
        yParent->mRight = x;
    }
    bool rebalance = !IsRed(y);
    if (y == z) {
        // the tree's last node leaves nothing to repair
        if (x == nullptr && root == z) rebalance = false;
    } else {
        // y takes z's place
        y->mParent = (y->mParent & 1) | (z->mParent & ~uintptr_t(1));
        node_base* parent = ParentOf(y);
        if (z == parent->mLeft)
            parent->mLeft = y;
        else
            parent->mRight = y;
        y->mLeft = z->mLeft;
        if (y->mLeft != nullptr)
            SetParent(y->mLeft, y);
        else
            xParent = y;
        y->mRight = z->mRight;
        if (y->mRight != nullptr)
            SetParent(y->mRight, y);
        else
            xParent = y;
        if (IsRed(z))
            MakeRed(y);
        else
            MakeBlack(y);
        if (root == z) root = y;
    }
    if (!rebalance) return;
    while (x != root && IsBlack(x)) {
        if (xIsLeft) {
            node_base* sibling = xParent->mRight;
            if (IsRed(sibling)) {
                MakeBlack(sibling);
                MakeRed(xParent);
                rotate_left(xParent, root);
                sibling = xParent->mRight;
            }
            if (IsBlack(sibling->mLeft) && IsBlack(sibling->mRight)) {
                MakeRed(sibling);
                x = xParent;
                node_base* parent = ParentOf(xParent);
                xIsLeft = parent->mLeft == xParent;
                xParent = parent;
            } else {
                if (IsBlack(sibling->mRight)) {
                    MakeBlack(sibling->mLeft);
                    MakeRed(sibling);
                    rotate_right(sibling, root);
                    sibling = xParent->mRight;
                }
                if (IsRed(xParent))
                    MakeRed(sibling);
                else
                    MakeBlack(sibling);
                MakeBlack(xParent);
                MakeBlack(sibling->mRight);
                rotate_left(xParent, root);
                x = root;
            }
        } else {
            node_base* sibling = xParent->mLeft;
            if (IsRed(sibling)) {
                MakeBlack(sibling);
                MakeRed(xParent);
                rotate_right(xParent, root);
                sibling = xParent->mLeft;
            }
            if (IsBlack(sibling->mLeft) && IsBlack(sibling->mRight)) {
                MakeRed(sibling);
                x = xParent;
                node_base* parent = ParentOf(xParent);
                xIsLeft = parent->mLeft == xParent;
                xParent = parent;
            } else {
                if (IsBlack(sibling->mLeft)) {
                    MakeBlack(sibling->mRight);
                    MakeRed(sibling);
                    rotate_left(sibling, root);
                    sibling = xParent->mLeft;
                }
                if (IsRed(xParent))
                    MakeRed(sibling);
                else
                    MakeBlack(sibling);
                MakeBlack(xParent);
                MakeBlack(sibling->mLeft);
                rotate_right(xParent, root);
                x = root;
            }
        }
    }
    if (x != nullptr) MakeBlack(x);
}
