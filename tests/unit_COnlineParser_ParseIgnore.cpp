// Test library for COnlineParser::ParseIgnore and DoParse (unit v5555), main app:
//   src/game/online/OnlineParser.cpp
// Callees are recording fakes; wide strings are opaque 32-bit words.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace online_callee {
void StringCtor(void* self) {
    ev("StringCtor", {});
    *static_cast<uint32_t*>(self) = 0x11110000u;
}
void WStringCtor(void* self) {
    ev("WStringCtor", {});
    *static_cast<uint32_t*>(self) = static_cast<uint32_t>(ret("wctor"));
}
void WStringDtor(void* self, int16_t flag) {
    ev("WStringDtor", {static_cast<int64_t>(*static_cast<uint32_t*>(self)), flag});
}
void StringDtor(void* self, int16_t flag) {
    ev("StringDtor", {static_cast<int64_t>(*static_cast<uint32_t*>(self)), flag});
}
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    uint32_t w = *static_cast<const uint32_t*>(other);
    ev("WStringCopy", {static_cast<int64_t>(w), pos, count});
    *static_cast<uint32_t*>(self) = w + 0x100;
}
void ArgumentCtor(void* self, const void* text) {
    uint32_t w = *static_cast<const uint32_t*>(text);
    ev("ArgumentCtor", {static_cast<int64_t>(w)});
    uint32_t* a = static_cast<uint32_t*>(self);
    a[0] = 0x22220000u;
    a[1] = w + 0x1000;
    a[2] = 0x77;
}
void ArgumentVectorPushBack(void* vec, const void* argument) {
    const uint32_t* a = static_cast<const uint32_t*>(argument);
    ev("PushBack", {lab(vec), static_cast<int64_t>(a[0]), static_cast<int64_t>(a[1]), static_cast<int64_t>(a[2])});
}
bool ParserGetString(void* parser, void* wstring) {
    ev("GetString", {lab(parser), static_cast<int64_t>(*static_cast<uint32_t*>(wstring))});
    *static_cast<uint32_t*>(wstring) = static_cast<uint32_t>(ret("gsval"));
    return ret("GetString") != 0;
}
bool ParserIgnoreRemainingString(void* parser) {
    ev("IgnoreRemaining", {lab(parser)});
    return ret("IgnoreRemaining") != 0;
}
}  // namespace online_callee

namespace online_parse_callee {
void ParserSetup(void* parser, const void* text, void* list, const uint8_t* props) {
    ev("Setup", {lab(parser), static_cast<int64_t>(*static_cast<const uint32_t*>(text)), lab(list), props[0], props[1], props[2]});
}
void ParserSkipWhitespace(void* parser) { ev("SkipWhitespace", {lab(parser)}); }
bool ParserGetChar(void* parser, char16_t* ch) {
    ev("GetChar", {lab(parser)});
    *ch = static_cast<char16_t>(ret("ch"));
    return ret("GetChar") != 0;
}
int32_t ParserParse(void* parser) {
    ev("Parse", {lab(parser)});
    return static_cast<int32_t>(ret("Parse"));
}
void WStringAssignRange(void* self, const void* src, uint32_t pos, uint32_t count) {
    uint32_t w = *static_cast<const uint32_t*>(src);
    ev("Assign", {lab(self), static_cast<int64_t>(w), pos, count});
    *static_cast<uint32_t*>(self) = w + 0x55;
}
}  // namespace online_parse_callee

extern "C" {

uint64_t tpw_pi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// offsets the module writes by host layout: mArguments, mText, mF00
int32_t tpw_pi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mArguments));
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mText));
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mF00));
    return n;
}

uint32_t tpw_pi_ParseIgnore(uint64_t self) {
    return reinterpret_cast<COnlineParser*>(self)->ParseIgnore() ? 1u : 0u;
}
void tpw_pi_DoParse(uint64_t self, uint64_t text) {
    reinterpret_cast<COnlineParser*>(self)->DoParse(reinterpret_cast<const void*>(text));
}

}  // extern "C"
