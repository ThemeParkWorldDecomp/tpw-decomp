// Unit v51059: CParser's set helpers and a handful of small library/destructor functions of the online code (main app, addresses in comments).
#include <cstdint>
#include <string>
#include "online.h"
#include "ui_ime.h"
#include "CMsg.h"
#include "ThingReceivers.h"
#include "email.h"
#include "FS_Online.h"

// The globals the listings read through the TOC (initial values of the Mac image).
uint8_t gWcharSetInserted = 1;      // 0x102b1b54
uint8_t gWcharSetNotInserted = 0;   // 0x102b1b55
uint32_t gWcharSetCountInit = 0;    // 0x102b1b50
uint32_t gTokenListInit = 0;        // 0x102b1b38
uint8_t gIPBaseVtableBlock[16];     // 0x102b1f7c
uint8_t gIPSystemVtableBlock[16];   // 0x102b1938

// 0x10211eb0: the position becomes the first word of the string's block (its length), the answer is true.
bool CParser::IgnoreRemainingString() {
    mPos = *static_cast<const uint32_t*>(mString);
    return true;
}

// 0x102117fc: the set at Mac +8 (the field the header calls mSeparators).
void CParser::AddWhitespace(char16_t c) {
    v51059::WcharInsertResult result;
    v51059::TreeInsertOne(&result, reinterpret_cast<WcharSetTree*>(mSeparators), &c);
}

// 0x10211798: the set at Mac +0x14 (the field the header calls mWhitespace).
void CParser::AddSeparator(char16_t c) {
    v51059::WcharInsertResult result;
    v51059::TreeInsertOne(&result, reinterpret_cast<WcharSetTree*>(mWhitespace), &c);
}

// 0x102112b0
void CParser::SetupSeparatorSet() {
    AddSeparator(u' ');
    AddSeparator(u'\n');
    AddSeparator(u'\t');
    AddSeparator(u'\r');
}

namespace cparser_callee {

// 0x10212060: the node holding the key, or the end node (the root word itself) when there is none.
void* TreeFind(void* treeBlock, const char16_t* key) {
    WcharSetTree* tree = static_cast<WcharSetTree*>(treeBlock);
    WcharSetTree::node* end = reinterpret_cast<WcharSetTree::node*>(&tree->mRoot);
    WcharSetTree::node* result = end;
    WcharSetTree::node* current = tree->mRoot;
    while (current) {
        if (current->mValue < *key) {
            current = current->mRight;
        } else {
            result = current;
            current = current->mLeft;
        }
    }
    if (result == end || *key < result->mValue) return end;
    return result;
}

}  // namespace cparser_callee

namespace v51059 {

// 0x10212854: the allocator hands the node back to operator delete.
void NodeDeallocate(void* allocator, WcharSetTree::node* node, uint32_t count) {
    (void)allocator;
    (void)count;
    cparser_callee::OperatorDelete(node);
}

// 0x10212928
void WcharAllocatorDestroy(void* allocator, char16_t* value) {
    (void)allocator;
    (void)value;
}

namespace {

// The part the compiler inlined for a grandchild: both its subtrees are destroyed by the function itself, then its value and its storage.
void DestroyLeaf(void* tree, WcharSetTree::node* node) {
    if (!node) return;
    if (node->mLeft) cparser_callee::TreeDestroy(tree, node->mLeft);
    if (node->mRight) cparser_callee::TreeDestroy(tree, node->mRight);
    WcharAllocatorDestroy(tree, &node->mValue);
    NodeDeallocate(reinterpret_cast<char*>(tree) + sizeof(void*), node, 1);
}

// The part inlined for a child: its two children as leaves, then operator delete on the child itself.
void DestroyChild(void* tree, WcharSetTree::node* node) {
    if (!node) return;
    DestroyLeaf(tree, node->mLeft);
    DestroyLeaf(tree, node->mRight);
    cparser_callee::OperatorDelete(node);
}

}  // namespace

// 0x10212960: pair<iterator, bool> through the hidden pointer.
void TreeInsertOne(WcharInsertResult* result, WcharSetTree* tree, const char16_t* key) {
    bool left = true;
    bool leftmost = true;
    WcharSetTree::node* notLess = nullptr;
    WcharSetTree::node* parent = reinterpret_cast<WcharSetTree::node*>(&tree->mRoot);
    WcharSetTree::node* current = tree->mRoot;
    while (current) {
        parent = current;
        if (*key < current->mValue) {
            left = true;
            current = current->mLeft;
        } else {
            notLess = current;
            current = current->mRight;
            left = false;
            leftmost = false;
        }
    }
    if (!notLess || notLess->mValue < *key) {
        result->mNode = tree->insert_node_at(parent, left, leftmost, *key);
        result->mInserted = gWcharSetInserted;
    } else {
        result->mNode = notLess;
        result->mInserted = gWcharSetNotInserted;
    }
}

// 0x10211110
WcharSetTree* WcharSetDtor(WcharSetTree* self, int16_t deleteFlag) {
    if (self) {
        if (self->mRoot) cparser_callee::TreeDestroy(self, self->mRoot);
        if (deleteFlag > 0) cparser_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10214348: both vtable stores happen, the second one stays.
void* IPSystemDtor(void* self, int16_t deleteFlag) {
    if (self) {
        void** word = static_cast<void**>(self);
        *word = &gIPBaseVtableBlock[8];
        *word = &gIPSystemVtableBlock[8];
        if (deleteFlag > 0) cparser_callee::OperatorDelete(self);
    }
    return self;
}

}  // namespace v51059

namespace cparser_callee {

// 0x10212604: the compiler unrolled two levels of the recursion.
void TreeDestroy(void* tree, void* root) {
    WcharSetTree::node* node = static_cast<WcharSetTree::node*>(root);
    v51059::DestroyChild(tree, node->mLeft);
    v51059::DestroyChild(tree, node->mRight);
    OperatorDelete(node);
}

}  // namespace cparser_callee

namespace parser_callee {

// 0x10212a88: count from the global word, no root, the first node is the end node (the root word itself). The less functor byte is read
// and overwritten.
void TreeCtor(void* treeBlock, const void* less, const void* allocator) {
    (void)allocator;
    WcharSetTree* tree = static_cast<WcharSetTree*>(treeBlock);
    tree->mCount = gWcharSetCountInit;
    tree->mRoot = nullptr;
    (void)*static_cast<const uint8_t*>(less);
    tree->mBegin = reinterpret_cast<WcharSetTree::node*>(&tree->mRoot);
}

}  // namespace parser_callee

namespace online_parser_callee {

// 0x10210dd4: size from the global word, the sentinel links to itself (the Mac copies two stack words first and overwrites them).
void ListCtor(void* list) {
    CParserTokenList* l = static_cast<CParserTokenList*>(list);
    CParserTokenNode* sentinel = reinterpret_cast<CParserTokenNode*>(&l->mPrev);
    l->mSize = gTokenListInit;
    l->mNext = sentinel;
    l->mPrev = sentinel;
}

}  // namespace online_parser_callee

// 0x10212f58
void COnlinePerson::FinalDestroy() {
    reinterpret_cast<CPeep*>(this)->FinalDestroy();
}

// 0x10212fb0
void COnlinePerson::ReceiveMessage(const CMsg& message) {
    (void)message;
}

// 0x102133e4
void COnlinePerson::ProcessIdle() {}

namespace std_destroy {

// 0x10210944
void DestroyRangeClear(Argument* first, Argument* last) {
    for (; first < last; ++first) Argument::Destruct(first, -1);
}

}  // namespace std_destroy

namespace online_callee {

// 0x1021425c
void NocaseSetDtor(void* self, int16_t flag) {
    if (self) {
        uint32_t* words = static_cast<uint32_t*>(self);
        if (words[1]) chatdata_callee::TreeNocaseDestroy(self, words[1]);
        if (flag > 0) cparser_callee::OperatorDelete(self);
    }
}

// 0x10214148
void BuddyMapDtor(void* self, int16_t flag) {
    if (self) {
        uint32_t* words = static_cast<uint32_t*>(self);
        if (words[1]) chatdata_callee::TreeBuddyDestroy(self, words[1]);
        if (flag > 0) cparser_callee::OperatorDelete(self);
    }
}

// 0x10213fbc
void WStringSetDtor(void* self, int16_t flag) {
    if (self) {
        uint32_t* words = static_cast<uint32_t*>(self);
        if (words[1]) chatdata_callee::TreeWStringDestroy(self, words[1]);
        if (flag > 0) cparser_callee::OperatorDelete(self);
    }
}

}  // namespace online_callee
