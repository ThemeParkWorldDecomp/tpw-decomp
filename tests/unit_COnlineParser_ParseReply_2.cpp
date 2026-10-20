// Test library for COnlineParser::ParseReply, ParseLast, ParseShout, ParseEmote, ParseEmoteTell (unit v5891), main app:
//   src/COnlineParser_ParseGoto_COnlineParser_ParseReply_2.cpp
//
// The callees (string constructors and destructors, Argument, the vector push_back, CParser::GetString and GetRemainingString) are
// recording fakes; the global gOnlineParserInitialText is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

char16_t gOnlineParserInitialText[16];

namespace online_callee {
void StringCtor(void* self) { ev("StringCtor", {lab(self)}); }
void WStringCtor(void* self) { ev("WStringCtor", {lab(self)}); }
void WStringCtorFrom(void* self, const char16_t* text) {
    ev("WStringCtorFrom", {lab(self), text[0], text[1], text[2], text[3]});
}
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {lab(self), flag}); }
void StringDtor(void* self, int16_t flag) { ev("StringDtor", {lab(self), flag}); }
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {lab(self), lab(other), static_cast<int32_t>(pos), static_cast<int32_t>(count)});
}
void ArgumentCtor(void* self, const void* text) {
    ev("ArgumentCtor", {lab(self), lab(text)});
    reinterpret_cast<uint32_t*>(self)[2] = 0;
}
void ArgumentVectorPushBack(void* vec, const void* argument) {
    ev("PushBack", {lab(vec), static_cast<int64_t>(reinterpret_cast<const uint32_t*>(argument)[2])});
}
bool ParserGetString(void* parser, void* wstring) {
    ev("GetString", {lab(parser), lab(wstring)});
    return ret("GetString") != 0;
}
}  // namespace online_callee

namespace token_callee {
bool ParserGetRemainingString(void* parser, void* wstring, int32_t* count) {
    ev("GetRemaining", {lab(parser), lab(wstring), count != nullptr});
    if (count != nullptr) *count = static_cast<int32_t>(ret("count"));
    return ret("GetRemaining") != 0;
}
}  // namespace token_callee

extern "C" {

uint64_t tpw_cop_addr(const char* name) {
    if (std::strcmp(name, "text") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gOnlineParserInitialText));
    return 0;
}

uint64_t tpw_cop_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cop_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mArguments));
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mText));
    return n;
}

uint32_t tpw_cop_reply(uint64_t self) { return reinterpret_cast<COnlineParser*>(self)->ParseReply(); }
uint32_t tpw_cop_last(uint64_t self) { return reinterpret_cast<COnlineParser*>(self)->ParseLast(); }
uint32_t tpw_cop_shout(uint64_t self) { return reinterpret_cast<COnlineParser*>(self)->ParseShout(); }
uint32_t tpw_cop_emote(uint64_t self) { return reinterpret_cast<COnlineParser*>(self)->ParseEmote(); }
uint32_t tpw_cop_emotetell(uint64_t self) { return reinterpret_cast<COnlineParser*>(self)->ParseEmoteTell(); }

}  // extern "C"
