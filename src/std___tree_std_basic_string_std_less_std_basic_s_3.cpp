// std::__tree<std::wstring, std::less<std::wstring>, std::allocator<std::wstring>> (a std::set<std::wstring>):
// insert_node_at 0x10227c78, erase 0x10228104, destroy 0x10223204.
// Mac words are 32 bits, the host's native. The tree is count +0, root +4 (the end node's left word), begin +8 (the end node's right
// word; the mLess bytes of WStringTree on the host). A node is left +0, right +4, parent|colour +8, the string (one pointer to its
// representation) +0x0c.
#include <cstdint>
#include <cstring>

#include "FS_MasterSave.h"
#include "MslWStringTree.h"
#include "ui_ime.h"

namespace {

const char kMessage[] = "tree::insert length error";
const char kType[] = "!std::exception!!std::logic_error!!std::length_error!!";

// The inlined ~basic_string of a node's string (0x10228194.. in erase, 0x102233b0.. in destroy): the count 0 is taken as 1, the
// representation is freed when it was the last reference, otherwise the node's word is cleared. The node is not freed here.
void ReleaseString(WStringTreeNode* node) {
    IME::MslWideStringRep* rep = node->mString;
    if (rep->mRefs == 0) rep->mRefs = 1;
    uint32_t refs = rep->mRefs - 1;
    rep->mRefs = refs;
    if (refs != 0) rep = nullptr;
    node->mString = rep;
    if (rep != nullptr) {
        IME::MslDelete(rep->mData);
        IME::MslDelete(rep);
    }
}

}  // namespace

// Stand-in for the allocator's deallocate until it is translated (the test library fakes it).
__attribute__((weak)) void wstring_tree_callee::NodeDeallocate(void* allocator, WStringTreeNode* node, unsigned long count) {
    (void)allocator;
    (void)count;
    IME::MslDelete(node);
}

WStringTreeNode* WStringTree::insert_node_at(node_base* parent, bool left, bool leftmost, const IME::MslWideString* key) {
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;   // dead store in the original too
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length) + 1));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            object.mRc.mCount = count;
        }
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kType, &object, IME::gMslLengthErrorDtorTV);   // the fakes return, and the original runs on
    }
    // The Mac asks for 0x10 bytes; the host node is larger.
    WStringTreeNode* node = static_cast<WStringTreeNode*>(IME::MslNew(sizeof(WStringTreeNode)));
    // placement new (0x10201850) of the string at node + 0xc, then basic_string(key, 0, npos)
    IME::MslWideString_construct(reinterpret_cast<IME::MslWideString*>(&node->mString), key, 0, 0xffffffffu);
    node->mRight = nullptr;
    node->mLeft = nullptr;
    node->mParent = reinterpret_cast<uintptr_t>(parent) | (node->mParent & 1);   // the colour bit of the fresh memory survives
    if (left)
        parent->mLeft = node;
    else
        parent->mRight = node;
    mCount = mCount + 1;
    MslRedBlackTree::balance_insert(node, mRoot);
    if (leftmost) std::memcpy(mLess, &node, sizeof(node));
    return node;
}

void WStringTree::erase(node_base* position) {
    node_base* begin;
    std::memcpy(&begin, mLess, sizeof(begin));
    if (position == begin) {
        // begin moves to the in-order successor
        node_base* next = position->mRight;
        if (next != nullptr) {
            while (next->mLeft != nullptr) next = next->mLeft;
        } else {
            node_base* current = position;
            for (;;) {
                node_base* up = reinterpret_cast<node_base*>(current->mParent & ~static_cast<uintptr_t>(1));
                if (current != up->mLeft) {
                    current = up;
                    continue;
                }
                next = up;
                break;
            }
        }
        std::memcpy(mLess, &next, sizeof(next));
    }
    MslRedBlackTree::balance_erase(position, mRoot);
    // (the original tests "&node->string != 0" here, always true)
    ReleaseString(static_cast<WStringTreeNode*>(position));
    IME::MslDelete(position);
    mCount = mCount - 1;
}

void WStringTree::destroy(node_base* nodeBase) {
    WStringTreeNode* node = static_cast<WStringTreeNode*>(nodeBase);
    // a grandchild: its subtrees, its string through the allocator, then the block through the allocator
    auto grandchild = [this](node_base* gb) {
        WStringTreeNode* g = static_cast<WStringTreeNode*>(gb);
        if (g->mLeft != nullptr) destroy(g->mLeft);
        if (g->mRight != nullptr) destroy(g->mRight);
        wstring_alloc::Destroy(this, &g->mString);
        wstring_tree_callee::NodeDeallocate(reinterpret_cast<char*>(this) + 4, g, 1);
    };
    // a child: both grandchildren, then its string destructor and operator delete
    auto child = [&](node_base* cb) {
        WStringTreeNode* c = static_cast<WStringTreeNode*>(cb);
        if (c->mLeft != nullptr) grandchild(c->mLeft);
        if (c->mRight != nullptr) grandchild(c->mRight);
        masterlist_callee::WStringDtor(&c->mString, -1);
        IME::MslDelete(c);
    };
    if (node->mLeft != nullptr) child(node->mLeft);
    if (node->mRight != nullptr) child(node->mRight);
    ReleaseString(node);
    IME::MslDelete(node);
}
