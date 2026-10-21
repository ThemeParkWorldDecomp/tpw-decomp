// Parser.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "CMsg.h"
#include "email.h"
#include "FS_Online.h"
#include "online.h"
#include "ThingReceivers.h"
#include "ui_ime.h"
#include <cstdint>
#include <string>

// cmp_nocase(const std::wstring&, const std::wstring&, bool) 0x10210e68: compares with the characters below 0x100 mapped through
// __wupper_map; with the flag set, the end of the second string alone ends the comparison as equal (a prefix match).
static inline uint16_t UpperOf(uint16_t c) {
    return c < 0x100 ? gWUpperMap[c] : c;
}

static inline const IME::MslWideStringRep* RepOf(const IME::MslWideString& s) {
    // The string object is one pointer to its representation (the header types the word as char16_t*).
    return *reinterpret_cast<const IME::MslWideStringRep* const*>(&s);
}

int32_t cmp_nocase(const IME::MslWideString& a, const IME::MslWideString& b, bool prefix) {
    const IME::MslWideStringRep* ra = RepOf(a);
    const IME::MslWideStringRep* rb = RepOf(b);
    const char16_t* p = ra->mData;    // r30
    const char16_t* q = rb->mData;    // r3
    uint32_t lenA = ra->mLength;
    uint32_t lenB = rb->mLength;
    const char16_t* endA = ra->mData + lenA;
    const char16_t* endB = rb->mData + lenB;
    while (p != endA && q != endB) {
        uint16_t cb = *q;
        uint16_t ca = *p;
        if (UpperOf(ca) != UpperOf(cb)) return UpperOf(ca) < UpperOf(cb) ? -1 : 1;
        ++p;
        ++q;
    }
    if (prefix && q == endB) return 0;
    if (lenB == lenA) return 0;
    return lenA < lenB ? -1 : 1;
}

uint8_t gCParserVtableBlock[16];   // 0x102b1b3c

// 0x10211064
CParser::CParser() {
    char unusedAllocator;
    char lessA, allocA, lessB, allocB;
    (void)unusedAllocator;
    mVtable = gCParserVtableBlock + 8;
    online_callee::WStringCtor(&mString);
    parser_callee::TreeCtor(mSeparators, &lessA, &allocA);
    parser_callee::TreeCtor(mWhitespace, &lessB, &allocB);
    SetupSeparatorSet();
    SetupWhitespaceSet();
}

// Unit v51059: CParser's set helpers and a handful of small library/destructor functions of the online code (main app, addresses in comments).

// The globals the listings read through the TOC (initial values of the Mac image).
uint8_t gWcharSetInserted = 1;      // 0x102b1b54
uint8_t gWcharSetNotInserted = 0;   // 0x102b1b55
uint32_t gWcharSetCountInit = 0;    // 0x102b1b50
uint32_t gTokenListInit = 0;        // 0x102b1b38
// 0x102112b0
void CParser::SetupSeparatorSet() {
    AddSeparator(u' ');
    AddSeparator(u'\n');
    AddSeparator(u'\t');
    AddSeparator(u'\r');
}

// 0x10211344
void CParser::SetupWhitespaceSet() {
    AddWhitespace(u' ');
    AddWhitespace(u'\n');
    AddWhitespace(u'\t');
    AddWhitespace(u'\r');
}

// CParser (the chat line parser's base): SetupForParsing 0x102113dc, Parse 0x102114f8, Match 0x10211630, destructor 0x102111d4,
// SkipWhitespace 0x10211860, SkipWhitespaceAndSeparator 0x1021193c, CountNonWhitespace 0x10211a60, GetString 0x10211b3c,
// GetRemainingString 0x10211d2c.

namespace cparser_callee {
__attribute__((weak)) void StringDtor(void*, int16_t) {}
__attribute__((weak)) void OperatorDelete(void* p) { ::operator delete(p); }
__attribute__((weak)) bool CallHandler(CParser*, const uint32_t*) { return true; }
}  // namespace cparser_callee

namespace {

using IME::MslWideString;
using IME::MslWideStringRep;

inline uint32_t Length(const CParser* p) { return static_cast<const MslWideStringRep*>(p->mString)->mLength; }
inline MslWideString* Str(CParser* p) { return reinterpret_cast<MslWideString*>(&p->mString); }
// The end() of a set's tree is its root word.
inline void* EndOf(void* tree) { return &static_cast<WcharSetTree*>(tree)->mRoot; }
inline bool InSet(CParser* p, uint8_t* tree, uint32_t index) {
    char16_t* c = IME::MslWideString_index(Str(p), index);
    return cparser_callee::TreeFind(tree, c) != EndOf(tree);
}

}  // namespace

// 0x102113dc
void CParser::SetupForParsing(const void* text, void* list, const uint8_t* props) {
    mCaseFlag = props[0];
    mStopFlag = props[1];
    mPrefixFlag = props[2];
    mTokens = list;
    IME::MslWideString_assign(Str(this), static_cast<const MslWideString*>(text), 0, 0xffffffffu);
    mCursor = IME::MslWideString_begin(Str(this));
    mPos = 0;
}

// 0x102114f8
int32_t CParser::Parse() {
    int32_t found = 0;
    while (mPos < Length(this)) {
        SkipWhitespaceAndSeparator();
        int32_t count = CountNonWhitespace();
        alignas(8) uint8_t temp[8];
        IME::MslWideString_substr(reinterpret_cast<MslWideString*>(temp), Str(this), mPos, count);
        CParserToken* token = static_cast<CParserToken*>(Match(temp));
        cparser_callee::StringDtor(temp, -1);
        mPos += count;
        if (token != nullptr) {
            uint32_t handler[3] = {token->mHandler[0], token->mHandler[1], token->mHandler[2]};
            found++;
            if (!cparser_callee::CallHandler(this, handler)) return -1;
        }
        if (mStopFlag != 0) break;
    }
    return found;
}

// 0x10211630
void* CParser::Match(const void* text) {
    CParserTokenList* list = static_cast<CParserTokenList*>(mTokens);
    CParserTokenNode* end = reinterpret_cast<CParserTokenNode*>(&list->mPrev);
    const MslWideString& arg = *static_cast<const MslWideString*>(text);
    for (CParserTokenNode* node = list->mNext; node != end; node = node->mNext) {
        const MslWideString& name = *reinterpret_cast<const MslWideString*>(&node->mToken.mName);
        if (mPrefixFlag != 0) {
            if (cmp_nocase(name, arg, true) == 0) return &node->mToken;
        } else if (mCaseFlag != 0) {
            const MslWideStringRep* argRep = *reinterpret_cast<MslWideStringRep* const*>(text);
            const MslWideStringRep* nameRep = static_cast<const MslWideStringRep*>(node->mToken.mName);
            if (IME::MslWideString_compare(&name, 0, nameRep->mLength, argRep->mData, argRep->mLength) == 0) return &node->mToken;
        } else {
            if (cmp_nocase(name, arg, false) == 0) return &node->mToken;
        }
    }
    return nullptr;
}

// 0x10211798: the set at Mac +0x14 (the field the header calls mWhitespace).
void CParser::AddSeparator(char16_t c) {
    v51059::WcharInsertResult result;
    v51059::TreeInsertOne(&result, reinterpret_cast<WcharSetTree*>(mWhitespace), &c);
}

// 0x102117fc: the set at Mac +8 (the field the header calls mSeparators).
void CParser::AddWhitespace(char16_t c) {
    v51059::WcharInsertResult result;
    v51059::TreeInsertOne(&result, reinterpret_cast<WcharSetTree*>(mSeparators), &c);
}

// 0x102111d4 (the delete flag is the hidden short after this)
CParser* CParser::Destruct(CParser* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtable = gCParserVtableBlock + 8;
        WcharSetTree* whitespace = reinterpret_cast<WcharSetTree*>(self->mWhitespace);
        if (whitespace->mRoot != nullptr) cparser_callee::TreeDestroy(whitespace, whitespace->mRoot);
        WcharSetTree* separators = reinterpret_cast<WcharSetTree*>(self->mSeparators);
        if (separators->mRoot != nullptr) cparser_callee::TreeDestroy(separators, separators->mRoot);
        cparser_callee::StringDtor(&self->mString, -1);
        if (deleteFlag > 0) cparser_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10211860
int32_t CParser::SkipWhitespace() {
    int32_t skipped = 0;
    for (;;) {
        if (!InSet(this, mSeparators, mPos)) return skipped;
        skipped++;
        mPos++;
        if (mPos >= Length(this)) return -1;
    }
}

// 0x1021193c
int32_t CParser::SkipWhitespaceAndSeparator() {
    int32_t skipped = 0;
    for (;;) {
        if (!InSet(this, mWhitespace, mPos)) {
            if (!InSet(this, mSeparators, mPos)) return skipped;
        }
        skipped++;
        mPos++;
        if (mPos >= Length(this)) return -1;
    }
}

// 0x10211a60
int32_t CParser::CountNonWhitespace() {
    int32_t count = 0;
    for (;;) {
        if (InSet(this, mSeparators, mPos + count)) return count;
        count++;
        if (mPos + count >= Length(this)) return count;
    }
}

// 0x10211b3c
bool CParser::GetString(void* out) {
    for (;;) {
        if (!InSet(this, mSeparators, mPos)) break;
        mPos++;
        if (mPos >= Length(this)) break;
    }
    int32_t count = 0;
    for (;;) {
        if (InSet(this, mWhitespace, mPos + count)) break;
        if (mPos + count >= Length(this)) break;
        count++;
    }
    if (mPos + count > Length(this)) return false;
    alignas(8) uint8_t temp[8];
    IME::MslWideString_substr(reinterpret_cast<MslWideString*>(temp), Str(this), mPos, count);
    IME::MslWideString_assign(static_cast<MslWideString*>(out), reinterpret_cast<MslWideString*>(temp), 0, 0xffffffffu);
    cparser_callee::StringDtor(temp, -1);
    mPos += count;
    return true;
}

// 0x10211d2c
bool CParser::GetRemainingString(void* out, int32_t* count) {
    for (;;) {
        if (!InSet(this, mSeparators, mPos)) break;
        mPos++;
        if (mPos >= Length(this)) break;
    }
    if (count != nullptr) *count = static_cast<int32_t>(Length(this) - mPos);
    alignas(8) uint8_t temp[8];
    IME::MslWideString_substr(reinterpret_cast<MslWideString*>(temp), Str(this), mPos, Length(this) - mPos);
    IME::MslWideString_assign(static_cast<MslWideString*>(out), reinterpret_cast<MslWideString*>(temp), 0, 0xffffffffu);
    cparser_callee::StringDtor(temp, -1);
    mPos = Length(this);
    return true;
}

// 0x10211eb0: the position becomes the first word of the string's block (its length), the answer is true.
bool CParser::IgnoreRemainingString() {
    mPos = *static_cast<const uint32_t*>(mString);
    return true;
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

namespace std_destroy {

// 0x10210944
void DestroyRangeClear(Argument* first, Argument* last) {
    for (; first < last; ++first) Argument::Destruct(first, -1);
}

}  // namespace std_destroy
