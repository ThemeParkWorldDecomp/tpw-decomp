// CParser (the chat line parser's base): SetupForParsing 0x102113dc, Parse 0x102114f8, Match 0x10211630, destructor 0x102111d4,
// SkipWhitespace 0x10211860, SkipWhitespaceAndSeparator 0x1021193c, CountNonWhitespace 0x10211a60, GetString 0x10211b3c,
// GetRemainingString 0x10211d2c.
#include <cstdint>
#include <string>
#include "online.h"
#include "ui_ime.h"

namespace cparser_callee {
// Weak stand-ins (the set and string library are not translated); the test library replaces them with recording fakes.
__attribute__((weak)) void* TreeFind(void* tree, const char16_t*) { return static_cast<char*>(tree) + 8; }
__attribute__((weak)) void TreeDestroy(void*, void*) {}
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
