// Test library for COnlineParser's token list and the Say and Tell commands (unit v5187; libtpw_unit_COnlineParser_InitToken.dylib,
// tests/test_COnlineParser_InitToken.py), main app:
//   src/game/online/OnlineParser.cpp   InitToken, InitTokenList, ParseSay, ParseTell
//
// The callees outside the unit (string and Argument constructors, the vector, the list insert, CToken, the parser's GetString and
// GetRemainingString, the table's FormatString and the converter's ConvertString) are recording fakes. gText and the handler table are
// defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "online.h"

using namespace calllog;

// gText: zeroed storage under the symbol the program uses (CText's constructor needs font code this library does not compile).
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
ParserHandler gOnlineTokenHandlers[36];

namespace {
int64_t I(int64_t v) { return v; }
}  // namespace

// ---- the fakes
namespace online_callee {
void StringCtor(void* self) { ev("StringCtor", {I(newlab(self))}); }
void StringDtor(void* self, int16_t flag) { ev("StringDtor", {I(lab(self)), I(flag)}); }
void WStringCtor(void* self) { ev("WStringCtor", {I(newlab(self))}); }
void WStringCtorFrom(void* self, const char16_t* text) {
    ev("WStringCtorFrom", {I(newlab(self))});
    ev("WStringText", {I(text[0]), I(text[0] ? text[1] : 0), I(text[0] && text[1] ? text[2] : 0), I(text[0] && text[1] && text[2] ? text[3] : 0),
                       I(text[0] && text[1] && text[2] && text[3] ? text[4] : 0),
                       I(text[0] && text[1] && text[2] && text[3] && text[4] ? text[5] : 0),
                       I(text[0] && text[1] && text[2] && text[3] && text[4] && text[5] ? text[6] : 0),
                       I(text[0] && text[1] && text[2] && text[3] && text[4] && text[5] && text[6] ? text[7] : 0),
                       I(text[0] && text[1] && text[2] && text[3] && text[4] && text[5] && text[6] && text[7] ? text[8] : 0),
                       I(text[0] && text[1] && text[2] && text[3] && text[4] && text[5] && text[6] && text[7] && text[8] ? text[9] : 0)});
}
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {I(lab(self)), I(flag)}); }
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {I(newlab(self)), I(lab(other)), I(pos), I(static_cast<int32_t>(count))});
}
void ArgumentCtor(void* self, const void* text) { ev("ArgumentCtor", {I(newlab(self)), I(lab(text))}); }
void ArgumentVectorPushBack(void* vec, const void* argument) { ev("PushBack", {I(lab(vec)), I(lab(argument))}); }
bool ParserGetString(void* parser, void* wstring) {
    ev("ParserGetString", {I(lab(parser)), I(lab(wstring))});
    return ret("ParserGetString") != 0;
}
}  // namespace online_callee

namespace token_callee {
void CTokenCtor(void* self, const void* name, uint32_t w0, uint32_t w1, uint32_t w2) {
    ev("CTokenCtor", {I(newlab(self)), I(lab(name)), I(w0), I(w1), I(w2)});
}
bool ParserGetRemainingString(void* parser, void* wstring, int32_t* count) {
    ev("ParserGetRemainingString", {I(lab(parser)), I(lab(wstring)), I(count == nullptr ? 0 : 1)});
    return ret("ParserGetRemainingString") != 0;
}
}  // namespace token_callee

namespace token_list {
void TokenList_insert(TokenListNode** result, TokenList* list, TokenListNode* position, const CToken* token) {
    (void)result;
    ev("ListInsert", {I(lab(reinterpret_cast<uint8_t*>(list) - 0x30)), I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(position))),
                      I(lab(token))});
}
}  // namespace token_list

int32_t TbITable::FormatString(unsigned char* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {I(lab(this)), I(size), I(id)});
    char text[32];
    std::snprintf(text, sizeof text, "n%d", static_cast<int>(ret("fmt")));
    std::memcpy(out, text, std::strlen(text) + 1);
    return 0;
}

TbError TbIMBToUnicode::ConvertString(char16_t* out, const unsigned char* source, uint16_t size) {
    int64_t bytes[10];
    bool ended = false;   // only the string itself: the bytes after its NUL are stale stack
    for (int i = 0; i < 10; ++i) {
        ended = ended || source[i] == 0;
        bytes[i] = ended ? 0 : source[i];
    }
    ev("ConvertString", {I(lab(this)), I(size), bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8],
                         bytes[9]});
    char text[32];
    std::snprintf(text, sizeof text, "w%d", static_cast<int>(ret("conv")));
    size_t n = std::strlen(text);
    for (size_t i = 0; i <= n; ++i) out[i] = static_cast<char16_t>(text[i]);
    return TbError(0);
}

extern "C" {

uint64_t tpw_ci_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"text", &gText},
        {"handlers", gOnlineTokenHandlers},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ci_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The converter and the table objects of the case (arena blocks, or 0 for none) go into gText's resource pointers.
void tpw_ci_setup(uint64_t converter, uint64_t table) {
    gText.mResourcePointers[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(converter));
    gText.mResourcePointers[19] = reinterpret_cast<void*>(static_cast<uintptr_t>(table));
}

int32_t tpw_ci_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mArguments));
    out[n++] = static_cast<int64_t>(offsetof(COnlineParser, mText));
    out[n++] = static_cast<int64_t>(sizeof(ParserHandler));
    out[n++] = static_cast<int64_t>(offsetof(CText, mResourcePointers));
    return n;
}

static COnlineParser* P(uint64_t p) { return reinterpret_cast<COnlineParser*>(static_cast<uintptr_t>(p)); }

void tpw_ci_InitToken(uint64_t parser, uint32_t id, uint32_t w0, uint32_t w1, uint32_t w2) {
    P(parser)->InitToken(id, ParserHandler{{w0, w1, w2}});
}
void tpw_ci_InitTokenList(uint64_t parser) { P(parser)->InitTokenList(); }
uint32_t tpw_ci_ParseSay(uint64_t parser) { return P(parser)->ParseSay() ? 1u : 0u; }
uint32_t tpw_ci_ParseTell(uint64_t parser) { return P(parser)->ParseTell() ? 1u : 0u; }

}  // extern "C"
