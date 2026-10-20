// Unit v5942: the red-black tree's balance_insert (0x10153b24), the subscription tree's insert_node_at (0x101538d8) and
// CSubscription::Load (0x10152bb4).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LbFile.h"
#include "MslSubscriptionTree.h"
#include "ui_ime.h"

namespace {

using node_base = MslRedBlackTree::node_base;

inline node_base* ParentOf(node_base* node) { return reinterpret_cast<node_base*>(node->mParent & ~uintptr_t(1)); }
inline bool IsRed(node_base* node) { return (node->mParent & 1) != 0; }
inline void SetRed(node_base* node) { node->mParent |= 1; }
inline void SetBlack(node_base* node) { node->mParent &= ~uintptr_t(1); }

}  // namespace

// 0x10153b24. `root` lives in a local the rotations update, and the loop and the last line read that local.
void MslRedBlackTree::balance_insert(node_base* node, node_base* root) {
    SetRed(node);
    while (node != root && IsRed(ParentOf(node))) {
        node_base* parent = ParentOf(node);
        node_base* grand = ParentOf(parent);
        if (parent == grand->mLeft) {
            node_base* uncle = grand->mRight;
            if (uncle != nullptr && IsRed(uncle)) {
                SetBlack(parent);
                SetBlack(uncle);
                node = ParentOf(ParentOf(node));
                SetRed(node);
            } else {
                if (node == parent->mRight) {
                    node = parent;
                    rotate_left(parent, root);
                }
                SetBlack(ParentOf(node));
                SetRed(ParentOf(ParentOf(node)));
                rotate_right(ParentOf(ParentOf(node)), root);
            }
        } else {
            node_base* uncle = grand->mLeft;
            if (uncle != nullptr && IsRed(uncle)) {
                SetBlack(parent);
                SetBlack(uncle);
                node = ParentOf(ParentOf(node));
                SetRed(node);
            } else {
                if (node == parent->mLeft) {
                    node = parent;
                    rotate_right(parent, root);
                }
                SetBlack(ParentOf(node));
                SetRed(ParentOf(ParentOf(node)));
                rotate_left(ParentOf(ParentOf(node)), root);
            }
        }
    }
    SetBlack(root);
}

// 0x101538d8. A full tree (count 0xffffffff) throws a std::length_error("tree::insert length error"); the fakes of __throw return,
// and the insert goes on. The Mac's first-node word (+8) is not kept on the host (begin() is derived), so `leftmost` has no store.
SubscriptionTree::node_base* SubscriptionTree::insert_node_at(node_base* parent, bool left, bool leftmost, const CSubscription& key) {
    (void)leftmost;
    if (mCount > 0xfffffffeu) {
        static const char kMessage[] = "tree::insert length error";   // 0x10248db0
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    node* created = static_cast<node*>(IME::MslNew(sizeof(node)));   // the Mac's node is 0xe bytes
    created->mValue = key;
    created->mRight = nullptr;
    created->mLeft = nullptr;
    created->mParent = reinterpret_cast<uintptr_t>(parent) | (created->mParent & 1);
    if (left)
        parent->mLeft = created;
    else
        parent->mRight = created;
    mCount = mCount + 1;
    MslRedBlackTree::balance_insert(created, mRoot);
    return created;
}

// 0x10152bb4. The file holds the id little-endian: the Mac reads it as a big-endian halfword and swaps it in place; on the host the
// two bytes are the id as they are (the swap here made a park's id 7 into 0x700; v5L05, the park's load).
void CSubscription::Load(void*& file) { LoadResult(file); }

int32_t CSubscription::LoadResult(void*& file) {
    uint32_t transferred = 0;
    LbFile_Read(file, this, 2, &transferred);
    if (transferred == 2) return 0;
    DEBUG_MESSAGE("Warning! Something went hideously wrong while loading the message subscriptions!", 0);
    return -1;
}
