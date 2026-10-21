// Test library for CParser's nine functions (unit v5911; libtpw_unit_CParser_Parse.dylib, tests/test_CParser_Parse.py), main app:
//   src/game/online/Parser.cpp
//
// The callees outside the unit are recording fakes: the string library (operator[], substr, assign, begin, compare), cmp_nocase, the
// set's find and destroy, the string destructor, operator delete and the pointer-to-member glue. The fakes work on real string
// representations built by the test (one pointer to { length, capacity, refs, data }), so the unit reads them as the original does.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"

using namespace calllog;

uint8_t gCParserVtableBlock[16];

namespace {

CParser* gParser = nullptr;

using IME::MslWideStringRep;

MslWideStringRep* RepOf(const void* stringObject) { return *static_cast<MslWideStringRep* const*>(stringObject); }

uint32_t Hash(const char16_t* p, uint32_t n) {
    uint32_t h = 0;
    for (uint32_t i = 0; i < n; i++) h = h * 31u + p[i];
    return h;
}

uint32_t HashOf(const void* stringObject) {
    const MslWideStringRep* rep = RepOf(stringObject);
    return Hash(rep->mData, rep->mLength);
}

MslWideStringRep* NewRep(const char16_t* chars, uint32_t n) {
    MslWideStringRep* rep = static_cast<MslWideStringRep*>(tpw::ArenaAllocZeroed(sizeof(MslWideStringRep)));
    char16_t* data = static_cast<char16_t*>(tpw::ArenaAllocZeroed(2 * n + 8));
    for (uint32_t i = 0; i < n; i++) data[i] = chars[i];
    rep->mLength = n;
    rep->mCapacity = n;
    rep->mData = data;
    return rep;
}

uint32_t TreeOffset(const void* tree) {
    if (tree == static_cast<const void*>(gParser->mSeparators)) return 8;
    if (tree == static_cast<const void*>(gParser->mWhitespace)) return 0x14;
    return 0xffff;
}

}  // namespace

namespace cparser_callee {
void* TreeFind(void* tree, const char16_t* key) {
    uint32_t off = TreeOffset(tree);
    ev("find", {off, static_cast<uint32_t>(*key)});
    int64_t answer = ret("find", static_cast<int64_t>(off) * 65536 + *key, 0);
    return answer == 0 ? static_cast<void*>(&static_cast<WcharSetTree*>(tree)->mRoot) : static_cast<void*>(static_cast<char*>(tree) + 0x10);
}
void TreeDestroy(void* tree, void* root) {
    ev("treedestroy", {TreeOffset(tree), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(root))});
}
void StringDtor(void* string, int16_t flag) {
    uint32_t which = string == static_cast<void*>(&gParser->mString) ? 4 : 99;
    ev("strdtor", {which, static_cast<uint32_t>(static_cast<int32_t>(flag)), HashOf(string)});
}
void OperatorDelete(void* p) { ev("delete", {p == static_cast<void*>(gParser) ? 1 : 0}); }
bool CallHandler(CParser* self, const uint32_t* handler) {
    ev("handler", {self == gParser ? 0 : 1, handler[0], handler[1], handler[2]});
    return (ret("handler") & 0xff) != 0;
}
}  // namespace cparser_callee

namespace IME {
char16_t* MslWideString_index(MslWideString* self, uint32_t index) {
    ev("index", {index});
    return RepOf(self)->mData + index;
}
MslWideString* MslWideString_substr(MslWideString* result, const MslWideString* self, uint32_t pos, uint32_t n) {
    ev("substr", {pos, n});
    const MslWideStringRep* rep = RepOf(self);
    uint32_t avail = pos <= rep->mLength ? rep->mLength - pos : 0;
    uint32_t count = n < avail ? n : avail;
    *reinterpret_cast<MslWideStringRep**>(result) = NewRep(rep->mData + (count ? pos : 0), count);
    return result;
}
MslWideString* MslWideString_assign(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    ev("assign", {pos, n, HashOf(str)});
    const MslWideStringRep* rep = RepOf(str);
    uint32_t avail = pos <= rep->mLength ? rep->mLength - pos : 0;
    uint32_t count = n < avail ? n : avail;
    *reinterpret_cast<MslWideStringRep**>(self) = NewRep(rep->mData + (count ? pos : 0), count);
    return self;
}
char16_t* MslWideString_begin(MslWideString* self) {
    ev("begin", {});
    return RepOf(self)->mData + (ret("begin") & 3);
}
int32_t MslWideString_compare(const MslWideString* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    ev("compare", {pos, n1, n2, Hash(s, n2), HashOf(self)});
    return static_cast<int32_t>(ret("compare"));
}
}  // namespace IME

int32_t cmp_nocase(const IME::MslWideString& a, const IME::MslWideString& b, bool prefix) {
    ev("cmp_nocase", {HashOf(&a), HashOf(&b), prefix ? 1 : 0});
    return static_cast<int32_t>(ret("cmp_nocase"));
}

extern "C" {

uint64_t tpw_cp_addr(const char* name) {
    if (std::strcmp(name, "vtbl") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gCParserVtableBlock + 8));
    return 0;
}

uint64_t tpw_cp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParser, mString));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mSeparators));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mWhitespace));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mCursor));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mPos));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mCaseFlag));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mStopFlag));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mPrefixFlag));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mTokens));
    out[n++] = static_cast<int64_t>(sizeof(CParser));
    out[n++] = static_cast<int64_t>(offsetof(CParserTokenNode, mNext));
    out[n++] = static_cast<int64_t>(offsetof(CParserTokenNode, mToken));
    out[n++] = static_cast<int64_t>(sizeof(CParserTokenNode));
    out[n++] = static_cast<int64_t>(offsetof(CParserToken, mHandler));
    out[n++] = static_cast<int64_t>(offsetof(CParserTokenList, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(CParserTokenList, mNext));
    out[n++] = static_cast<int64_t>(sizeof(CParserTokenList));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree, mRoot));
    out[n++] = static_cast<int64_t>(sizeof(MslWideStringRep));
    out[n++] = static_cast<int64_t>(offsetof(MslWideStringRep, mData));
    return n;
}

#define PARSER(p) (gParser = reinterpret_cast<CParser*>(static_cast<uintptr_t>(p)))

void tpw_cp_setup(uint64_t parser, uint64_t text, uint64_t list, uint64_t props) {
    PARSER(parser)->SetupForParsing(reinterpret_cast<const void*>(static_cast<uintptr_t>(text)),
                                    reinterpret_cast<void*>(static_cast<uintptr_t>(list)),
                                    reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(props)));
}
int32_t tpw_cp_parse(uint64_t parser) { return PARSER(parser)->Parse(); }
uint64_t tpw_cp_match(uint64_t parser, uint64_t text) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(PARSER(parser)->Match(reinterpret_cast<const void*>(static_cast<uintptr_t>(text)))));
}
uint64_t tpw_cp_dtor(uint64_t parser, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(CParser::Destruct(PARSER(parser), static_cast<int16_t>(flag))));
}
int32_t tpw_cp_skipws(uint64_t parser) { return PARSER(parser)->SkipWhitespace(); }
int32_t tpw_cp_skipwssep(uint64_t parser) { return PARSER(parser)->SkipWhitespaceAndSeparator(); }
int32_t tpw_cp_count(uint64_t parser) { return PARSER(parser)->CountNonWhitespace(); }
uint32_t tpw_cp_getstring(uint64_t parser, uint64_t out) {
    return PARSER(parser)->GetString(reinterpret_cast<void*>(static_cast<uintptr_t>(out)));
}
uint32_t tpw_cp_getremaining(uint64_t parser, uint64_t out, uint64_t count) {
    return PARSER(parser)->GetRemainingString(reinterpret_cast<void*>(static_cast<uintptr_t>(out)),
                                              reinterpret_cast<int32_t*>(static_cast<uintptr_t>(count)));
}

}  // extern "C"
