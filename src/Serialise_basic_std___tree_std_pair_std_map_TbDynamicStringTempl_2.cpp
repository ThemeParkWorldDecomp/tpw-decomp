// ParkThemeTree::destroy (0x1016d784, 496 B) and insert_node_at (0x1016eb98, 400 B): the theme map's tree.
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "analyser.h"
#include "FS_MasterSave.h"
#include "ui_ime.h"

using namespace ParkThemeTreeCallee;

namespace {

using node = ParkThemeTree::node;

// The original inlines two levels of the recursion: a grandchild is taken down with the allocator's destroy and deallocate, a child
// with the pair's destructor and operator delete.
void DropInner(ParkThemeTree* tree, node* g) {
    if (g->mLeft != nullptr) tree->destroy(g->mLeft);
    if (g->mRight != nullptr) tree->destroy(g->mRight);
    PairDestroy(tree, &g->mValue);
    NodeDeallocate(tree, g, 1);
}

void DropMid(ParkThemeTree* tree, node* m) {
    node* g = m->mLeft;
    if (g != nullptr) DropInner(tree, g);
    g = m->mRight;
    if (g != nullptr) DropInner(tree, g);
    PairDtor(&m->mValue, -1);
    IME::MslDelete(m);
}

}  // namespace

void ParkThemeTree::destroy(node* subtree) {
    node* child = subtree->mLeft;
    if (child != nullptr) DropMid(this, child);
    child = subtree->mRight;
    if (child != nullptr) DropMid(this, child);
    subtree->mValue.first.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();   // the key, then the node itself
    IME::MslDelete(subtree);
}

ParkThemeTree::node* ParkThemeTree::insert_node_at(node* parent, bool left, bool leftmost, const Pair& value) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {   // the original goes on when the (faked) throw returns
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        char* copy = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, copy);
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node* made = static_cast<node*>(IME::MslNew(sizeof(node)));   // 0x24 on the Mac
    if (made != nullptr) PairCopy(&made->mValue, value);
    made->mLeft = nullptr;
    made->mRight = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);   // the new block's old colour bit survives
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}
