// Test library for the COnlineParser Parse functions (unit v5051; libtpw_unit_COnlineParser_ParseGoto.dylib, tests/test_COnlineParser_ParseGoto.py), main app:
//   src/game/online/OnlineParser.cpp   ParseBlackMark, ParseBuddy, ParsePage, ParseGoto, ParseLocate
//
// The callees (string constructors, Argument, vector push_back, CParser::GetString and IgnoreRemainingString) are fakes recording what they read.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace online_callee {

static uint32_t& Word0(void* p) { return *static_cast<uint32_t*>(p); }

void StringCtor(void* self) { ev("StringCtor", {}); Word0(self) = 0xC0DE0001u; }
void WStringCtor(void* self) { ev("WStringCtor", {}); Word0(self) = 0xC0DE0002u; }
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {static_cast<int64_t>(Word0(self)), flag}); }
void StringDtor(void* self, int16_t flag) { ev("StringDtor", {static_cast<int64_t>(Word0(self)), flag}); }
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    const uint32_t value = *static_cast<const uint32_t*>(other);
    ev("WStringCopy", {static_cast<int64_t>(value), static_cast<int64_t>(pos), static_cast<int64_t>(count)});
    Word0(self) = value ^ 0x1234u;
}
void ArgumentCtor(void* self, const void* text) {
    const uint32_t value = *static_cast<const uint32_t*>(text);
    ev("ArgumentCtor", {static_cast<int64_t>(value)});
    uint32_t* words = static_cast<uint32_t*>(self);
    words[0] = 0xAC01u;
    words[1] = value;
    words[2] = 0xAC02u;
}
void ArgumentVectorPushBack(void* vec, const void* argument) {
    const uint32_t* v = static_cast<const uint32_t*>(vec);
    const uint32_t* a = static_cast<const uint32_t*>(argument);
    ev("PushBack", {static_cast<int64_t>(v[0]), static_cast<int64_t>(a[0]), static_cast<int64_t>(a[1]), static_cast<int64_t>(a[2])});
}
bool ParserGetString(void* parser, void* wstring) {
    (void)parser;
    const bool ok = ret("GetString") != 0;
    const uint32_t value = static_cast<uint32_t>(ret("Word"));
    ev("GetString", {ok ? 1 : 0});
    Word0(wstring) = value;
    return ok;
}
bool ParserIgnoreRemainingString(void* parser) {
    (void)parser;
    ev("Ignore", {});
    return (ret("Ignore") & 0xff) != 0;
}

}  // namespace online_callee

extern "C" {

uint64_t tpw_parse_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_parse_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mArguments));
    return n;
}

uint32_t tpw_parse_BlackMark(uint64_t p) { return reinterpret_cast<COnlineParser*>(static_cast<uintptr_t>(p))->ParseBlackMark(); }
uint32_t tpw_parse_Buddy(uint64_t p) { return reinterpret_cast<COnlineParser*>(static_cast<uintptr_t>(p))->ParseBuddy(); }
uint32_t tpw_parse_Page(uint64_t p) { return reinterpret_cast<COnlineParser*>(static_cast<uintptr_t>(p))->ParsePage(); }
uint32_t tpw_parse_Goto(uint64_t p) { return reinterpret_cast<COnlineParser*>(static_cast<uintptr_t>(p))->ParseGoto(); }
uint32_t tpw_parse_Locate(uint64_t p) { return reinterpret_cast<COnlineParser*>(static_cast<uintptr_t>(p))->ParseLocate(); }

}  // extern "C"
