// Test library for the COnlineParser constructor, destructor and four simple commands (unit v5131;
// libtpw_unit_COnlineParser_COnlineParser.dylib, tests/test_COnlineParser_COnlineParser.py), main app:
//   src/COnlineParser_ParseGoto_COnlineParser_COnlineParser.cpp
//
// Every callee is a fake recording its pointers as (range tag * 0x10000 + offset), the module registers the ranges.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace {

struct Range {
    uintptr_t base;
    uintptr_t size;
    int tag;
};
Range gRanges[16];
int gRangeCount = 0;

void Reg(int tag, uintptr_t base, uintptr_t size) {
    for (int i = 0; i < gRangeCount; i++)
        if (gRanges[i].tag == tag) {
            gRanges[i] = {base, size, tag};
            return;
        }
    gRanges[gRangeCount++] = {base, size, tag};
}

int64_t P(const void* p) {
    uintptr_t a = reinterpret_cast<uintptr_t>(p);
    for (int i = 0; i < gRangeCount; i++)
        if (a >= gRanges[i].base && a < gRanges[i].base + gRanges[i].size)
            return static_cast<int64_t>(gRanges[i].tag) * 0x10000 + static_cast<int64_t>(a - gRanges[i].base);
    return -1;
}

}  // namespace

namespace online_callee {
void StringCtor(void* self) {
    Reg(5, reinterpret_cast<uintptr_t>(self), 12);
    ev("StringCtor", {P(self)});
}
void WStringCtor(void* self) { ev("WStringCtor", {P(self)}); }
void StringDtor(void* self, int16_t flag) { ev("StringDtor", {P(self), static_cast<int64_t>(flag)}); }
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {P(self), static_cast<int64_t>(flag)}); }
void ArgumentVectorPushBack(void* vec, const void* argument) {
    ev("Push", {P(vec), P(argument), static_cast<int64_t>(static_cast<const uint32_t*>(argument)[2])});
}
bool ParserIgnoreRemainingString(void* parser) {
    ev("Ignore", {P(parser)});
    return false;
}
void OperatorDelete(void* p) { ev("delete", {P(p)}); }
void WStringAssign(void* self, const char16_t* text, uint32_t n) {
    ev("Assign", {P(self), P(text), static_cast<int64_t>(n), text[0], text[1], text[2], text[3], text[4], text[5], text[6], text[7]});
}
}  // namespace online_callee

namespace online_parser_callee {
void ParserCtor(void* parser) { ev("ParserCtor", {P(parser)}); }
void ParserDtor(void* parser, int16_t flag) { ev("ParserDtor", {P(parser), static_cast<int64_t>(flag)}); }
void ListCtor(void* list) { ev("ListCtor", {P(list)}); }
void ListClear(void* list) { ev("ListClear", {P(list)}); }
void ArgumentVectorClear(void* vec) { ev("VecClear", {P(vec)}); }
uint32_t WcsLen(const char16_t* text) {
    uint32_t n = 0;
    while (n < 64 && text[n] != 0) n++;
    ev("wcslen", {P(text), static_cast<int64_t>(n)});
    return n;
}
void InitTokenList(void* parser) { ev("InitTokenList", {P(parser)}); }
}  // namespace online_parser_callee

extern "C" {

uint64_t tpw_oparser_addr(const char* name) {
    if (std::strcmp(name, "text") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gOnlineParserInitialText));
    return 0;
}

uint64_t tpw_oparser_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_oparser_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mArguments));
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mText));
    out[n++] = static_cast<int64_t>(sizeof(gOnlineParserInitialText));
    return n;
}

void tpw_oparser_reset() { gRangeCount = 0; }
void tpw_oparser_reg(int32_t tag, uint64_t base, uint64_t size) { Reg(tag, static_cast<uintptr_t>(base), static_cast<uintptr_t>(size)); }
int64_t tpw_oparser_p(uint64_t p) { return P(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

uint64_t tpw_oparser_ctor(uint64_t self, uint64_t vec, uint64_t text) {
    return reinterpret_cast<uintptr_t>(COnlineParser::Construct(reinterpret_cast<COnlineParser*>(self), reinterpret_cast<void*>(vec),
                                                                reinterpret_cast<void*>(text)));
}
uint64_t tpw_oparser_dtor(uint64_t self, int16_t flag) {
    return reinterpret_cast<uintptr_t>(COnlineParser::Destruct(reinterpret_cast<COnlineParser*>(self), flag));
}
void tpw_oparser_earmuffs(uint64_t self) { reinterpret_cast<COnlineParser*>(self)->ParseEarmuffs(); }
void tpw_oparser_afk(uint64_t self) { reinterpret_cast<COnlineParser*>(self)->ParseAfk(); }
void tpw_oparser_vote(uint64_t self) { reinterpret_cast<COnlineParser*>(self)->ParseVote(); }
void tpw_oparser_filter(uint64_t self) { reinterpret_cast<COnlineParser*>(self)->ParseFilter(); }

}  // extern "C"
