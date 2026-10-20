// std::__tree<nocase_string, std::less<nocase_string>, std::allocator<nocase_string>> (a std::set<nocase_string>):
//   erase 0x1022419c, destroy 0x10223e64, insert_node_at 0x10228458.
// The Mac tree is count +0, root +4 (the end node's left word), begin +8; a node is left, right, parent|colour and the nocase_string
// (one word: a pointer to the shared representation) at +0xc. The host keeps native pointers (see NocaseSetTree in online.h).
#include "MslSubscriptionTree.h"
#include "online.h"
#include "ui_ime.h"

namespace {

using node = NocaseSetTree::node;

nocase_string* ValueOf(node* n) { return reinterpret_cast<nocase_string*>(&n->mValue); }

// The inlined nocase_string destructor: drops one reference (a count of 0 is read as 1); on the last one frees the data and the rep.
void ReleaseValue(node* n) {
    NocaseRep* rep = n->mValue;
    if (rep->mRefs == 0) {
        uint32_t refs = rep->mRefs;
        if (refs == 0) refs = 1;
        rep->mRefs = refs;
    }
    uint32_t left = rep->mRefs - 1;
    rep->mRefs = left;
    if (left != 0) rep = nullptr;
    n->mValue = rep;
    if (n->mValue != nullptr) {
        IME::MslDelete(n->mValue->mData);
        IME::MslDelete(n->mValue);
    }
}

// A grandchild, expanded in place by the compiler: its subtrees, then the allocator's destroy and deallocate.
void DestroyInner(NocaseSetTree* tree, node* x) {
    if (x->mLeft != nullptr) tree->destroy(x->mLeft);
    if (x->mRight != nullptr) tree->destroy(x->mRight);
    nocase_callee::AllocDestroy(tree, ValueOf(x));
    nocase_callee::Deallocate(reinterpret_cast<char*>(tree) + 4, x, 1);
}

// A child, expanded in place: its two children as above, then the string destructor and operator delete.
void DestroyChild(NocaseSetTree* tree, node* x) {
    if (x->mLeft != nullptr) DestroyInner(tree, x->mLeft);
    if (x->mRight != nullptr) DestroyInner(tree, x->mRight);
    nocase_callee::StringDtor(ValueOf(x), -1);
    IME::MslDelete(x);
}

}  // namespace

void NocaseSetTree::erase(node* position) {
    if (position == mBegin) {
        node* right = position->mRight;
        node* result;
        if (right == nullptr) {
            node* current = position;
            node* parent;
            bool again;
            do {
                parent = reinterpret_cast<node*>(current->mParent & ~static_cast<uintptr_t>(1));
                again = current != parent->mLeft;
                current = parent;
            } while (again);
            result = parent;
        } else {
            result = right;
            while (result->mLeft != nullptr) result = result->mLeft;
        }
        mBegin = result;
    }
    MslRedBlackTree::balance_erase(reinterpret_cast<MslRedBlackTree::node_base*>(position),
                                   reinterpret_cast<MslRedBlackTree::node_base*>(mRoot));
    ReleaseValue(position);
    IME::MslDelete(position);
    mCount = mCount - 1;
}

void NocaseSetTree::destroy(node* subtree) {
    if (subtree->mLeft != nullptr) DestroyChild(this, subtree->mLeft);
    if (subtree->mRight != nullptr) DestroyChild(this, subtree->mRight);
    ReleaseValue(subtree);
    IME::MslDelete(subtree);
}

NocaseSetTree::node* NocaseSetTree::insert_node_at(node* parent, bool left, bool leftmost, const nocase_string& key) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
            if (object.mRc.mCount != nullptr) *object.mRc.mCount = 1;
        }
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node* made = static_cast<node*>(IME::MslNew(sizeof(node)));   // 0x10 bytes on the Mac
    // The original tests the placement-new result (the address of the value, never NULL) before constructing.
    nocase_callee::StringCopy(ValueOf(made), &key);
    made->mRight = nullptr;
    made->mLeft = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    nocase_callee::BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}
